/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Fibre Channel Host Bus Adapters.                                *
 * Copyright (C) 2004-2012 Emulex.  All rights reserved.           *
 * EMULEX and SLI are trademarks of Emulex.                        *
 * www.emulex.com                                                  *
 * Portions Copyright (C) 2004-2005 Christoph Hellwig              *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of version 2 of the GNU General       *
 * Public License as published by the Free Software Foundation.    *
 * This program is distributed in the hope that it will be useful. *
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND          *
 * WARRANTIES, INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,  *
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE      *
 * DISCLAIMED, EXCEPT TO THE EXTENT THAT SUCH DISCLAIMERS ARE HELD *
 * TO BE LEGALLY INVALID.  See the GNU General Public License for  *
 * more details, a copy of which can be found in the file COPYING  *
 * included with this package.                                     *
 *******************************************************************/

#include <linux/blkdev.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/fc/fc_fs.h>
#include <linux/aer.h>

#include "lpfc_hw4.h"
#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_sli4.h"
#include "lpfc_nl.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_crtn.h"
#include "lpfc_logmsg.h"
#include "lpfc_compat.h"
#include "lpfc_debugfs.h"
#include "lpfc_vport.h"

/* There are only four IOCB completion types. */
typedef enum _lpfc_iocb_type {
	LPFC_UNKNOWN_IOCB,
	LPFC_UNSOL_IOCB,
	LPFC_SOL_IOCB,
	LPFC_ABORT_IOCB
} lpfc_iocb_type;


/* Provide function prototypes local to this module. */
static int lpfc_sli_issue_mbox_s4(struct lpfc_hba *, LPFC_MBOXQ_t *,
				  uint32_t);
static int lpfc_sli4_read_rev(struct lpfc_hba *, LPFC_MBOXQ_t *,
			      uint8_t *, uint32_t *);
static struct lpfc_iocbq *lpfc_sli4_els_wcqe_to_rspiocbq(struct lpfc_hba *,
							 struct lpfc_iocbq *);
static void lpfc_sli4_send_seq_to_ulp(struct lpfc_vport *,
				      struct hbq_dmabuf *);
static int lpfc_sli4_fp_handle_wcqe(struct lpfc_hba *, struct lpfc_queue *,
				    struct lpfc_cqe *);
static int lpfc_sli4_post_els_sgl_list(struct lpfc_hba *, struct list_head *,
				       int);
static void lpfc_sli4_hba_handle_eqe(struct lpfc_hba *, struct lpfc_eqe *,
			uint32_t);

static IOCB_t *
lpfc_get_iocb_from_iocbq(struct lpfc_iocbq *iocbq)
{
	return &iocbq->iocb;
}

/**
 * lpfc_sli4_wq_put - Put a Work Queue Entry on an Work Queue
 * @q: The Work Queue to operate on.
 * @wqe: The work Queue Entry to put on the Work queue.
 *
 * This routine will copy the contents of @wqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing the Work Queue Entry. This function returns 0 if
 * successful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static uint32_t
lpfc_sli4_wq_put(struct lpfc_queue *q, union lpfc_wqe *wqe)
{
	union lpfc_wqe *temp_wqe;
	struct lpfc_register doorbell;
	uint32_t host_index;
	uint32_t idx;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return -ENOMEM;
	temp_wqe = q->qe[q->host_index].wqe;

	/* If the host has not yet processed the next entry then we are done */
	idx = ((q->host_index + 1) % q->entry_count);
	if (idx == q->hba_index) {
		q->WQ_overflow++;
		return -ENOMEM;
	}
	q->WQ_posted++;
	/* set consumption flag every once in a while */
	if (!((q->host_index + 1) % q->entry_repost))
		bf_set(wqe_wqec, &wqe->generic.wqe_com, 1);
	else
		bf_set(wqe_wqec, &wqe->generic.wqe_com, 0);
	if (q->phba->sli3_options & LPFC_SLI4_PHWQ_ENABLED)
		bf_set(wqe_wqid, &wqe->generic.wqe_com, q->queue_id);
	lpfc_sli_pcimem_bcopy(wqe, temp_wqe, q->entry_size);
	/* ensure WQE bcopy flushed before doorbell write */
	wmb();

	/* Update the host index before invoking device */
	host_index = q->host_index;

	q->host_index = idx;

	/* Ring Doorbell */
	doorbell.word0 = 0;
	if (q->db_format == LPFC_DB_LIST_FORMAT) {
		bf_set(lpfc_wq_db_list_fm_num_posted, &doorbell, 1);
		bf_set(lpfc_wq_db_list_fm_index, &doorbell, host_index);
		bf_set(lpfc_wq_db_list_fm_id, &doorbell, q->queue_id);
	} else if (q->db_format == LPFC_DB_RING_FORMAT) {
		bf_set(lpfc_wq_db_ring_fm_num_posted, &doorbell, 1);
		bf_set(lpfc_wq_db_ring_fm_id, &doorbell, q->queue_id);
	} else {
		return -EINVAL;
	}
	writel(doorbell.word0, q->db_regaddr);

	return 0;
}

/**
 * lpfc_sli4_wq_release - Updates internal hba index for WQ
 * @q: The Work Queue to operate on.
 * @index: The index to advance the hba index to.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * Work Queue Entries by the HBA. When the HBA indicates that it has consumed
 * an entry the host calls this function to update the queue's internal
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lpfc_sli4_wq_release(struct lpfc_queue *q, uint32_t index)
{
	uint32_t released = 0;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return 0;

	if (q->hba_index == index)
		return 0;
	do {
		q->hba_index = ((q->hba_index + 1) % q->entry_count);
		released++;
	} while (q->hba_index != index);
	return released;
}

/**
 * lpfc_sli4_mq_put - Put a Mailbox Queue Entry on an Mailbox Queue
 * @q: The Mailbox Queue to operate on.
 * @wqe: The Mailbox Queue Entry to put on the Work queue.
 *
 * This routine will copy the contents of @mqe to the next available entry on
 * the @q. This function will then ring the Work Queue Doorbell to signal the
 * HBA to start processing the Work Queue Entry. This function returns 0 if
 * successful. If no entries are available on @q then this function will return
 * -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static uint32_t
lpfc_sli4_mq_put(struct lpfc_queue *q, struct lpfc_mqe *mqe)
{
	struct lpfc_mqe *temp_mqe;
	struct lpfc_register doorbell;
	uint32_t host_index;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return -ENOMEM;
	temp_mqe = q->qe[q->host_index].mqe;

	/* If the host has not yet processed the next entry then we are done */
	if (((q->host_index + 1) % q->entry_count) == q->hba_index)
		return -ENOMEM;
	lpfc_sli_pcimem_bcopy(mqe, temp_mqe, q->entry_size);
	/* Save off the mailbox pointer for completion */
	q->phba->mbox = (MAILBOX_t *)temp_mqe;

	/* Update the host index before invoking device */
	host_index = q->host_index;
	q->host_index = ((q->host_index + 1) % q->entry_count);

	/* Ring Doorbell */
	doorbell.word0 = 0;
	bf_set(lpfc_mq_doorbell_num_posted, &doorbell, 1);
	bf_set(lpfc_mq_doorbell_id, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.MQDBregaddr);
	return 0;
}

/**
 * lpfc_sli4_mq_release - Updates internal hba index for MQ
 * @q: The Mailbox Queue to operate on.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * a Mailbox Queue Entry by the HBA. When the HBA indicates that it has consumed
 * an entry the host calls this function to update the queue's internal
 * pointers. This routine returns the number of entries that were consumed by
 * the HBA.
 **/
static uint32_t
lpfc_sli4_mq_release(struct lpfc_queue *q)
{
	/* sanity check on queue memory */
	if (unlikely(!q))
		return 0;

	/* Clear the mailbox pointer for completion */
	q->phba->mbox = NULL;
	q->hba_index = ((q->hba_index + 1) % q->entry_count);
	return 1;
}

/**
 * lpfc_sli4_eq_get - Gets the next valid EQE from a EQ
 * @q: The Event Queue to get the first valid EQE from
 *
 * This routine will get the first valid Event Queue Entry from @q, update
 * the queue's internal hba index, and return the EQE. If no valid EQEs are in
 * the Queue (no more work to do), or the Queue is full of EQEs that have been
 * processed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfc_eqe *
lpfc_sli4_eq_get(struct lpfc_queue *q)
{
	struct lpfc_eqe *eqe;
	uint32_t idx;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return NULL;
	eqe = q->qe[q->hba_index].eqe;

	/* If the next EQE is not valid then we are done */
	if (!bf_get_le32(lpfc_eqe_valid, eqe))
		return NULL;
	/* If the host has not yet processed the next entry then we are done */
	idx = ((q->hba_index + 1) % q->entry_count);
	if (idx == q->host_index)
		return NULL;

	q->hba_index = idx;
	return eqe;
}

/**
 * lpfc_sli4_eq_clr_intr - Turn off interrupts from this EQ
 * @q: The Event Queue to disable interrupts
 *
 **/
static inline void
lpfc_sli4_eq_clr_intr(struct lpfc_queue *q)
{
	struct lpfc_register doorbell;

	doorbell.word0 = 0;
	bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfc_eqcq_doorbell_eqid_hi, &doorbell,
		(q->queue_id >> LPFC_EQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_eqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
}

/**
 * lpfc_sli4_eq_release - Indicates the host has finished processing an EQ
 * @q: The Event Queue that the host has completed processing for.
 * @arm: Indicates whether the host wants to arms this CQ.
 *
 * This routine will mark all Event Queue Entries on @q, from the last
 * known completed entry to the last entry that was processed, as completed
 * by clearing the valid bit for each completion queue entry. Then it will
 * notify the HBA, by ringing the doorbell, that the EQEs have been processed.
 * The internal host index in the @q will be updated by this routine to indicate
 * that the host has finished processing the entries. The @arm parameter
 * indicates that the queue should be rearmed when ringing the doorbell.
 *
 * This function will return the number of EQEs that were popped.
 **/
uint32_t
lpfc_sli4_eq_release(struct lpfc_queue *q, bool arm)
{
	uint32_t released = 0;
	struct lpfc_eqe *temp_eqe;
	struct lpfc_register doorbell;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return 0;

	/* while there are valid entries */
	while (q->hba_index != q->host_index) {
		temp_eqe = q->qe[q->host_index].eqe;
		bf_set_le32(lpfc_eqe_valid, temp_eqe, 0);
		released++;
		q->host_index = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !arm))
		return 0;

	/* ring doorbell for number popped */
	doorbell.word0 = 0;
	if (arm) {
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
		bf_set(lpfc_eqcq_doorbell_eqci, &doorbell, 1);
	}
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_EVENT);
	bf_set(lpfc_eqcq_doorbell_eqid_hi, &doorbell,
			(q->queue_id >> LPFC_EQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_eqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	/* PCI read to flush PCI pipeline on re-arming for INTx mode */
	if ((q->phba->intr_type == INTx) && (arm == LPFC_QUEUE_REARM))
		readl(q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

/**
 * lpfc_sli4_cq_get - Gets the next valid CQE from a CQ
 * @q: The Completion Queue to get the first valid CQE from
 *
 * This routine will get the first valid Completion Queue Entry from @q, update
 * the queue's internal hba index, and return the CQE. If no valid CQEs are in
 * the Queue (no more work to do), or the Queue is full of CQEs that have been
 * processed, but not popped back to the HBA then this routine will return NULL.
 **/
static struct lpfc_cqe *
lpfc_sli4_cq_get(struct lpfc_queue *q)
{
	struct lpfc_cqe *cqe;
	uint32_t idx;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return NULL;

	/* If the next CQE is not valid then we are done */
	if (!bf_get_le32(lpfc_cqe_valid, q->qe[q->hba_index].cqe))
		return NULL;
	/* If the host has not yet processed the next entry then we are done */
	idx = ((q->hba_index + 1) % q->entry_count);
	if (idx == q->host_index)
		return NULL;

	cqe = q->qe[q->hba_index].cqe;
	q->hba_index = idx;
	return cqe;
}

/**
 * lpfc_sli4_cq_release - Indicates the host has finished processing a CQ
 * @q: The Completion Queue that the host has completed processing for.
 * @arm: Indicates whether the host wants to arms this CQ.
 *
 * This routine will mark all Completion queue entries on @q, from the last
 * known completed entry to the last entry that was processed, as completed
 * by clearing the valid bit for each completion queue entry. Then it will
 * notify the HBA, by ringing the doorbell, that the CQEs have been processed.
 * The internal host index in the @q will be updated by this routine to indicate
 * that the host has finished processing the entries. The @arm parameter
 * indicates that the queue should be rearmed when ringing the doorbell.
 *
 * This function will return the number of CQEs that were released.
 **/
uint32_t
lpfc_sli4_cq_release(struct lpfc_queue *q, bool arm)
{
	uint32_t released = 0;
	struct lpfc_cqe *temp_qe;
	struct lpfc_register doorbell;

	/* sanity check on queue memory */
	if (unlikely(!q))
		return 0;
	/* while there are valid entries */
	while (q->hba_index != q->host_index) {
		temp_qe = q->qe[q->host_index].cqe;
		bf_set_le32(lpfc_cqe_valid, temp_qe, 0);
		released++;
		q->host_index = ((q->host_index + 1) % q->entry_count);
	}
	if (unlikely(released == 0 && !arm))
		return 0;

	/* ring doorbell for number popped */
	doorbell.word0 = 0;
	if (arm)
		bf_set(lpfc_eqcq_doorbell_arm, &doorbell, 1);
	bf_set(lpfc_eqcq_doorbell_num_released, &doorbell, released);
	bf_set(lpfc_eqcq_doorbell_qt, &doorbell, LPFC_QUEUE_TYPE_COMPLETION);
	bf_set(lpfc_eqcq_doorbell_cqid_hi, &doorbell,
			(q->queue_id >> LPFC_CQID_HI_FIELD_SHIFT));
	bf_set(lpfc_eqcq_doorbell_cqid_lo, &doorbell, q->queue_id);
	writel(doorbell.word0, q->phba->sli4_hba.EQCQDBregaddr);
	return released;
}

/**
 * lpfc_sli4_rq_put - Put a Receive Buffer Queue Entry on a Receive Queue
 * @q: The Header Receive Queue to operate on.
 * @wqe: The Receive Queue Entry to put on the Receive queue.
 *
 * This routine will copy the contents of @wqe to the next available entry on
 * the @q. This function will then ring the Receive Queue Doorbell to signal the
 * HBA to start processing the Receive Queue Entry. This function returns the
 * index that the rqe was copied to if successful. If no entries are available
 * on @q then this function will return -ENOMEM.
 * The caller is expected to hold the hbalock when calling this routine.
 **/
static int
lpfc_sli4_rq_put(struct lpfc_queue *hq, struct lpfc_queue *dq,
		 struct lpfc_rqe *hrqe, struct lpfc_rqe *drqe)
{
	struct lpfc_rqe *temp_hrqe;
	struct lpfc_rqe *temp_drqe;
	struct lpfc_register doorbell;
	int put_index;

	/* sanity check on queue memory */
	if (unlikely(!hq) || unlikely(!dq))
		return -ENOMEM;
	put_index = hq->host_index;
	temp_hrqe = hq->qe[hq->host_index].rqe;
	temp_drqe = dq->qe[dq->host_index].rqe;

	if (hq->type != LPFC_HRQ || dq->type != LPFC_DRQ)
		return -EINVAL;
	if (hq->host_index != dq->host_index)
		return -EINVAL;
	/* If the host has not yet processed the next entry then we are done */
	if (((hq->host_index + 1) % hq->entry_count) == hq->hba_index)
		return -EBUSY;
	lpfc_sli_pcimem_bcopy(hrqe, temp_hrqe, hq->entry_size);
	lpfc_sli_pcimem_bcopy(drqe, temp_drqe, dq->entry_size);

	/* Update the host index to point to the next slot */
	hq->host_index = ((hq->host_index + 1) % hq->entry_count);
	dq->host_index = ((dq->host_index + 1) % dq->entry_count);

	/* Ring The Header Receive Queue Doorbell */
	if (!(hq->host_index % hq->entry_repost)) {
		doorbell.word0 = 0;
		if (hq->db_format == LPFC_DB_RING_FORMAT) {
			bf_set(lpfc_rq_db_ring_fm_num_posted, &doorbell,
			       hq->entry_repost);
			bf_set(lpfc_rq_db_ring_fm_id, &doorbell, hq->queue_id);
		} else if (hq->db_format == LPFC_DB_LIST_FORMAT) {
			bf_set(lpfc_rq_db_list_fm_num_posted, &doorbell,
			       hq->entry_repost);
			bf_set(lpfc_rq_db_list_fm_index, &doorbell,
			       hq->host_index);
			bf_set(lpfc_rq_db_list_fm_id, &doorbell, hq->queue_id);
		} else {
			return -EINVAL;
		}
		writel(doorbell.word0, hq->db_regaddr);
	}
	return put_index;
}

/**
 * lpfc_sli4_rq_release - Updates internal hba index for RQ
 * @q: The Header Receive Queue to operate on.
 *
 * This routine will update the HBA index of a queue to reflect consumption of
 * one Receive Queue Entry by the HBA. When the HBA indicates that it has
 * consumed an entry the host calls this function to update the queue's
 * internal pointers. This routine returns the number of entries that were
 * consumed by the HBA.
 **/
static uint32_t
lpfc_sli4_rq_release(struct lpfc_queue *hq, struct lpfc_queue *dq)
{
	/* sanity check on queue memory */
	if (unlikely(!hq) || unlikely(!dq))
		return 0;

	if ((hq->type != LPFC_HRQ) || (dq->type != LPFC_DRQ))
		return 0;
	hq->hba_index = ((hq->hba_index + 1) % hq->entry_count);
	dq->hba_index = ((dq->hba_index + 1) % dq->entry_count);
	return 1;
}

/**
 * lpfc_cmd_iocb - Get next command iocb entry in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function returns pointer to next command iocb entry
 * in the command ring. The caller must hold hbalock to prevent
 * other threads consume the next command iocb.
 * SLI-2/SLI-3 provide different sized iocbs.
 **/
static inline IOCB_t *
lpfc_cmd_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->sli.sli3.cmdringaddr) +
			   pring->sli.sli3.cmdidx * phba->iocb_cmd_size);
}

/**
 * lpfc_resp_iocb - Get next response iocb entry in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function returns pointer to next response iocb entry
 * in the response ring. The caller must hold hbalock to make sure
 * that no other thread consume the next response iocb.
 * SLI-2/SLI-3 provide different sized iocbs.
 **/
static inline IOCB_t *
lpfc_resp_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	return (IOCB_t *) (((char *) pring->sli.sli3.rspringaddr) +
			   pring->sli.sli3.rspidx * phba->iocb_rsp_size);
}

/**
 * __lpfc_sli_get_iocbq - Allocates an iocb object from iocb pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called with hbalock held. This function
 * allocates a new driver iocb object from the iocb pool. If the
 * allocation is successful, it returns pointer to the newly
 * allocated iocb object else it returns NULL.
 **/
struct lpfc_iocbq *
__lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct list_head *lpfc_iocb_list = &phba->lpfc_iocb_list;
	struct lpfc_iocbq * iocbq = NULL;

	list_remove_head(lpfc_iocb_list, iocbq, struct lpfc_iocbq, list);
	if (iocbq)
		phba->iocb_cnt++;
	if (phba->iocb_cnt > phba->iocb_max)
		phba->iocb_max = phba->iocb_cnt;
	return iocbq;
}

/**
 * __lpfc_clear_active_sglq - Remove the active sglq for this XRI.
 * @phba: Pointer to HBA context object.
 * @xritag: XRI value.
 *
 * This function clears the sglq pointer from the array of acive
 * sglq's. The xritag that is passed in is used to index into the
 * array. Before the xritag can be used it needs to be adjusted
 * by subtracting the xribase.
 *
 * Returns sglq ponter = success, NULL = Failure.
 **/
static struct lpfc_sglq *
__lpfc_clear_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	struct lpfc_sglq *sglq;

	sglq = phba->sli4_hba.lpfc_sglq_active_list[xritag];
	phba->sli4_hba.lpfc_sglq_active_list[xritag] = NULL;
	return sglq;
}

/**
 * __lpfc_get_active_sglq - Get the active sglq for this XRI.
 * @phba: Pointer to HBA context object.
 * @xritag: XRI value.
 *
 * This function returns the sglq pointer from the array of acive
 * sglq's. The xritag that is passed in is used to index into the
 * array. Before the xritag can be used it needs to be adjusted
 * by subtracting the xribase.
 *
 * Returns sglq ponter = success, NULL = Failure.
 **/
struct lpfc_sglq *
__lpfc_get_active_sglq(struct lpfc_hba *phba, uint16_t xritag)
{
	struct lpfc_sglq *sglq;

	sglq =  phba->sli4_hba.lpfc_sglq_active_list[xritag];
	return sglq;
}

/**
 * lpfc_clr_rrq_active - Clears RRQ active bit in xri_bitmap.
 * @phba: Pointer to HBA context object.
 * @xritag: xri used in this exchange.
 * @rrq: The RRQ to be cleared.
 *
 **/
void
lpfc_clr_rrq_active(struct lpfc_hba *phba,
		    uint16_t xritag,
		    struct lpfc_node_rrq *rrq)
{
	struct lpfc_nodelist *ndlp = NULL;

	if ((rrq->vport) && NLP_CHK_NODE_ACT(rrq->ndlp))
		ndlp = lpfc_findnode_did(rrq->vport, rrq->nlp_DID);

	/* The target DID could have been swapped (cable swap)
	 * we should use the ndlp from the findnode if it is
	 * available.
	 */
	if ((!ndlp) && rrq->ndlp)
		ndlp = rrq->ndlp;

	if (!ndlp)
		goto out;

	if (test_and_clear_bit(xritag, ndlp->active_rrqs.xri_bitmap)) {
		rrq->send_rrq = 0;
		rrq->xritag = 0;
		rrq->rrq_stop_time = 0;
	}
out:
	mempool_free(rrq, phba->rrq_pool);
}

/**
 * lpfc_handle_rrq_active - Checks if RRQ has waithed RATOV.
 * @phba: Pointer to HBA context object.
 *
 * This function is called with hbalock held. This function
 * Checks if stop_time (ratov from setting rrq active) has
 * been reached, if it has and the send_rrq flag is set then
 * it will call lpfc_send_rrq. If the send_rrq flag is not set
 * then it will just call the routine to clear the rrq and
 * free the rrq resource.
 * The timer is set to the next rrq that is going to expire before
 * leaving the routine.
 *
 **/
void
lpfc_handle_rrq_active(struct lpfc_hba *phba)
{
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long next_time;
	unsigned long iflags;
	LIST_HEAD(send_rrq);

	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	next_time = jiffies + msecs_to_jiffies(1000 * (phba->fc_ratov + 1));
	list_for_each_entry_safe(rrq, nextrrq,
				 &phba->active_rrq_list, list) {
		if (time_after(jiffies, rrq->rrq_stop_time))
			list_move(&rrq->list, &send_rrq);
		else if (time_before(rrq->rrq_stop_time, next_time))
			next_time = rrq->rrq_stop_time;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	if (!list_empty(&phba->active_rrq_list))
		mod_timer(&phba->rrq_tmr, next_time);
	list_for_each_entry_safe(rrq, nextrrq, &send_rrq, list) {
		list_del(&rrq->list);
		if (!rrq->send_rrq)
			/* this call will free the rrq */
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
		else if (lpfc_send_rrq(phba, rrq)) {
			/* if we send the rrq then the completion handler
			*  will clear the bit in the xribitmap.
			*/
			lpfc_clr_rrq_active(phba, rrq->xritag,
					    rrq);
		}
	}
}

/**
 * lpfc_get_active_rrq - Get the active RRQ for this exchange.
 * @vport: Pointer to vport context object.
 * @xri: The xri used in the exchange.
 * @did: The targets DID for this exchange.
 *
 * returns NULL = rrq not found in the phba->active_rrq_list.
 *         rrq = rrq for this xri and target.
 **/
struct lpfc_node_rrq *
lpfc_get_active_rrq(struct lpfc_vport *vport, uint16_t xri, uint32_t did)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long iflags;

	if (phba->sli_rev != LPFC_SLI_REV4)
		return NULL;
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry_safe(rrq, nextrrq, &phba->active_rrq_list, list) {
		if (rrq->vport == vport && rrq->xritag == xri &&
				rrq->nlp_DID == did){
			list_del(&rrq->list);
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			return rrq;
		}
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return NULL;
}

/**
 * lpfc_cleanup_vports_rrqs - Remove and clear the active RRQ for this vport.
 * @vport: Pointer to vport context object.
 * @ndlp: Pointer to the lpfc_node_list structure.
 * If ndlp is NULL Remove all active RRQs for this vport from the
 * phba->active_rrq_list and clear the rrq.
 * If ndlp is not NULL then only remove rrqs for this vport & this ndlp.
 **/
void
lpfc_cleanup_vports_rrqs(struct lpfc_vport *vport, struct lpfc_nodelist *ndlp)

{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long iflags;
	LIST_HEAD(rrq_list);

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;
	if (!ndlp) {
		lpfc_sli4_vport_delete_els_xri_aborted(vport);
		lpfc_sli4_vport_delete_fcp_xri_aborted(vport);
	}
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry_safe(rrq, nextrrq, &phba->active_rrq_list, list)
		if ((rrq->vport == vport) && (!ndlp  || rrq->ndlp == ndlp))
			list_move(&rrq->list, &rrq_list);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	list_for_each_entry_safe(rrq, nextrrq, &rrq_list, list) {
		list_del(&rrq->list);
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	}
}

/**
 * lpfc_cleanup_wt_rrqs - Remove all rrq's from the active list.
 * @phba: Pointer to HBA context object.
 *
 * Remove all rrqs from the phba->active_rrq_list and free them by
 * calling __lpfc_clr_active_rrq
 *
 **/
void
lpfc_cleanup_wt_rrqs(struct lpfc_hba *phba)
{
	struct lpfc_node_rrq *rrq;
	struct lpfc_node_rrq *nextrrq;
	unsigned long next_time;
	unsigned long iflags;
	LIST_HEAD(rrq_list);

	if (phba->sli_rev != LPFC_SLI_REV4)
		return;
	spin_lock_irqsave(&phba->hbalock, iflags);
	phba->hba_flag &= ~HBA_RRQ_ACTIVE;
	next_time = jiffies + msecs_to_jiffies(1000 * (phba->fc_ratov * 2));
	list_splice_init(&phba->active_rrq_list, &rrq_list);
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	list_for_each_entry_safe(rrq, nextrrq, &rrq_list, list) {
		list_del(&rrq->list);
		lpfc_clr_rrq_active(phba, rrq->xritag, rrq);
	}
	if (!list_empty(&phba->active_rrq_list))
		mod_timer(&phba->rrq_tmr, next_time);
}


/**
 * lpfc_test_rrq_active - Test RRQ bit in xri_bitmap.
 * @phba: Pointer to HBA context object.
 * @ndlp: Targets nodelist pointer for this exchange.
 * @xritag the xri in the bitmap to test.
 *
 * This function is called with hbalock held. This function
 * returns 0 = rrq not active for this xri
 *         1 = rrq is valid for this xri.
 **/
int
lpfc_test_rrq_active(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
			uint16_t  xritag)
{
	if (!ndlp)
		return 0;
	if (test_bit(xritag, ndlp->active_rrqs.xri_bitmap))
			return 1;
	else
		return 0;
}

/**
 * lpfc_set_rrq_active - set RRQ active bit in xri_bitmap.
 * @phba: Pointer to HBA context object.
 * @ndlp: nodelist pointer for this target.
 * @xritag: xri used in this exchange.
 * @rxid: Remote Exchange ID.
 * @send_rrq: Flag used to determine if we should send rrq els cmd.
 *
 * This function takes the hbalock.
 * The active bit is always set in the active rrq xri_bitmap even
 * if there is no slot avaiable for the other rrq information.
 *
 * returns 0 rrq actived for this xri
 *         < 0 No memory or invalid ndlp.
 **/
int
lpfc_set_rrq_active(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
		    uint16_t xritag, uint16_t rxid, uint16_t send_rrq)
{
	unsigned long iflags;
	struct lpfc_node_rrq *rrq;
	int empty;

	if (!ndlp)
		return -EINVAL;

	if (!phba->cfg_enable_rrq)
		return -EINVAL;

	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->pport->load_flag & FC_UNLOADING) {
		phba->hba_flag &= ~HBA_RRQ_ACTIVE;
		goto out;
	}

	/*
	 * set the active bit even if there is no mem available.
	 */
	if (NLP_CHK_FREE_REQ(ndlp))
		goto out;

	if (ndlp->vport && (ndlp->vport->load_flag & FC_UNLOADING))
		goto out;

	if (test_and_set_bit(xritag, ndlp->active_rrqs.xri_bitmap))
		goto out;

	spin_unlock_irqrestore(&phba->hbalock, iflags);
	rrq = mempool_alloc(phba->rrq_pool, GFP_KERNEL);
	if (!rrq) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3155 Unable to allocate RRQ xri:0x%x rxid:0x%x"
				" DID:0x%x Send:%d\n",
				xritag, rxid, ndlp->nlp_DID, send_rrq);
		return -EINVAL;
	}
	if (phba->cfg_enable_rrq == 1)
		rrq->send_rrq = send_rrq;
	else
		rrq->send_rrq = 0;
	rrq->xritag = xritag;
	rrq->rrq_stop_time = jiffies +
				msecs_to_jiffies(1000 * (phba->fc_ratov + 1));
	rrq->ndlp = ndlp;
	rrq->nlp_DID = ndlp->nlp_DID;
	rrq->vport = ndlp->vport;
	rrq->rxid = rxid;
	spin_lock_irqsave(&phba->hbalock, iflags);
	empty = list_empty(&phba->active_rrq_list);
	list_add_tail(&rrq->list, &phba->active_rrq_list);
	phba->hba_flag |= HBA_RRQ_ACTIVE;
	if (empty)
		lpfc_worker_wake_up(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return 0;
out:
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"2921 Can't set rrq active xri:0x%x rxid:0x%x"
			" DID:0x%x Send:%d\n",
			xritag, rxid, ndlp->nlp_DID, send_rrq);
	return -EINVAL;
}

/**
 * __lpfc_sli_get_sglq - Allocates an iocb object from sgl pool
 * @phba: Pointer to HBA context object.
 * @piocb: Pointer to the iocbq.
 *
 * This function is called with hbalock held. This function
 * gets a new driver sglq object from the sglq list. If the
 * list is not empty then it is successful, it returns pointer to the newly
 * allocated sglq object else it returns NULL.
 **/
static struct lpfc_sglq *
__lpfc_sli_get_sglq(struct lpfc_hba *phba, struct lpfc_iocbq *piocbq)
{
	struct list_head *lpfc_sgl_list = &phba->sli4_hba.lpfc_sgl_list;
	struct lpfc_sglq *sglq = NULL;
	struct lpfc_sglq *start_sglq = NULL;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_nodelist *ndlp;
	int found = 0;

	if (piocbq->iocb_flag &  LPFC_IO_FCP) {
		lpfc_cmd = (struct lpfc_scsi_buf *) piocbq->context1;
		ndlp = lpfc_cmd->rdata->pnode;
	} else  if ((piocbq->iocb.ulpCommand == CMD_GEN_REQUEST64_CR) &&
			!(piocbq->iocb_flag & LPFC_IO_LIBDFC))
		ndlp = piocbq->context_un.ndlp;
	else  if (piocbq->iocb_flag & LPFC_IO_LIBDFC)
		ndlp = piocbq->context_un.ndlp;
	else
		ndlp = piocbq->context1;

	list_remove_head(lpfc_sgl_list, sglq, struct lpfc_sglq, list);
	start_sglq = sglq;
	while (!found) {
		if (!sglq)
			return NULL;
		if (lpfc_test_rrq_active(phba, ndlp, sglq->sli4_lxritag)) {
			/* This xri has an rrq outstanding for this DID.
			 * put it back in the list and get another xri.
			 */
			list_add_tail(&sglq->list, lpfc_sgl_list);
			sglq = NULL;
			list_remove_head(lpfc_sgl_list, sglq,
						struct lpfc_sglq, list);
			if (sglq == start_sglq) {
				sglq = NULL;
				break;
			} else
				continue;
		}
		sglq->ndlp = ndlp;
		found = 1;
		phba->sli4_hba.lpfc_sglq_active_list[sglq->sli4_lxritag] = sglq;
		sglq->state = SGL_ALLOCATED;
	}
	return sglq;
}

/**
 * lpfc_sli_get_iocbq - Allocates an iocb object from iocb pool
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. This function
 * allocates a new driver iocb object from the iocb pool. If the
 * allocation is successful, it returns pointer to the newly
 * allocated iocb object else it returns NULL.
 **/
struct lpfc_iocbq *
lpfc_sli_get_iocbq(struct lpfc_hba *phba)
{
	struct lpfc_iocbq * iocbq = NULL;
	unsigned long iflags;

	spin_lock_irqsave(&phba->hbalock, iflags);
	iocbq = __lpfc_sli_get_iocbq(phba);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return iocbq;
}

/**
 * __lpfc_sli_release_iocbq_s4 - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 * The sqlq structure that holds the xritag and phys and virtual
 * mappings for the scatter gather list is retrieved from the
 * active array of sglq. The get of the sglq pointer also clears
 * the entry in the array. If the status of the IO indiactes that
 * this IO was aborted then the sglq entry it put on the
 * lpfc_abts_els_sgl_list until the CQ_ABORTED_XRI is received. If the
 * IO has good status or fails for any other reason then the sglq
 * entry is added to the free list (lpfc_sgl_list).
 **/
static void
__lpfc_sli_release_iocbq_s4(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_sglq *sglq;
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);
	unsigned long iflag = 0;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];

	if (iocbq->sli4_xritag == NO_XRI)
		sglq = NULL;
	else
		sglq = __lpfc_clear_active_sglq(phba, iocbq->sli4_lxritag);

	/*
	** This should have been removed from the txcmplq before calling
	** iocbq_release. The normal completion
	** path should have already done the list_del_init.
	*/
	if (unlikely(!list_empty(&iocbq->list))) {
		if (iocbq->iocb_flag & LPFC_IO_ON_TXCMPLQ)
			iocbq->iocb_flag &= ~LPFC_IO_ON_TXCMPLQ;
		list_del_init(&iocbq->list);
	}


	if (sglq)  {
		if ((iocbq->iocb_flag & LPFC_EXCHANGE_BUSY) &&
			(sglq->state != SGL_XRI_ABORTED)) {
			spin_lock_irqsave(&phba->sli4_hba.abts_sgl_list_lock,
					iflag);
			list_add(&sglq->list,
				&phba->sli4_hba.lpfc_abts_els_sgl_list);
			spin_unlock_irqrestore(
				&phba->sli4_hba.abts_sgl_list_lock, iflag);
		} else {
			sglq->state = SGL_FREED;
			sglq->ndlp = NULL;
			list_add_tail(&sglq->list,
				&phba->sli4_hba.lpfc_sgl_list);

			/* Check if TXQ queue needs to be serviced */
			if (!list_empty(&pring->txq))
				lpfc_worker_wake_up(phba);
		}
	}


	/*
	 * Clean all volatile data fields, preserve iotag and node struct.
	 */
	memset((char *)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_lxritag = NO_XRI;
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}


/**
 * __lpfc_sli_release_iocbq_s3 - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 **/
static void
__lpfc_sli_release_iocbq_s3(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	size_t start_clean = offsetof(struct lpfc_iocbq, iocb);

	/*
	** This should have been removed from the txcmplq before calling
	** iocbq_release. The normal completion
	** path should have already done the list_del_init.
	*/
	if (unlikely(!list_empty(&iocbq->list)))
		list_del_init(&iocbq->list);

	/*
	 * Clean all volatile data fields, preserve iotag and node struct.
	 */
	memset((char*)iocbq + start_clean, 0, sizeof(*iocbq) - start_clean);
	iocbq->sli4_xritag = NO_XRI;
	list_add_tail(&iocbq->list, &phba->lpfc_iocb_list);
}

/**
 * __lpfc_sli_release_iocbq - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with hbalock held to release driver
 * iocb object to the iocb pool. The iotag in the iocb object
 * does not change for each use of the iocb object. This function
 * clears all other fields of the iocb object when it is freed.
 **/
static void
__lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	phba->__lpfc_sli_release_iocbq(phba, iocbq);
	phba->iocb_cnt--;
}

/**
 * lpfc_sli_release_iocbq - Release iocb to the iocb pool
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function is called with no lock held to release the iocb to
 * iocb pool.
 **/
void
lpfc_sli_release_iocbq(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	unsigned long iflags;

	/*
	 * Clean all volatile data fields, preserve iotag and node struct.
	 */
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_sli_release_iocbq(phba, iocbq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
}

/**
 * lpfc_sli_cancel_iocbs - Cancel all iocbs from a list.
 * @phba: Pointer to HBA context object.
 * @iocblist: List of IOCBs.
 * @ulpstatus: ULP status in IOCB command field.
 * @ulpWord4: ULP word-4 in IOCB command field.
 *
 * This function is called with a list of IOCBs to cancel. It cancels the IOCB
 * on the list by invoking the complete callback function associated with the
 * IOCB with the provided @ulpstatus and @ulpword4 set to the IOCB commond
 * fields.
 **/
void
lpfc_sli_cancel_iocbs(struct lpfc_hba *phba, struct list_head *iocblist,
		      uint32_t ulpstatus, uint32_t ulpWord4)
{
	struct lpfc_iocbq *piocb;

	while (!list_empty(iocblist)) {
		list_remove_head(iocblist, piocb, struct lpfc_iocbq, list);
		if (!piocb->iocb_cmpl)
			lpfc_sli_release_iocbq(phba, piocb);
		else {
			piocb->iocb.ulpStatus = ulpstatus;
			piocb->iocb.un.ulpWord[4] = ulpWord4;
			(piocb->iocb_cmpl) (phba, piocb, piocb);
		}
	}
	return;
}

/**
 * lpfc_sli_iocb_cmd_type - Get the iocb type
 * @iocb_cmnd: iocb command code.
 *
 * This function is called by ring event handler function to get the iocb type.
 * This function translates the iocb command to an iocb command type used to
 * decide the final disposition of each completed IOCB.
 * The function returns
 * LPFC_UNKNOWN_IOCB if it is an unsupported iocb
 * LPFC_SOL_IOCB     if it is a solicited iocb completion
 * LPFC_ABORT_IOCB   if it is an abort iocb
 * LPFC_UNSOL_IOCB   if it is an unsolicited iocb
 *
 * The caller is not required to hold any lock.
 **/
static lpfc_iocb_type
lpfc_sli_iocb_cmd_type(uint8_t iocb_cmnd)
{
	lpfc_iocb_type type = LPFC_UNKNOWN_IOCB;

	if (iocb_cmnd > CMD_MAX_IOCB_CMD)
		return 0;

	switch (iocb_cmnd) {
	case CMD_XMIT_SEQUENCE_CR:
	case CMD_XMIT_SEQUENCE_CX:
	case CMD_XMIT_BCAST_CN:
	case CMD_XMIT_BCAST_CX:
	case CMD_ELS_REQUEST_CR:
	case CMD_ELS_REQUEST_CX:
	case CMD_CREATE_XRI_CR:
	case CMD_CREATE_XRI_CX:
	case CMD_GET_RPI_CN:
	case CMD_XMIT_ELS_RSP_CX:
	case CMD_GET_RPI_CR:
	case CMD_FCP_IWRITE_CR:
	case CMD_FCP_IWRITE_CX:
	case CMD_FCP_IREAD_CR:
	case CMD_FCP_IREAD_CX:
	case CMD_FCP_ICMND_CR:
	case CMD_FCP_ICMND_CX:
	case CMD_FCP_TSEND_CX:
	case CMD_FCP_TRSP_CX:
	case CMD_FCP_TRECEIVE_CX:
	case CMD_FCP_AUTO_TRSP_CX:
	case CMD_ADAPTER_MSG:
	case CMD_ADAPTER_DUMP:
	case CMD_XMIT_SEQUENCE64_CR:
	case CMD_XMIT_SEQUENCE64_CX:
	case CMD_XMIT_BCAST64_CN:
	case CMD_XMIT_BCAST64_CX:
	case CMD_ELS_REQUEST64_CR:
	case CMD_ELS_REQUEST64_CX:
	case CMD_FCP_IWRITE64_CR:
	case CMD_FCP_IWRITE64_CX:
	case CMD_FCP_IREAD64_CR:
	case CMD_FCP_IREAD64_CX:
	case CMD_FCP_ICMND64_CR:
	case CMD_FCP_ICMND64_CX:
	case CMD_FCP_TSEND64_CX:
	case CMD_FCP_TRSP64_CX:
	case CMD_FCP_TRECEIVE64_CX:
	case CMD_GEN_REQUEST64_CR:
	case CMD_GEN_REQUEST64_CX:
	case CMD_XMIT_ELS_RSP64_CX:
	case DSSCMD_IWRITE64_CR:
	case DSSCMD_IWRITE64_CX:
	case DSSCMD_IREAD64_CR:
	case DSSCMD_IREAD64_CX:
		type = LPFC_SOL_IOCB;
		break;
	case CMD_ABORT_XRI_CN:
	case CMD_ABORT_XRI_CX:
	case CMD_CLOSE_XRI_CN:
	case CMD_CLOSE_XRI_CX:
	case CMD_XRI_ABORTED_CX:
	case CMD_ABORT_MXRI64_CN:
	case CMD_XMIT_BLS_RSP64_CX:
		type = LPFC_ABORT_IOCB;
		break;
	case CMD_RCV_SEQUENCE_CX:
	case CMD_RCV_ELS_REQ_CX:
	case CMD_RCV_SEQUENCE64_CX:
	case CMD_RCV_ELS_REQ64_CX:
	case CMD_ASYNC_STATUS:
	case CMD_IOCB_RCV_SEQ64_CX:
	case CMD_IOCB_RCV_ELS64_CX:
	case CMD_IOCB_RCV_CONT64_CX:
	case CMD_IOCB_RET_XRI64_CX:
		type = LPFC_UNSOL_IOCB;
		break;
	case CMD_IOCB_XMIT_MSEQ64_CR:
	case CMD_IOCB_XMIT_MSEQ64_CX:
	case CMD_IOCB_RCV_SEQ_LIST64_CX:
	case CMD_IOCB_RCV_ELS_LIST64_CX:
	case CMD_IOCB_CLOSE_EXTENDED_CN:
	case CMD_IOCB_ABORT_EXTENDED_CN:
	case CMD_IOCB_RET_HBQE64_CN:
	case CMD_IOCB_FCP_IBIDIR64_CR:
	case CMD_IOCB_FCP_IBIDIR64_CX:
	case CMD_IOCB_FCP_ITASKMGT64_CX:
	case CMD_IOCB_LOGENTRY_CN:
	case CMD_IOCB_LOGENTRY_ASYNC_CN:
		printk("%s - Unhandled SLI-3 Command x%x\n",
				__func__, iocb_cmnd);
		type = LPFC_UNKNOWN_IOCB;
		break;
	default:
		type = LPFC_UNKNOWN_IOCB;
		break;
	}

	return type;
}

/**
 * lpfc_sli_ring_map - Issue config_ring mbox for all rings
 * @phba: Pointer to HBA context object.
 *
 * This function is called from SLI initialization code
 * to configure every ring of the HBA's SLI interface. The
 * caller is not required to hold any lock. This function issues
 * a config_ring mailbox command for each ring.
 * This function returns zero if successful else returns a negative
 * error code.
 **/
static int
lpfc_sli_ring_map(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	int i, rc, ret = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return -ENOMEM;
	pmbox = &pmb->u.mb;
	phba->link_state = LPFC_INIT_MBX_CMDS;
	for (i = 0; i < psli->num_rings; i++) {
		lpfc_config_ring(phba, i, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0446 Adapter failed to init (%d), "
					"mbxCmd x%x CFG_RING, mbxStatus x%x, "
					"ring %d\n",
					rc, pmbox->mbxCommand,
					pmbox->mbxStatus, i);
			phba->link_state = LPFC_HBA_ERROR;
			ret = -ENXIO;
			break;
		}
	}
	mempool_free(pmb, phba->mbox_mem_pool);
	return ret;
}

/**
 * lpfc_sli_ringtxcmpl_put - Adds new iocb to the txcmplq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @piocb: Pointer to the driver iocb object.
 *
 * This function is called with hbalock held. The function adds the
 * new iocb to txcmplq of the given ring. This function always returns
 * 0. If this function is called for ELS ring, this function checks if
 * there is a vport associated with the ELS command. This function also
 * starts els_tmofunc timer if this is an ELS command.
 **/
static int
lpfc_sli_ringtxcmpl_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *piocb)
{
	list_add_tail(&piocb->list, &pring->txcmplq);
	piocb->iocb_flag |= LPFC_IO_ON_TXCMPLQ;

	if ((unlikely(pring->ringno == LPFC_ELS_RING)) &&
	   (piocb->iocb.ulpCommand != CMD_ABORT_XRI_CN) &&
	   (piocb->iocb.ulpCommand != CMD_CLOSE_XRI_CN)) {
		if (!piocb->vport)
			BUG();
		else
			mod_timer(&piocb->vport->els_tmofunc,
				jiffies +
				msecs_to_jiffies(1000 * (phba->fc_ratov << 1)));
	}


	return 0;
}

/**
 * lpfc_sli_ringtx_get - Get first element of the txq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called with hbalock held to get next
 * iocb in txq of the given ring. If there is any iocb in
 * the txq, the function returns first iocb in the list after
 * removing the iocb from the list, else it returns NULL.
 **/
struct lpfc_iocbq *
lpfc_sli_ringtx_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_iocbq *cmd_iocb;

	list_remove_head((&pring->txq), cmd_iocb, struct lpfc_iocbq, list);
	return cmd_iocb;
}

/**
 * lpfc_sli_next_iocb_slot - Get next iocb slot in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called with hbalock held and the caller must post the
 * iocb without releasing the lock. If the caller releases the lock,
 * iocb slot returned by the function is not guaranteed to be available.
 * The function returns pointer to the next available iocb slot if there
 * is available slot in the ring, else it returns NULL.
 * If the get index of the ring is ahead of the put index, the function
 * will post an error attention event to the worker thread to take the
 * HBA to offline state.
 **/
static IOCB_t *
lpfc_sli_next_iocb_slot (struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	uint32_t  max_cmd_idx = pring->sli.sli3.numCiocb;
	if ((pring->sli.sli3.next_cmdidx == pring->sli.sli3.cmdidx) &&
	   (++pring->sli.sli3.next_cmdidx >= max_cmd_idx))
		pring->sli.sli3.next_cmdidx = 0;

	if (unlikely(pring->sli.sli3.local_getidx ==
		pring->sli.sli3.next_cmdidx)) {

		pring->sli.sli3.local_getidx = le32_to_cpu(pgp->cmdGetInx);

		if (unlikely(pring->sli.sli3.local_getidx >= max_cmd_idx)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0315 Ring %d issue: portCmdGet %d "
					"is bigger than cmd ring %d\n",
					pring->ringno,
					pring->sli.sli3.local_getidx,
					max_cmd_idx);

			phba->link_state = LPFC_HBA_ERROR;
			/*
			 * All error attention handlers are posted to
			 * worker thread
			 */
			phba->work_ha |= HA_ERATT;
			phba->work_hs = HS_FFER3;

			lpfc_worker_wake_up(phba);

			return NULL;
		}

		if (pring->sli.sli3.local_getidx == pring->sli.sli3.next_cmdidx)
			return NULL;
	}

	return lpfc_cmd_iocb(phba, pring);
}

/**
 * lpfc_sli_next_iotag - Get an iotag for the iocb
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to driver iocb object.
 *
 * This function gets an iotag for the iocb. If there is no unused iotag and
 * the iocbq_lookup_len < 0xffff, this function allocates a bigger iotag_lookup
 * array and assigns a new iotag.
 * The function returns the allocated iotag if successful, else returns zero.
 * Zero is not a valid iotag.
 * The caller is not required to hold any lock.
 **/
uint16_t
lpfc_sli_next_iotag(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	struct lpfc_iocbq **new_arr;
	struct lpfc_iocbq **old_arr;
	size_t new_len;
	struct lpfc_sli *psli = &phba->sli;
	uint16_t iotag;

	spin_lock_irq(&phba->hbalock);
	iotag = psli->last_iotag;
	if(++iotag < psli->iocbq_lookup_len) {
		psli->last_iotag = iotag;
		psli->iocbq_lookup[iotag] = iocbq;
		spin_unlock_irq(&phba->hbalock);
		iocbq->iotag = iotag;
		return iotag;
	} else if (psli->iocbq_lookup_len < (0xffff
					   - LPFC_IOCBQ_LOOKUP_INCREMENT)) {
		new_len = psli->iocbq_lookup_len + LPFC_IOCBQ_LOOKUP_INCREMENT;
		spin_unlock_irq(&phba->hbalock);
		new_arr = kzalloc(new_len * sizeof (struct lpfc_iocbq *),
				  GFP_KERNEL);
		if (new_arr) {
			spin_lock_irq(&phba->hbalock);
			old_arr = psli->iocbq_lookup;
			if (new_len <= psli->iocbq_lookup_len) {
				/* highly unprobable case */
				kfree(new_arr);
				iotag = psli->last_iotag;
				if(++iotag < psli->iocbq_lookup_len) {
					psli->last_iotag = iotag;
					psli->iocbq_lookup[iotag] = iocbq;
					spin_unlock_irq(&phba->hbalock);
					iocbq->iotag = iotag;
					return iotag;
				}
				spin_unlock_irq(&phba->hbalock);
				return 0;
			}
			if (psli->iocbq_lookup)
				memcpy(new_arr, old_arr,
				       ((psli->last_iotag  + 1) *
					sizeof (struct lpfc_iocbq *)));
			psli->iocbq_lookup = new_arr;
			psli->iocbq_lookup_len = new_len;
			psli->last_iotag = iotag;
			psli->iocbq_lookup[iotag] = iocbq;
			spin_unlock_irq(&phba->hbalock);
			iocbq->iotag = iotag;
			kfree(old_arr);
			return iotag;
		}
	} else
		spin_unlock_irq(&phba->hbalock);

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"0318 Failed to allocate IOTAG.last IOTAG is %d\n",
			psli->last_iotag);

	return 0;
}

/**
 * lpfc_sli_submit_iocb - Submit an iocb to the firmware
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @iocb: Pointer to iocb slot in the ring.
 * @nextiocb: Pointer to driver iocb object which need to be
 *            posted to firmware.
 *
 * This function is called with hbalock held to post a new iocb to
 * the firmware. This function copies the new iocb to ring iocb slot and
 * updates the ring pointers. It adds the new iocb to txcmplq if there is
 * a completion call back for this iocb else the function will free the
 * iocb object.
 **/
static void
lpfc_sli_submit_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		IOCB_t *iocb, struct lpfc_iocbq *nextiocb)
{
	/*
	 * Set up an iotag
	 */
	nextiocb->iocb.ulpIoTag = (nextiocb->iocb_cmpl) ? nextiocb->iotag : 0;


	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_debugfs_slow_ring_trc(phba,
			"IOCB cmd ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
			*(((uint32_t *) &nextiocb->iocb) + 4),
			*(((uint32_t *) &nextiocb->iocb) + 6),
			*(((uint32_t *) &nextiocb->iocb) + 7));
	}

	/*
	 * Issue iocb command to adapter
	 */
	lpfc_sli_pcimem_bcopy(&nextiocb->iocb, iocb, phba->iocb_cmd_size);
	wmb();
	pring->stats.iocb_cmd++;

	/*
	 * If there is no completion routine to call, we can release the
	 * IOCB buffer back right now. For IOCBs, like QUE_RING_BUF,
	 * that have no rsp ring completion, iocb_cmpl MUST be NULL.
	 */
	if (nextiocb->iocb_cmpl)
		lpfc_sli_ringtxcmpl_put(phba, pring, nextiocb);
	else
		__lpfc_sli_release_iocbq(phba, nextiocb);

	/*
	 * Let the HBA know what IOCB slot will be the next one the
	 * driver will put a command into.
	 */
	pring->sli.sli3.cmdidx = pring->sli.sli3.next_cmdidx;
	writel(pring->sli.sli3.cmdidx, &phba->host_gp[pring->ringno].cmdPutInx);
}

/**
 * lpfc_sli_update_full_ring - Update the chip attention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * The caller is not required to hold any lock for calling this function.
 * This function updates the chip attention bits for the ring to inform firmware
 * that there are pending work to be done for this ring and requests an
 * interrupt when there is space available in the ring. This function is
 * called when the driver is unable to post more iocbs to the ring due
 * to unavailability of space in the ring.
 **/
static void
lpfc_sli_update_full_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	pring->flag |= LPFC_CALL_RING_AVAILABLE;

	wmb();

	/*
	 * Set ring 'ringno' to SET R0CE_REQ in Chip Att register.
	 * The HBA will tell us when an IOCB entry is available.
	 */
	writel((CA_R0ATT|CA_R0CE_REQ) << (ringno*4), phba->CAregaddr);
	readl(phba->CAregaddr); /* flush */

	pring->stats.iocb_cmd_full++;
}

/**
 * lpfc_sli_update_ring - Update chip attention register
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function updates the chip attention register bit for the
 * given ring to inform HBA that there is more work to be done
 * in this ring. The caller is not required to hold any lock.
 **/
static void
lpfc_sli_update_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	int ringno = pring->ringno;

	/*
	 * Tell the HBA that there is work to do in this ring.
	 */
	if (!(phba->sli3_options & LPFC_SLI3_CRP_ENABLED)) {
		wmb();
		writel(CA_R0ATT << (ringno * 4), phba->CAregaddr);
		readl(phba->CAregaddr); /* flush */
	}
}

/**
 * lpfc_sli_resume_iocb - Process iocbs in the txq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called with hbalock held to post pending iocbs
 * in the txq to the firmware. This function is called when driver
 * detects space available in the ring.
 **/
static void
lpfc_sli_resume_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	IOCB_t *iocb;
	struct lpfc_iocbq *nextiocb;

	/*
	 * Check to see if:
	 *  (a) there is anything on the txq to send
	 *  (b) link is up
	 *  (c) link attention events can be processed (fcp ring only)
	 *  (d) IOCB processing is not blocked by the outstanding mbox command.
	 */

	if (lpfc_is_link_up(phba) &&
	    (!list_empty(&pring->txq)) &&
	    (pring->ringno != phba->sli.fcp_ring ||
	     phba->sli.sli_flag & LPFC_PROCESS_LA)) {

		while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
		       (nextiocb = lpfc_sli_ringtx_get(phba, pring)))
			lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

		if (iocb)
			lpfc_sli_update_ring(phba, pring);
		else
			lpfc_sli_update_full_ring(phba, pring);
	}

	return;
}

/**
 * lpfc_sli_next_hbq_slot - Get next hbq entry for the HBQ
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 *
 * This function is called with hbalock held to get the next
 * available slot for the given HBQ. If there is free slot
 * available for the HBQ it will return pointer to the next available
 * HBQ entry else it will return NULL.
 **/
static struct lpfc_hbq_entry *
lpfc_sli_next_hbq_slot(struct lpfc_hba *phba, uint32_t hbqno)
{
	struct hbq_s *hbqp = &phba->hbqs[hbqno];

	if (hbqp->next_hbqPutIdx == hbqp->hbqPutIdx &&
	    ++hbqp->next_hbqPutIdx >= hbqp->entry_count)
		hbqp->next_hbqPutIdx = 0;

	if (unlikely(hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)) {
		uint32_t raw_index = phba->hbq_get[hbqno];
		uint32_t getidx = le32_to_cpu(raw_index);

		hbqp->local_hbqGetIdx = getidx;

		if (unlikely(hbqp->local_hbqGetIdx >= hbqp->entry_count)) {
			lpfc_printf_log(phba, KERN_ERR,
					LOG_SLI | LOG_VPORT,
					"1802 HBQ %d: local_hbqGetIdx "
					"%u is > than hbqp->entry_count %u\n",
					hbqno, hbqp->local_hbqGetIdx,
					hbqp->entry_count);

			phba->link_state = LPFC_HBA_ERROR;
			return NULL;
		}

		if (hbqp->local_hbqGetIdx == hbqp->next_hbqPutIdx)
			return NULL;
	}

	return (struct lpfc_hbq_entry *) phba->hbqs[hbqno].hbq_virt +
			hbqp->hbqPutIdx;
}

/**
 * lpfc_sli_hbqbuf_free_all - Free all the hbq buffers
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held to free all the
 * hbq buffers while uninitializing the SLI interface. It also
 * frees the HBQ buffers returned by the firmware but not yet
 * processed by the upper layers.
 **/
void
lpfc_sli_hbqbuf_free_all(struct lpfc_hba *phba)
{
	struct lpfc_dmabuf *dmabuf, *next_dmabuf;
	struct hbq_dmabuf *hbq_buf;
	unsigned long flags;
	int i, hbq_count;
	uint32_t hbqno;

	hbq_count = lpfc_sli_hbq_count();
	/* Return all memory used by all HBQs */
	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < hbq_count; ++i) {
		list_for_each_entry_safe(dmabuf, next_dmabuf,
				&phba->hbqs[i].hbq_buffer_list, list) {
			hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
			list_del(&hbq_buf->dbuf.list);
			(phba->hbqs[i].hbq_free_buffer)(phba, hbq_buf);
		}
		phba->hbqs[i].buffer_count = 0;
	}
	/* Return all HBQ buffer that are in-fly */
	list_for_each_entry_safe(dmabuf, next_dmabuf, &phba->rb_pend_list,
				 list) {
		hbq_buf = container_of(dmabuf, struct hbq_dmabuf, dbuf);
		list_del(&hbq_buf->dbuf.list);
		if (hbq_buf->tag == -1) {
			(phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer)
				(phba, hbq_buf);
		} else {
			hbqno = hbq_buf->tag >> 16;
			if (hbqno >= LPFC_MAX_HBQS)
				(phba->hbqs[LPFC_ELS_HBQ].hbq_free_buffer)
					(phba, hbq_buf);
			else
				(phba->hbqs[hbqno].hbq_free_buffer)(phba,
					hbq_buf);
		}
	}

	/* Mark the HBQs not in use */
	phba->hbq_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);
}

/**
 * lpfc_sli_hbq_to_firmware - Post the hbq buffer to firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post a
 * hbq buffer to the firmware. If the function finds an empty
 * slot in the HBQ, it will post the buffer. The function will return
 * pointer to the hbq entry if it successfully post the buffer
 * else it will return NULL.
 **/
static int
lpfc_sli_hbq_to_firmware(struct lpfc_hba *phba, uint32_t hbqno,
			 struct hbq_dmabuf *hbq_buf)
{
	return phba->lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buf);
}

/**
 * lpfc_sli_hbq_to_firmware_s3 - Post the hbq buffer to SLI3 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post a hbq buffer to the
 * firmware. If the function finds an empty slot in the HBQ, it will post the
 * buffer and place it on the hbq_buffer_list. The function will return zero if
 * it successfully post the buffer else it will return an error.
 **/
static int
lpfc_sli_hbq_to_firmware_s3(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	struct lpfc_hbq_entry *hbqe;
	dma_addr_t physaddr = hbq_buf->dbuf.phys;

	/* Get next HBQ entry slot to use */
	hbqe = lpfc_sli_next_hbq_slot(phba, hbqno);
	if (hbqe) {
		struct hbq_s *hbqp = &phba->hbqs[hbqno];

		hbqe->bde.addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		hbqe->bde.addrLow  = le32_to_cpu(putPaddrLow(physaddr));
		hbqe->bde.tus.f.bdeSize = hbq_buf->size;
		hbqe->bde.tus.f.bdeFlags = 0;
		hbqe->bde.tus.w = le32_to_cpu(hbqe->bde.tus.w);
		hbqe->buffer_tag = le32_to_cpu(hbq_buf->tag);
				/* Sync SLIM */
		hbqp->hbqPutIdx = hbqp->next_hbqPutIdx;
		writel(hbqp->hbqPutIdx, phba->hbq_put + hbqno);
				/* flush */
		readl(phba->hbq_put + hbqno);
		list_add_tail(&hbq_buf->dbuf.list, &hbqp->hbq_buffer_list);
		return 0;
	} else
		return -ENOMEM;
}

/**
 * lpfc_sli_hbq_to_firmware_s4 - Post the hbq buffer to SLI4 firmware
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @hbq_buf: Pointer to HBQ buffer.
 *
 * This function is called with the hbalock held to post an RQE to the SLI4
 * firmware. If able to post the RQE to the RQ it will queue the hbq entry to
 * the hbq_buffer_list and return zero, otherwise it will return an error.
 **/
static int
lpfc_sli_hbq_to_firmware_s4(struct lpfc_hba *phba, uint32_t hbqno,
			    struct hbq_dmabuf *hbq_buf)
{
	int rc;
	struct lpfc_rqe hrqe;
	struct lpfc_rqe drqe;

	hrqe.address_lo = putPaddrLow(hbq_buf->hbuf.phys);
	hrqe.address_hi = putPaddrHigh(hbq_buf->hbuf.phys);
	drqe.address_lo = putPaddrLow(hbq_buf->dbuf.phys);
	drqe.address_hi = putPaddrHigh(hbq_buf->dbuf.phys);
	rc = lpfc_sli4_rq_put(phba->sli4_hba.hdr_rq, phba->sli4_hba.dat_rq,
			      &hrqe, &drqe);
	if (rc < 0)
		return rc;
	hbq_buf->tag = rc;
	list_add_tail(&hbq_buf->dbuf.list, &phba->hbqs[hbqno].hbq_buffer_list);
	return 0;
}

/* HBQ for ELS and CT traffic. */
static struct lpfc_hbq_init lpfc_els_hbq = {
	.rn = 1,
	.entry_count = 256,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_ELS_RING),
	.buffer_count = 0,
	.init_count = 40,
	.add_count = 40,
};

/* HBQ for the extra ring if needed */
static struct lpfc_hbq_init lpfc_extra_hbq = {
	.rn = 1,
	.entry_count = 200,
	.mask_count = 0,
	.profile = 0,
	.ring_mask = (1 << LPFC_EXTRA_RING),
	.buffer_count = 0,
	.init_count = 0,
	.add_count = 5,
};

/* Array of HBQs */
struct lpfc_hbq_init *lpfc_hbq_defs[] = {
	&lpfc_els_hbq,
	&lpfc_extra_hbq,
};

/**
 * lpfc_sli_hbqbuf_fill_hbqs - Post more hbq buffers to HBQ
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 * @count: Number of HBQ buffers to be posted.
 *
 * This function is called with no lock held to post more hbq buffers to the
 * given HBQ. The function returns the number of HBQ buffers successfully
 * posted.
 **/
static int
lpfc_sli_hbqbuf_fill_hbqs(struct lpfc_hba *phba, uint32_t hbqno, uint32_t count)
{
	uint32_t i, posted = 0;
	unsigned long flags;
	struct hbq_dmabuf *hbq_buffer;
	LIST_HEAD(hbq_buf_list);
	if (!phba->hbqs[hbqno].hbq_alloc_buffer)
		return 0;

	if ((phba->hbqs[hbqno].buffer_count + count) >
	    lpfc_hbq_defs[hbqno]->entry_count)
		count = lpfc_hbq_defs[hbqno]->entry_count -
					phba->hbqs[hbqno].buffer_count;
	if (!count)
		return 0;
	/* Allocate HBQ entries */
	for (i = 0; i < count; i++) {
		hbq_buffer = (phba->hbqs[hbqno].hbq_alloc_buffer)(phba);
		if (!hbq_buffer)
			break;
		list_add_tail(&hbq_buffer->dbuf.list, &hbq_buf_list);
	}
	/* Check whether HBQ is still in use */
	spin_lock_irqsave(&phba->hbalock, flags);
	if (!phba->hbq_in_use)
		goto err;
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		hbq_buffer->tag = (phba->hbqs[hbqno].buffer_count |
				      (hbqno << 16));
		if (!lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer)) {
			phba->hbqs[hbqno].buffer_count++;
			posted++;
		} else
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);
	return posted;
err:
	spin_unlock_irqrestore(&phba->hbalock, flags);
	while (!list_empty(&hbq_buf_list)) {
		list_remove_head(&hbq_buf_list, hbq_buffer, struct hbq_dmabuf,
				 dbuf.list);
		(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
	return 0;
}

/**
 * lpfc_sli_hbqbuf_add_hbqs - Post more HBQ buffers to firmware
 * @phba: Pointer to HBA context object.
 * @qno: HBQ number.
 *
 * This function posts more buffers to the HBQ. This function
 * is called with no lock held. The function returns the number of HBQ entries
 * successfully allocated.
 **/
int
lpfc_sli_hbqbuf_add_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return 0;
	else
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->add_count);
}

/**
 * lpfc_sli_hbqbuf_init_hbqs - Post initial buffers to the HBQ
 * @phba: Pointer to HBA context object.
 * @qno:  HBQ queue number.
 *
 * This function is called from SLI initialization code path with
 * no lock held to post initial HBQ buffers to firmware. The
 * function returns the number of HBQ entries successfully allocated.
 **/
static int
lpfc_sli_hbqbuf_init_hbqs(struct lpfc_hba *phba, uint32_t qno)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					lpfc_hbq_defs[qno]->entry_count);
	else
		return lpfc_sli_hbqbuf_fill_hbqs(phba, qno,
					 lpfc_hbq_defs[qno]->init_count);
}

/**
 * lpfc_sli_hbqbuf_get - Remove the first hbq off of an hbq list
 * @phba: Pointer to HBA context object.
 * @hbqno: HBQ number.
 *
 * This function removes the first hbq buffer on an hbq list and returns a
 * pointer to that buffer. If it finds no buffers on the list it returns NULL.
 **/
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_get(struct list_head *rb_list)
{
	struct lpfc_dmabuf *d_buf;

	list_remove_head(rb_list, d_buf, struct lpfc_dmabuf, list);
	if (!d_buf)
		return NULL;
	return container_of(d_buf, struct hbq_dmabuf, dbuf);
}

/**
 * lpfc_sli_hbqbuf_find - Find the hbq buffer associated with a tag
 * @phba: Pointer to HBA context object.
 * @tag: Tag of the hbq buffer.
 *
 * This function is called with hbalock held. This function searches
 * for the hbq buffer associated with the given tag in the hbq buffer
 * list. If it finds the hbq buffer, it returns the hbq_buffer other wise
 * it returns NULL.
 **/
static struct hbq_dmabuf *
lpfc_sli_hbqbuf_find(struct lpfc_hba *phba, uint32_t tag)
{
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *hbq_buf;
	uint32_t hbqno;

	hbqno = tag >> 16;
	if (hbqno >= LPFC_MAX_HBQS)
		return NULL;

	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(d_buf, &phba->hbqs[hbqno].hbq_buffer_list, list) {
		hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		if (hbq_buf->tag == tag) {
			spin_unlock_irq(&phba->hbalock);
			return hbq_buf;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI | LOG_VPORT,
			"1803 Bad hbq tag. Data: x%x x%x\n",
			tag, phba->hbqs[tag >> 16].buffer_count);
	return NULL;
}

/**
 * lpfc_sli_free_hbq - Give back the hbq buffer to firmware
 * @phba: Pointer to HBA context object.
 * @hbq_buffer: Pointer to HBQ buffer.
 *
 * This function is called with hbalock. This function gives back
 * the hbq buffer to firmware. If the HBQ does not have space to
 * post the buffer, it will free the buffer.
 **/
void
lpfc_sli_free_hbq(struct lpfc_hba *phba, struct hbq_dmabuf *hbq_buffer)
{
	uint32_t hbqno;

	if (hbq_buffer) {
		hbqno = hbq_buffer->tag >> 16;
		if (lpfc_sli_hbq_to_firmware(phba, hbqno, hbq_buffer))
			(phba->hbqs[hbqno].hbq_free_buffer)(phba, hbq_buffer);
	}
}

/**
 * lpfc_sli_chk_mbx_command - Check if the mailbox is a legitimate mailbox
 * @mbxCommand: mailbox command code.
 *
 * This function is called by the mailbox event handler function to verify
 * that the completed mailbox command is a legitimate mailbox command. If the
 * completed mailbox is not known to the function, it will return MBX_SHUTDOWN
 * and the mailbox event handler will take the HBA offline.
 **/
static int
lpfc_sli_chk_mbx_command(uint8_t mbxCommand)
{
	uint8_t ret;

	switch (mbxCommand) {
	case MBX_LOAD_SM:
	case MBX_READ_NV:
	case MBX_WRITE_NV:
	case MBX_WRITE_VPARMS:
	case MBX_RUN_BIU_DIAG:
	case MBX_INIT_LINK:
	case MBX_DOWN_LINK:
	case MBX_CONFIG_LINK:
	case MBX_CONFIG_RING:
	case MBX_RESET_RING:
	case MBX_READ_CONFIG:
	case MBX_READ_RCONFIG:
	case MBX_READ_SPARM:
	case MBX_READ_STATUS:
	case MBX_READ_RPI:
	case MBX_READ_XRI:
	case MBX_READ_REV:
	case MBX_READ_LNK_STAT:
	case MBX_REG_LOGIN:
	case MBX_UNREG_LOGIN:
	case MBX_CLEAR_LA:
	case MBX_DUMP_MEMORY:
	case MBX_DUMP_CONTEXT:
	case MBX_RUN_DIAGS:
	case MBX_RESTART:
	case MBX_UPDATE_CFG:
	case MBX_DOWN_LOAD:
	case MBX_DEL_LD_ENTRY:
	case MBX_RUN_PROGRAM:
	case MBX_SET_MASK:
	case MBX_SET_VARIABLE:
	case MBX_UNREG_D_ID:
	case MBX_KILL_BOARD:
	case MBX_CONFIG_FARP:
	case MBX_BEACON:
	case MBX_LOAD_AREA:
	case MBX_RUN_BIU_DIAG64:
	case MBX_CONFIG_PORT:
	case MBX_READ_SPARM64:
	case MBX_READ_RPI64:
	case MBX_REG_LOGIN64:
	case MBX_READ_TOPOLOGY:
	case MBX_WRITE_WWN:
	case MBX_SET_DEBUG:
	case MBX_LOAD_EXP_ROM:
	case MBX_ASYNCEVT_ENABLE:
	case MBX_REG_VPI:
	case MBX_UNREG_VPI:
	case MBX_HEARTBEAT:
	case MBX_PORT_CAPABILITIES:
	case MBX_PORT_IOV_CONTROL:
	case MBX_SLI4_CONFIG:
	case MBX_SLI4_REQ_FTRS:
	case MBX_REG_FCFI:
	case MBX_UNREG_FCFI:
	case MBX_REG_VFI:
	case MBX_UNREG_VFI:
	case MBX_INIT_VPI:
	case MBX_INIT_VFI:
	case MBX_RESUME_RPI:
	case MBX_READ_EVENT_LOG_STATUS:
	case MBX_READ_EVENT_LOG:
	case MBX_SECURITY_MGMT:
	case MBX_AUTH_PORT:
	case MBX_ACCESS_VDATA:
		ret = mbxCommand;
		break;
	default:
		ret = MBX_SHUTDOWN;
		break;
	}
	return ret;
}

/**
 * lpfc_sli_wake_mbox_wait - lpfc_sli_issue_mbox_wait mbox completion handler
 * @phba: Pointer to HBA context object.
 * @pmboxq: Pointer to mailbox command.
 *
 * This is completion handler function for mailbox commands issued from
 * lpfc_sli_issue_mbox_wait function. This function is called by the
 * mailbox event handler function with no lock held. This function
 * will wake up thread waiting on the wait queue pointed by context1
 * of the mailbox.
 **/
void
lpfc_sli_wake_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq)
{
	wait_queue_head_t *pdone_q;
	unsigned long drvr_flag;

	/*
	 * If pdone_q is empty, the driver thread gave up waiting and
	 * continued running.
	 */
	pmboxq->mbox_flag |= LPFC_MBX_WAKE;
	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	pdone_q = (wait_queue_head_t *) pmboxq->context1;
	if (pdone_q)
		wake_up_interruptible(pdone_q);
	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return;
}


/**
 * lpfc_sli_def_mbox_cmpl - Default mailbox completion handler
 * @phba: Pointer to HBA context object.
 * @pmb: Pointer to mailbox object.
 *
 * This function is the default mailbox completion handler. It
 * frees the memory resources associated with the completed mailbox
 * command. If the completed command is a REG_LOGIN mailbox command,
 * this function will issue a UREG_LOGIN to re-claim the RPI.
 **/
void
lpfc_sli_def_mbox_cmpl(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmb)
{
	struct lpfc_vport  *vport = pmb->vport;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct Scsi_Host *shost;
	uint16_t rpi, vpi;
	int rc;

	mp = (struct lpfc_dmabuf *) (pmb->context1);

	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
	}

	/*
	 * If a REG_LOGIN succeeded  after node is destroyed or node
	 * is in re-discovery driver need to cleanup the RPI.
	 */
	if (!(phba->pport->load_flag & FC_UNLOADING) &&
	    pmb->u.mb.mbxCommand == MBX_REG_LOGIN64 &&
	    !pmb->u.mb.mbxStatus) {
		rpi = pmb->u.mb.un.varWords[0];
		vpi = pmb->u.mb.un.varRegLogin.vpi;
		lpfc_unreg_login(phba, vpi, rpi, pmb);
		pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
		if (rc != MBX_NOT_FINISHED)
			return;
	}

	if ((pmb->u.mb.mbxCommand == MBX_REG_VPI) &&
		!(phba->pport->load_flag & FC_UNLOADING) &&
		!pmb->u.mb.mbxStatus) {
		shost = lpfc_shost_from_vport(vport);
		spin_lock_irq(shost->host_lock);
		vport->vpi_state |= LPFC_VPI_REGISTERED;
		vport->fc_flag &= ~FC_VPORT_NEEDS_REG_VPI;
		spin_unlock_irq(shost->host_lock);
	}

	if (pmb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
		ndlp = (struct lpfc_nodelist *)pmb->context2;
		lpfc_nlp_put(ndlp);
		pmb->context2 = NULL;
	}

	/* Check security permission status on INIT_LINK mailbox command */
	if ((pmb->u.mb.mbxCommand == MBX_INIT_LINK) &&
	    (pmb->u.mb.mbxStatus == MBXERR_SEC_NO_PERMISSION))
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"2860 SLI authentication is required "
				"for INIT_LINK but has not done yet\n");

	if (bf_get(lpfc_mqe_command, &pmb->u.mqe) == MBX_SLI4_CONFIG)
		lpfc_sli4_mbox_cmd_free(phba, pmb);
	else
		mempool_free(pmb, phba->mbox_mem_pool);
}

/**
 * lpfc_sli_handle_mb_event - Handle mailbox completions from firmware
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. This function processes all
 * the completed mailbox commands and gives it to upper layers. The interrupt
 * service routine processes mailbox completion interrupt and adds completed
 * mailbox commands to the mboxq_cmpl queue and signals the worker thread.
 * Worker thread call lpfc_sli_handle_mb_event, which will return the
 * completed mailbox commands in mboxq_cmpl queue to the upper layers. This
 * function returns the mailbox commands to the upper layer by calling the
 * completion handler function of each mailbox.
 **/
int
lpfc_sli_handle_mb_event(struct lpfc_hba *phba)
{
	MAILBOX_t *pmbox;
	LPFC_MBOXQ_t *pmb;
	int rc;
	LIST_HEAD(cmplq);

	phba->sli.slistat.mbox_event++;

	/* Get all completed mailboxe buffers into the cmplq */
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->sli.mboxq_cmpl, &cmplq);
	spin_unlock_irq(&phba->hbalock);

	/* Get a Mailbox buffer to setup mailbox commands for callback */
	do {
		list_remove_head(&cmplq, pmb, LPFC_MBOXQ_t, list);
		if (pmb == NULL)
			break;

		pmbox = &pmb->u.mb;

		if (pmbox->mbxCommand != MBX_HEARTBEAT) {
			if (pmb->vport) {
				lpfc_debugfs_disc_trc(pmb->vport,
					LPFC_DISC_TRC_MBOX_VPORT,
					"MBOX cmpl vport: cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
			else {
				lpfc_debugfs_disc_trc(phba->pport,
					LPFC_DISC_TRC_MBOX,
					"MBOX cmpl:       cmd:x%x mb:x%x x%x",
					(uint32_t)pmbox->mbxCommand,
					pmbox->un.varWords[0],
					pmbox->un.varWords[1]);
			}
		}

		/*
		 * It is a fatal error if unknown mbox command completion.
		 */
		if (lpfc_sli_chk_mbx_command(pmbox->mbxCommand) ==
		    MBX_SHUTDOWN) {
			/* Unknown mailbox command compl */
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):0323 Unknown Mailbox command "
					"x%x (x%x/x%x) Cmpl\n",
					pmb->vport ? pmb->vport->vpi : 0,
					pmbox->mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									pmb),
					lpfc_sli_config_mbox_opcode_get(phba,
									pmb));
			phba->link_state = LPFC_HBA_ERROR;
			phba->work_hs = HS_FFER3;
			lpfc_handle_eratt(phba);
			continue;
		}

		if (pmbox->mbxStatus) {
			phba->sli.slistat.mbox_stat_err++;
			if (pmbox->mbxStatus == MBXERR_NO_RESOURCES) {
				/* Mbox cmd cmpl error - RETRYing */
				lpfc_printf_log(phba, KERN_INFO,
					LOG_MBOX | LOG_SLI,
					"(%d):0305 Mbox cmd cmpl "
					"error - RETRYing Data: x%x "
					"(x%x/x%x) x%x x%x x%x\n",
					pmb->vport ? pmb->vport->vpi : 0,
					pmbox->mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									pmb),
					lpfc_sli_config_mbox_opcode_get(phba,
									pmb),
					pmbox->mbxStatus,
					pmbox->un.varWords[0],
					pmb->vport->port_state);
				pmbox->mbxStatus = 0;
				pmbox->mbxOwner = OWN_HOST;
				rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
				if (rc != MBX_NOT_FINISHED)
					continue;
			}
		}

		/* Mailbox cmd <cmd> Cmpl <cmpl> */
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0307 Mailbox cmd x%x (x%x/x%x) Cmpl x%p "
				"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x "
				"x%x x%x x%x\n",
				pmb->vport ? pmb->vport->vpi : 0,
				pmbox->mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, pmb),
				lpfc_sli_config_mbox_opcode_get(phba, pmb),
				pmb->mbox_cmpl,
				*((uint32_t *) pmbox),
				pmbox->un.varWords[0],
				pmbox->un.varWords[1],
				pmbox->un.varWords[2],
				pmbox->un.varWords[3],
				pmbox->un.varWords[4],
				pmbox->un.varWords[5],
				pmbox->un.varWords[6],
				pmbox->un.varWords[7],
				pmbox->un.varWords[8],
				pmbox->un.varWords[9],
				pmbox->un.varWords[10]);

		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba,pmb);
	} while (1);
	return 0;
}

/**
 * lpfc_sli_get_buff - Get the buffer associated with the buffer tag
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @tag: buffer tag.
 *
 * This function is called with no lock held. When QUE_BUFTAG_BIT bit
 * is set in the tag the buffer is posted for a particular exchange,
 * the function will return the buffer without replacing the buffer.
 * If the buffer is for unsolicited ELS or CT traffic, this function
 * returns the buffer and also posts another buffer to the firmware.
 **/
static struct lpfc_dmabuf *
lpfc_sli_get_buff(struct lpfc_hba *phba,
		  struct lpfc_sli_ring *pring,
		  uint32_t tag)
{
	struct hbq_dmabuf *hbq_entry;

	if (tag & QUE_BUFTAG_BIT)
		return lpfc_sli_ring_taggedbuf_get(phba, pring, tag);
	hbq_entry = lpfc_sli_hbqbuf_find(phba, tag);
	if (!hbq_entry)
		return NULL;
	return &hbq_entry->dbuf;
}

/**
 * lpfc_complete_unsol_iocb - Complete an unsolicited sequence
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the iocbq struct representing the sequence starting frame.
 * @fch_r_ctl: the r_ctl for the first frame of the sequence.
 * @fch_type: the type for the first frame of the sequence.
 *
 * This function is called with no lock held. This function uses the r_ctl and
 * type of the received sequence to find the correct callback function to call
 * to process the sequence.
 **/
static int
lpfc_complete_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_iocbq *saveq, uint32_t fch_r_ctl,
			 uint32_t fch_type)
{
	int i;

	/* unSolicited Responses */
	if (pring->prt[0].profile) {
		if (pring->prt[0].lpfc_sli_rcv_unsol_event)
			(pring->prt[0].lpfc_sli_rcv_unsol_event) (phba, pring,
									saveq);
		return 1;
	}
	/* We must search, based on rctl / type
	   for the right routine */
	for (i = 0; i < pring->num_mask; i++) {
		if ((pring->prt[i].rctl == fch_r_ctl) &&
		    (pring->prt[i].type == fch_type)) {
			if (pring->prt[i].lpfc_sli_rcv_unsol_event)
				(pring->prt[i].lpfc_sli_rcv_unsol_event)
						(phba, pring, saveq);
			return 1;
		}
	}
	return 0;
}

/**
 * lpfc_sli_process_unsol_iocb - Unsolicited iocb handler
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the unsolicited iocb.
 *
 * This function is called with no lock held by the ring event handler
 * when there is an unsolicited iocb posted to the response ring by the
 * firmware. This function gets the buffer associated with the iocbs
 * and calls the event handler for the ring. This function handles both
 * qring buffers and hbq buffers.
 * When the function returns 1 the caller can free the iocb object otherwise
 * upper layer functions will free the iocb objects.
 **/
static int
lpfc_sli_process_unsol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			    struct lpfc_iocbq *saveq)
{
	IOCB_t           * irsp;
	WORD5            * w5p;
	uint32_t           Rctl, Type;
	uint32_t           match;
	struct lpfc_iocbq *iocbq;
	struct lpfc_dmabuf *dmzbuf;

	match = 0;
	irsp = &(saveq->iocb);

	if (irsp->ulpCommand == CMD_ASYNC_STATUS) {
		if (pring->lpfc_sli_rcv_async_status)
			pring->lpfc_sli_rcv_async_status(phba, pring, saveq);
		else
			lpfc_printf_log(phba,
					KERN_WARNING,
					LOG_SLI,
					"0316 Ring %d handler: unexpected "
					"ASYNC_STATUS iocb received evt_code "
					"0x%x\n",
					pring->ringno,
					irsp->un.asyncstat.evt_code);
		return 1;
	}

	if ((irsp->ulpCommand == CMD_IOCB_RET_XRI64_CX) &&
		(phba->sli3_options & LPFC_SLI3_HBQ_ENABLED)) {
		if (irsp->ulpBdeCount > 0) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
					irsp->un.ulpWord[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 1) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
					irsp->unsli3.sli3Words[3]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		if (irsp->ulpBdeCount > 2) {
			dmzbuf = lpfc_sli_get_buff(phba, pring,
				irsp->unsli3.sli3Words[7]);
			lpfc_in_buf_free(phba, dmzbuf);
		}

		return 1;
	}

	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		if (irsp->ulpBdeCount != 0) {
			saveq->context2 = lpfc_sli_get_buff(phba, pring,
						irsp->un.ulpWord[3]);
			if (!saveq->context2)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0341 Ring %d Cannot find buffer for "
					"an unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->un.ulpWord[3]);
		}
		if (irsp->ulpBdeCount == 2) {
			saveq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
			if (!saveq->context3)
				lpfc_printf_log(phba,
					KERN_ERR,
					LOG_SLI,
					"0342 Ring %d Cannot find buffer for an"
					" unsolicited iocb. tag 0x%x\n",
					pring->ringno,
					irsp->unsli3.sli3Words[7]);
		}
		list_for_each_entry(iocbq, &saveq->list, list) {
			irsp = &(iocbq->iocb);
			if (irsp->ulpBdeCount != 0) {
				iocbq->context2 = lpfc_sli_get_buff(phba, pring,
							irsp->un.ulpWord[3]);
				if (!iocbq->context2)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0343 Ring %d Cannot find "
						"buffer for an unsolicited iocb"
						". tag 0x%x\n", pring->ringno,
						irsp->un.ulpWord[3]);
			}
			if (irsp->ulpBdeCount == 2) {
				iocbq->context3 = lpfc_sli_get_buff(phba, pring,
						irsp->unsli3.sli3Words[7]);
				if (!iocbq->context3)
					lpfc_printf_log(phba,
						KERN_ERR,
						LOG_SLI,
						"0344 Ring %d Cannot find "
						"buffer for an unsolicited "
						"iocb. tag 0x%x\n",
						pring->ringno,
						irsp->unsli3.sli3Words[7]);
			}
		}
	}
	if (irsp->ulpBdeCount != 0 &&
	    (irsp->ulpCommand == CMD_IOCB_RCV_CONT64_CX ||
	     irsp->ulpStatus == IOSTAT_INTERMED_RSP)) {
		int found = 0;

		/* search continue save q for same XRI */
		list_for_each_entry(iocbq, &pring->iocb_continue_saveq, clist) {
			if (iocbq->iocb.unsli3.rcvsli3.ox_id ==
				saveq->iocb.unsli3.rcvsli3.ox_id) {
				list_add_tail(&saveq->list, &iocbq->list);
				found = 1;
				break;
			}
		}
		if (!found)
			list_add_tail(&saveq->clist,
				      &pring->iocb_continue_saveq);
		if (saveq->iocb.ulpStatus != IOSTAT_INTERMED_RSP) {
			list_del_init(&iocbq->clist);
			saveq = iocbq;
			irsp = &(saveq->iocb);
		} else
			return 0;
	}
	if ((irsp->ulpCommand == CMD_RCV_ELS_REQ64_CX) ||
	    (irsp->ulpCommand == CMD_RCV_ELS_REQ_CX) ||
	    (irsp->ulpCommand == CMD_IOCB_RCV_ELS64_CX)) {
		Rctl = FC_RCTL_ELS_REQ;
		Type = FC_TYPE_ELS;
	} else {
		w5p = (WORD5 *)&(saveq->iocb.un.ulpWord[5]);
		Rctl = w5p->hcsw.Rctl;
		Type = w5p->hcsw.Type;

		/* Firmware Workaround */
		if ((Rctl == 0) && (pring->ringno == LPFC_ELS_RING) &&
			(irsp->ulpCommand == CMD_RCV_SEQUENCE64_CX ||
			 irsp->ulpCommand == CMD_IOCB_RCV_SEQ64_CX)) {
			Rctl = FC_RCTL_ELS_REQ;
			Type = FC_TYPE_ELS;
			w5p->hcsw.Rctl = Rctl;
			w5p->hcsw.Type = Type;
		}
	}

	if (!lpfc_complete_unsol_iocb(phba, pring, saveq, Rctl, Type))
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0313 Ring %d handler: unexpected Rctl x%x "
				"Type x%x received\n",
				pring->ringno, Rctl, Type);

	return 1;
}

/**
 * lpfc_sli_iocbq_lookup - Find command iocb for the given response iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @prspiocb: Pointer to response iocb object.
 *
 * This function looks up the iocb_lookup table to get the command iocb
 * corresponding to the given response iocb using the iotag of the
 * response iocb. This function is called with the hbalock held.
 * This function returns the command iocb object if it finds the command
 * iocb else returns NULL.
 **/
static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup(struct lpfc_hba *phba,
		      struct lpfc_sli_ring *pring,
		      struct lpfc_iocbq *prspiocb)
{
	struct lpfc_iocbq *cmd_iocb = NULL;
	uint16_t iotag;

	iotag = prspiocb->iocb.ulpIoTag;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		list_del_init(&cmd_iocb->list);
		if (cmd_iocb->iocb_flag & LPFC_IO_ON_TXCMPLQ) {
			cmd_iocb->iocb_flag &= ~LPFC_IO_ON_TXCMPLQ;
		}
		return cmd_iocb;
	}

	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0317 iotag x%x is out off "
			"range: max iotag x%x wd0 x%x\n",
			iotag, phba->sli.last_iotag,
			*(((uint32_t *) &prspiocb->iocb) + 7));
	return NULL;
}

/**
 * lpfc_sli_iocbq_lookup_by_tag - Find command iocb for the iotag
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @iotag: IOCB tag.
 *
 * This function looks up the iocb_lookup table to get the command iocb
 * corresponding to the given iotag. This function is called with the
 * hbalock held.
 * This function returns the command iocb object if it finds the command
 * iocb else returns NULL.
 **/
static struct lpfc_iocbq *
lpfc_sli_iocbq_lookup_by_tag(struct lpfc_hba *phba,
			     struct lpfc_sli_ring *pring, uint16_t iotag)
{
	struct lpfc_iocbq *cmd_iocb;

	if (iotag != 0 && iotag <= phba->sli.last_iotag) {
		cmd_iocb = phba->sli.iocbq_lookup[iotag];
		if (cmd_iocb->iocb_flag & LPFC_IO_ON_TXCMPLQ) {
			/* remove from txcmpl queue list */
			list_del_init(&cmd_iocb->list);
			cmd_iocb->iocb_flag &= ~LPFC_IO_ON_TXCMPLQ;
			return cmd_iocb;
		}
	}
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0372 iotag x%x is out off range: max iotag (x%x)\n",
			iotag, phba->sli.last_iotag);
	return NULL;
}

/**
 * lpfc_sli_process_sol_iocb - process solicited iocb completion
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @saveq: Pointer to the response iocb to be processed.
 *
 * This function is called by the ring event handler for non-fcp
 * rings when there is a new response iocb in the response ring.
 * The caller is not required to hold any locks. This function
 * gets the command iocb associated with the response iocb and
 * calls the completion handler for the command iocb. If there
 * is no completion handler, the function will free the resources
 * associated with command iocb. If the response iocb is for
 * an already aborted command iocb, the status of the completion
 * is changed to IOSTAT_LOCAL_REJECT/IOERR_SLI_ABORTED.
 * This function always returns 1.
 **/
static int
lpfc_sli_process_sol_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			  struct lpfc_iocbq *saveq)
{
	struct lpfc_iocbq *cmdiocbp;
	int rc = 1;
	unsigned long iflag;

	/* Based on the iotag field, get the cmd IOCB from the txcmplq */
	spin_lock_irqsave(&phba->hbalock, iflag);
	cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring, saveq);
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	if (cmdiocbp) {
		if (cmdiocbp->iocb_cmpl) {
			/*
			 * If an ELS command failed send an event to mgmt
			 * application.
			 */
			if (saveq->iocb.ulpStatus &&
			     (pring->ringno == LPFC_ELS_RING) &&
			     (cmdiocbp->iocb.ulpCommand ==
				CMD_ELS_REQUEST64_CR))
				lpfc_send_els_failure_event(phba,
					cmdiocbp, saveq);

			/*
			 * Post all ELS completions to the worker thread.
			 * All other are passed to the completion callback.
			 */
			if (pring->ringno == LPFC_ELS_RING) {
				if ((phba->sli_rev < LPFC_SLI_REV4) &&
				    (cmdiocbp->iocb_flag &
							LPFC_DRIVER_ABORTED)) {
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
					cmdiocbp->iocb_flag &=
						~LPFC_DRIVER_ABORTED;
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					saveq->iocb.ulpStatus =
						IOSTAT_LOCAL_REJECT;
					saveq->iocb.un.ulpWord[4] =
						IOERR_SLI_ABORTED;

					/* Firmware could still be in progress
					 * of DMAing payload, so don't free data
					 * buffer till after a hbeat.
					 */
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
					saveq->iocb_flag |= LPFC_DELAY_MEM_FREE;
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
				}
				if (phba->sli_rev == LPFC_SLI_REV4) {
					if (saveq->iocb_flag &
					    LPFC_EXCHANGE_BUSY) {
						/* Set cmdiocb flag for the
						 * exchange busy so sgl (xri)
						 * will not be released until
						 * the abort xri is received
						 * from hba.
						 */
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						cmdiocbp->iocb_flag |=
							LPFC_EXCHANGE_BUSY;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
					}
					if (cmdiocbp->iocb_flag &
					    LPFC_DRIVER_ABORTED) {
						/*
						 * Clear LPFC_DRIVER_ABORTED
						 * bit in case it was driver
						 * initiated abort.
						 */
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						cmdiocbp->iocb_flag &=
							~LPFC_DRIVER_ABORTED;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
						cmdiocbp->iocb.ulpStatus =
							IOSTAT_LOCAL_REJECT;
						cmdiocbp->iocb.un.ulpWord[4] =
							IOERR_ABORT_REQUESTED;
						/*
						 * For SLI4, irsiocb contains
						 * NO_XRI in sli_xritag, it
						 * shall not affect releasing
						 * sgl (xri) process.
						 */
						saveq->iocb.ulpStatus =
							IOSTAT_LOCAL_REJECT;
						saveq->iocb.un.ulpWord[4] =
							IOERR_SLI_ABORTED;
						spin_lock_irqsave(
							&phba->hbalock, iflag);
						saveq->iocb_flag |=
							LPFC_DELAY_MEM_FREE;
						spin_unlock_irqrestore(
							&phba->hbalock, iflag);
					}
				}
			}
			(cmdiocbp->iocb_cmpl) (phba, cmdiocbp, saveq);
		} else
			lpfc_sli_release_iocbq(phba, cmdiocbp);
	} else {
		/*
		 * Unknown initiating command based on the response iotag.
		 * This could be the case on the ELS ring because of
		 * lpfc_els_abort().
		 */
		if (pring->ringno != LPFC_ELS_RING) {
			/*
			 * Ring <ringno> handler: unexpected completion IoTag
			 * <IoTag>
			 */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					 "0322 Ring %d handler: "
					 "unexpected completion IoTag x%x "
					 "Data: x%x x%x x%x x%x\n",
					 pring->ringno,
					 saveq->iocb.ulpIoTag,
					 saveq->iocb.ulpStatus,
					 saveq->iocb.un.ulpWord[4],
					 saveq->iocb.ulpCommand,
					 saveq->iocb.ulpContext);
		}
	}

	return rc;
}

/**
 * lpfc_sli_rsp_pointers_error - Response ring pointer error handler
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function is called from the iocb ring event handlers when
 * put pointer is ahead of the get pointer for a ring. This function signal
 * an error attention condition to the worker thread and the worker
 * thread will transition the HBA to offline state.
 **/
static void
lpfc_sli_rsp_pointers_error(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	/*
	 * Ring <ringno> handler: portRspPut <portRspPut> is bigger than
	 * rsp ring <portRspMax>
	 */
	lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0312 Ring %d handler: portRspPut %d "
			"is bigger than rsp ring %d\n",
			pring->ringno, le32_to_cpu(pgp->rspPutInx),
			pring->sli.sli3.numRiocb);

	phba->link_state = LPFC_HBA_ERROR;

	/*
	 * All error attention handlers are posted to
	 * worker thread
	 */
	phba->work_ha |= HA_ERATT;
	phba->work_hs = HS_FFER3;

	lpfc_worker_wake_up(phba);

	return;
}

/**
 * lpfc_poll_eratt - Error attention polling timer timeout handler
 * @ptr: Pointer to address of HBA context object.
 *
 * This function is invoked by the Error Attention polling timer when the
 * timer times out. It will check the SLI Error Attention register for
 * possible attention events. If so, it will post an Error Attention event
 * and wake up worker thread to process it. Otherwise, it will set up the
 * Error Attention polling timer for the next poll.
 **/
void lpfc_poll_eratt(unsigned long ptr)
{
	struct lpfc_hba *phba;
	uint32_t eratt = 0, rem;
	uint64_t sli_intr, cnt;

	phba = (struct lpfc_hba *)ptr;

	/* Here we will also keep track of interrupts per sec of the hba */
	sli_intr = phba->sli.slistat.sli_intr;

	if (phba->sli.slistat.sli_prev_intr > sli_intr)
		cnt = (((uint64_t)(-1) - phba->sli.slistat.sli_prev_intr) +
			sli_intr);
	else
		cnt = (sli_intr - phba->sli.slistat.sli_prev_intr);

	/* 64-bit integer division not supporte on 32-bit x86 - use do_div */
	rem = do_div(cnt, LPFC_ERATT_POLL_INTERVAL);
	phba->sli.slistat.sli_ips = cnt;

	phba->sli.slistat.sli_prev_intr = sli_intr;

	/* Check chip HA register for error event */
	eratt = lpfc_sli_check_eratt(phba);

	if (eratt)
		/* Tell the worker thread there is work to do */
		lpfc_worker_wake_up(phba);
	else
		/* Restart the timer for next eratt poll */
		mod_timer(&phba->eratt_poll,
			  jiffies +
			  msecs_to_jiffies(1000 * LPFC_ERATT_POLL_INTERVAL));
	return;
}


/**
 * lpfc_sli_handle_fast_ring_event - Handle ring events on FCP ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the interrupt context when there is a ring
 * event for the fcp ring. The caller does not hold any lock.
 * The function processes each response iocb in the response ring until it
 * finds an iocb with LE bit set and chains all the iocbs up to the iocb with
 * LE bit set. The function will call the completion handler of the command iocb
 * if the response iocb indicates a completion for a command iocb or it is
 * an abort completion. The function will call lpfc_sli_process_unsol_iocb
 * function if this is an unsolicited iocb.
 * This routine presumes LPFC_FCP_RING handling and doesn't bother
 * to check it explicitly.
 */
int
lpfc_sli_handle_fast_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp = &phba->port_gp[pring->ringno];
	IOCB_t *irsp = NULL;
	IOCB_t *entry = NULL;
	struct lpfc_iocbq *cmdiocbq = NULL;
	struct lpfc_iocbq rspiocbq;
	uint32_t status;
	uint32_t portRspPut, portRspMax;
	int rc = 1;
	lpfc_iocb_type type;
	unsigned long iflag;
	uint32_t rsp_cmpl = 0;

	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->sli.sli3.numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (unlikely(portRspPut >= portRspMax)) {
		lpfc_sli_rsp_pointers_error(phba, pring);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	}
	if (phba->fcp_ring_in_use) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		return 1;
	} else
		phba->fcp_ring_in_use = 1;

	rmb();
	while (pring->sli.sli3.rspidx != portRspPut) {
		/*
		 * Fetch an entry off the ring and copy it into a local data
		 * structure.  The copy involves a byte-swap since the
		 * network byte order and pci byte orders are different.
		 */
		entry = lpfc_resp_iocb(phba, pring);
		phba->last_completion_time = jiffies;

		if (++pring->sli.sli3.rspidx >= portRspMax)
			pring->sli.sli3.rspidx = 0;

		lpfc_sli_pcimem_bcopy((uint32_t *) entry,
				      (uint32_t *) &rspiocbq.iocb,
				      phba->iocb_rsp_size);
		INIT_LIST_HEAD(&(rspiocbq.list));
		irsp = &rspiocbq.iocb;

		type = lpfc_sli_iocb_cmd_type(irsp->ulpCommand & CMD_IOCB_MASK);
		pring->stats.iocb_rsp++;
		rsp_cmpl++;

		if (unlikely(irsp->ulpStatus)) {
			/*
			 * If resource errors reported from HBA, reduce
			 * queuedepths of the SCSI device.
			 */
			if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
			    ((irsp->un.ulpWord[4] & IOERR_PARAM_MASK) ==
			     IOERR_NO_RESOURCES)) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->lpfc_rampdown_queue_depth(phba);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}

			/* Rsp ring <ringno> error: IOCB */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0336 Rsp Ring %d error: IOCB Data: "
					"x%x x%x x%x x%x x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(uint32_t *)&irsp->un1,
					*((uint32_t *)&irsp->un1 + 1));
		}

		switch (type) {
		case LPFC_ABORT_IOCB:
		case LPFC_SOL_IOCB:
			/*
			 * Idle exchange closed via ABTS from port.  No iocb
			 * resources need to be recovered.
			 */
			if (unlikely(irsp->ulpCommand == CMD_XRI_ABORTED_CX)) {
				lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
						"0333 IOCB cmd 0x%x"
						" processed. Skipping"
						" completion\n",
						irsp->ulpCommand);
				break;
			}

			cmdiocbq = lpfc_sli_iocbq_lookup(phba, pring,
							 &rspiocbq);
			if (unlikely(!cmdiocbq))
				break;
			if (cmdiocbq->iocb_flag & LPFC_DRIVER_ABORTED)
				cmdiocbq->iocb_flag &= ~LPFC_DRIVER_ABORTED;
			if (cmdiocbq->iocb_cmpl) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				(cmdiocbq->iocb_cmpl)(phba, cmdiocbq,
						      &rspiocbq);
				spin_lock_irqsave(&phba->hbalock, iflag);
			}
			break;
		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			lpfc_sli_process_unsol_iocb(phba, pring, &rspiocbq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;
		default:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *) irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				/* Unknown IOCB command */
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0334 Unknown IOCB command "
						"Data: x%x, x%x x%x x%x x%x\n",
						type, irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		/*
		 * The response IOCB has been processed.  Update the ring
		 * pointer in SLIM.  If the port response put pointer has not
		 * been updated, sync the pgp->rspPutInx and fetch the new port
		 * response put pointer.
		 */
		writel(pring->sli.sli3.rspidx,
			&phba->host_gp[pring->ringno].rspGetInx);

		if (pring->sli.sli3.rspidx == portRspPut)
			portRspPut = le32_to_cpu(pgp->rspPutInx);
	}

	if ((rsp_cmpl > 0) && (mask & HA_R0RE_REQ)) {
		pring->stats.iocb_rsp_full++;
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr);
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/* Force update of the local copy of cmdGetInx */
		pring->sli.sli3.local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	phba->fcp_ring_in_use = 0;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

/**
 * lpfc_sli_sp_handle_rspiocb - Handle slow-path response iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @rspiocbp: Pointer to driver response IOCB object.
 *
 * This function is called from the worker thread when there is a slow-path
 * response IOCB to process. This function chains all the response iocbs until
 * seeing the iocb with the LE bit set. The function will call
 * lpfc_sli_process_sol_iocb function if the response iocb indicates a
 * completion of a command iocb. The function will call the
 * lpfc_sli_process_unsol_iocb function if this is an unsolicited iocb.
 * The function frees the resources or calls the completion handler if this
 * iocb is an abort completion. The function returns NULL when the response
 * iocb has the LE bit set and all the chained iocbs are processed, otherwise
 * this function shall chain the iocb on to the iocb_continueq and return the
 * response iocb passed in.
 **/
static struct lpfc_iocbq *
lpfc_sli_sp_handle_rspiocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			struct lpfc_iocbq *rspiocbp)
{
	struct lpfc_iocbq *saveq;
	struct lpfc_iocbq *cmdiocbp;
	struct lpfc_iocbq *next_iocb;
	IOCB_t *irsp = NULL;
	uint32_t free_saveq;
	uint8_t iocb_cmd_type;
	lpfc_iocb_type type;
	unsigned long iflag;
	int rc;

	spin_lock_irqsave(&phba->hbalock, iflag);
	/* First add the response iocb to the countinueq list */
	list_add_tail(&rspiocbp->list, &(pring->iocb_continueq));
	pring->iocb_continueq_cnt++;

	/* Now, determine whether the list is completed for processing */
	irsp = &rspiocbp->iocb;
	if (irsp->ulpLe) {
		/*
		 * By default, the driver expects to free all resources
		 * associated with this iocb completion.
		 */
		free_saveq = 1;
		saveq = list_get_first(&pring->iocb_continueq,
				       struct lpfc_iocbq, list);
		irsp = &(saveq->iocb);
		list_del_init(&pring->iocb_continueq);
		pring->iocb_continueq_cnt = 0;

		pring->stats.iocb_rsp++;

		/*
		 * If resource errors reported from HBA, reduce
		 * queuedepths of the SCSI device.
		 */
		if ((irsp->ulpStatus == IOSTAT_LOCAL_REJECT) &&
		    ((irsp->un.ulpWord[4] & IOERR_PARAM_MASK) ==
		     IOERR_NO_RESOURCES)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			phba->lpfc_rampdown_queue_depth(phba);
			spin_lock_irqsave(&phba->hbalock, iflag);
		}

		if (irsp->ulpStatus) {
			/* Rsp ring <ringno> error: IOCB */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0328 Rsp Ring %d error: "
					"IOCB Data: "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x "
					"x%x x%x x%x x%x\n",
					pring->ringno,
					irsp->un.ulpWord[0],
					irsp->un.ulpWord[1],
					irsp->un.ulpWord[2],
					irsp->un.ulpWord[3],
					irsp->un.ulpWord[4],
					irsp->un.ulpWord[5],
					*(((uint32_t *) irsp) + 6),
					*(((uint32_t *) irsp) + 7),
					*(((uint32_t *) irsp) + 8),
					*(((uint32_t *) irsp) + 9),
					*(((uint32_t *) irsp) + 10),
					*(((uint32_t *) irsp) + 11),
					*(((uint32_t *) irsp) + 12),
					*(((uint32_t *) irsp) + 13),
					*(((uint32_t *) irsp) + 14),
					*(((uint32_t *) irsp) + 15));
		}

		/*
		 * Fetch the IOCB command type and call the correct completion
		 * routine. Solicited and Unsolicited IOCBs on the ELS ring
		 * get freed back to the lpfc_iocb_list by the discovery
		 * kernel thread.
		 */
		iocb_cmd_type = irsp->ulpCommand & CMD_IOCB_MASK;
		type = lpfc_sli_iocb_cmd_type(iocb_cmd_type);
		switch (type) {
		case LPFC_SOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_sol_iocb(phba, pring, saveq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			break;

		case LPFC_UNSOL_IOCB:
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			rc = lpfc_sli_process_unsol_iocb(phba, pring, saveq);
			spin_lock_irqsave(&phba->hbalock, iflag);
			if (!rc)
				free_saveq = 0;
			break;

		case LPFC_ABORT_IOCB:
			cmdiocbp = NULL;
			if (irsp->ulpCommand != CMD_XRI_ABORTED_CX)
				cmdiocbp = lpfc_sli_iocbq_lookup(phba, pring,
								 saveq);
			if (cmdiocbp) {
				/* Call the specified completion routine */
				if (cmdiocbp->iocb_cmpl) {
					spin_unlock_irqrestore(&phba->hbalock,
							       iflag);
					(cmdiocbp->iocb_cmpl)(phba, cmdiocbp,
							      saveq);
					spin_lock_irqsave(&phba->hbalock,
							  iflag);
				} else
					__lpfc_sli_release_iocbq(phba,
								 cmdiocbp);
			}
			break;

		case LPFC_UNKNOWN_IOCB:
			if (irsp->ulpCommand == CMD_ADAPTER_MSG) {
				char adaptermsg[LPFC_MAX_ADPTMSG];
				memset(adaptermsg, 0, LPFC_MAX_ADPTMSG);
				memcpy(&adaptermsg[0], (uint8_t *)irsp,
				       MAX_MSG_DATA);
				dev_warn(&((phba->pcidev)->dev),
					 "lpfc%d: %s\n",
					 phba->brd_no, adaptermsg);
			} else {
				/* Unknown IOCB command */
				lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
						"0335 Unknown IOCB "
						"command Data: x%x "
						"x%x x%x x%x\n",
						irsp->ulpCommand,
						irsp->ulpStatus,
						irsp->ulpIoTag,
						irsp->ulpContext);
			}
			break;
		}

		if (free_saveq) {
			list_for_each_entry_safe(rspiocbp, next_iocb,
						 &saveq->list, list) {
				list_del(&rspiocbp->list);
				__lpfc_sli_release_iocbq(phba, rspiocbp);
			}
			__lpfc_sli_release_iocbq(phba, saveq);
		}
		rspiocbp = NULL;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rspiocbp;
}

/**
 * lpfc_sli_handle_slow_ring_event - Wrapper func for handling slow-path iocbs
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This routine wraps the actual slow_ring event process routine from the
 * API jump table function pointer from the lpfc_hba struct.
 **/
void
lpfc_sli_handle_slow_ring_event(struct lpfc_hba *phba,
				struct lpfc_sli_ring *pring, uint32_t mask)
{
	phba->lpfc_sli_handle_slow_ring_event(phba, pring, mask);
}

/**
 * lpfc_sli_handle_slow_ring_event_s3 - Handle SLI3 ring event for non-FCP rings
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the worker thread when there is a ring event
 * for non-fcp rings. The caller does not hold any lock. The function will
 * remove each response iocb in the response ring and calls the handle
 * response iocb routine (lpfc_sli_sp_handle_rspiocb) to process it.
 **/
static void
lpfc_sli_handle_slow_ring_event_s3(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_pgp *pgp;
	IOCB_t *entry;
	IOCB_t *irsp = NULL;
	struct lpfc_iocbq *rspiocbp = NULL;
	uint32_t portRspPut, portRspMax;
	unsigned long iflag;
	uint32_t status;

	pgp = &phba->port_gp[pring->ringno];
	spin_lock_irqsave(&phba->hbalock, iflag);
	pring->stats.iocb_event++;

	/*
	 * The next available response entry should never exceed the maximum
	 * entries.  If it does, treat it as an adapter hardware error.
	 */
	portRspMax = pring->sli.sli3.numRiocb;
	portRspPut = le32_to_cpu(pgp->rspPutInx);
	if (portRspPut >= portRspMax) {
		/*
		 * Ring <ringno> handler: portRspPut <portRspPut> is bigger than
		 * rsp ring <portRspMax>
		 */
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0303 Ring %d handler: portRspPut %d "
				"is bigger than rsp ring %d\n",
				pring->ringno, portRspPut, portRspMax);

		phba->link_state = LPFC_HBA_ERROR;
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		phba->work_hs = HS_FFER3;
		lpfc_handle_eratt(phba);

		return;
	}

	rmb();
	while (pring->sli.sli3.rspidx != portRspPut) {
		/*
		 * Build a completion list and call the appropriate handler.
		 * The process is to get the next available response iocb, get
		 * a free iocb from the list, copy the response data into the
		 * free iocb, insert to the continuation list, and update the
		 * next response index to slim.  This process makes response
		 * iocb's in the ring available to DMA as fast as possible but
		 * pays a penalty for a copy operation.  Since the iocb is
		 * only 32 bytes, this penalty is considered small relative to
		 * the PCI reads for register values and a slim write.  When
		 * the ulpLe field is set, the entire Command has been
		 * received.
		 */
		entry = lpfc_resp_iocb(phba, pring);

		phba->last_completion_time = jiffies;
		rspiocbp = __lpfc_sli_get_iocbq(phba);
		if (rspiocbp == NULL) {
			printk(KERN_ERR "%s: out of buffers! Failing "
			       "completion.\n", __func__);
			break;
		}

		lpfc_sli_pcimem_bcopy(entry, &rspiocbp->iocb,
				      phba->iocb_rsp_size);
		irsp = &rspiocbp->iocb;

		if (++pring->sli.sli3.rspidx >= portRspMax)
			pring->sli.sli3.rspidx = 0;

		if (pring->ringno == LPFC_ELS_RING) {
			lpfc_debugfs_slow_ring_trc(phba,
			"IOCB rsp ring:   wd4:x%08x wd6:x%08x wd7:x%08x",
				*(((uint32_t *) irsp) + 4),
				*(((uint32_t *) irsp) + 6),
				*(((uint32_t *) irsp) + 7));
		}

		writel(pring->sli.sli3.rspidx,
			&phba->host_gp[pring->ringno].rspGetInx);

		spin_unlock_irqrestore(&phba->hbalock, iflag);
		/* Handle the response IOCB */
		rspiocbp = lpfc_sli_sp_handle_rspiocb(phba, pring, rspiocbp);
		spin_lock_irqsave(&phba->hbalock, iflag);

		/*
		 * If the port response put pointer has not been updated, sync
		 * the pgp->rspPutInx in the MAILBOX_tand fetch the new port
		 * response put pointer.
		 */
		if (pring->sli.sli3.rspidx == portRspPut) {
			portRspPut = le32_to_cpu(pgp->rspPutInx);
		}
	} /* while (pring->sli.sli3.rspidx != portRspPut) */

	if ((rspiocbp != NULL) && (mask & HA_R0RE_REQ)) {
		/* At least one response entry has been freed */
		pring->stats.iocb_rsp_full++;
		/* SET RxRE_RSP in Chip Att register */
		status = ((CA_R0ATT | CA_R0RE_RSP) << (pring->ringno * 4));
		writel(status, phba->CAregaddr);
		readl(phba->CAregaddr); /* flush */
	}
	if ((mask & HA_R0CE_RSP) && (pring->flag & LPFC_CALL_RING_AVAILABLE)) {
		pring->flag &= ~LPFC_CALL_RING_AVAILABLE;
		pring->stats.iocb_cmd_empty++;

		/* Force update of the local copy of cmdGetInx */
		pring->sli.sli3.local_getidx = le32_to_cpu(pgp->cmdGetInx);
		lpfc_sli_resume_iocb(phba, pring);

		if ((pring->lpfc_sli_cmd_available))
			(pring->lpfc_sli_cmd_available) (phba, pring);

	}

	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return;
}

/**
 * lpfc_sli_handle_slow_ring_event_s4 - Handle SLI4 slow-path els events
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mask: Host attention register mask for this ring.
 *
 * This function is called from the worker thread when there is a pending
 * ELS response iocb on the driver internal slow-path response iocb worker
 * queue. The caller does not hold any lock. The function will remove each
 * response iocb from the response worker queue and calls the handle
 * response iocb routine (lpfc_sli_sp_handle_rspiocb) to process it.
 **/
static void
lpfc_sli_handle_slow_ring_event_s4(struct lpfc_hba *phba,
				   struct lpfc_sli_ring *pring, uint32_t mask)
{
	struct lpfc_iocbq *irspiocbq;
	struct hbq_dmabuf *dmabuf;
	struct lpfc_cq_event *cq_event;
	unsigned long iflag;

	spin_lock_irqsave(&phba->hbalock, iflag);
	phba->hba_flag &= ~HBA_SP_QUEUE_EVT;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	while (!list_empty(&phba->sli4_hba.sp_queue_event)) {
		/* Get the response iocb from the head of work queue */
		spin_lock_irqsave(&phba->hbalock, iflag);
		list_remove_head(&phba->sli4_hba.sp_queue_event,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irqrestore(&phba->hbalock, iflag);

		switch (bf_get(lpfc_wcqe_c_code, &cq_event->cqe.wcqe_cmpl)) {
		case CQE_CODE_COMPL_WQE:
			irspiocbq = container_of(cq_event, struct lpfc_iocbq,
						 cq_event);
			/* Translate ELS WCQE to response IOCBQ */
			irspiocbq = lpfc_sli4_els_wcqe_to_rspiocbq(phba,
								   irspiocbq);
			if (irspiocbq)
				lpfc_sli_sp_handle_rspiocb(phba, pring,
							   irspiocbq);
			break;
		case CQE_CODE_RECEIVE:
		case CQE_CODE_RECEIVE_V1:
			dmabuf = container_of(cq_event, struct hbq_dmabuf,
					      cq_event);
			lpfc_sli4_handle_received_buffer(phba, dmabuf);
			break;
		default:
			break;
		}
	}
}

/**
 * lpfc_sli_abort_iocb_ring - Abort all iocbs in the ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function aborts all iocbs in the given ring and frees all the iocb
 * objects in txq. This function issues an abort iocb for all the iocb commands
 * in txcmplq. The iocbs in the txcmplq is not guaranteed to complete before
 * the return of this function. The caller is not required to hold any locks.
 **/
void
lpfc_sli_abort_iocb_ring(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq *iocb, *next_iocb;

	if (pring->ringno == LPFC_ELS_RING) {
		lpfc_fabric_abort_hba(phba);
	}

	/* Error everything on txq and txcmplq
	 * First do the txq.
	 */
	spin_lock_irq(&phba->hbalock);
	list_splice_init(&pring->txq, &completions);

	/* Next issue ABTS for everything on the txcmplq */
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list)
		lpfc_sli_issue_abort_iotag(phba, pring, iocb);

	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

/**
 * lpfc_sli_flush_fcp_rings - flush all iocbs in the fcp ring
 * @phba: Pointer to HBA context object.
 *
 * This function flushes all iocbs in the fcp ring and frees all the iocb
 * objects in txq and txcmplq. This function will not issue abort iocbs
 * for all the iocb commands in txcmplq, they will just be returned with
 * IOERR_SLI_DOWN. This function is invoked with EEH when device's PCI
 * slot has been permanently disabled.
 **/
void
lpfc_sli_flush_fcp_rings(struct lpfc_hba *phba)
{
	LIST_HEAD(txq);
	LIST_HEAD(txcmplq);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring  *pring;

	/* Currently, only one fcp ring */
	pring = &psli->ring[psli->fcp_ring];

	spin_lock_irq(&phba->hbalock);
	/* Retrieve everything on txq */
	list_splice_init(&pring->txq, &txq);

	/* Retrieve everything on the txcmplq */
	list_splice_init(&pring->txcmplq, &txcmplq);

	/* Indicate the I/O queues are flushed */
	phba->hba_flag |= HBA_FCP_IOQ_FLUSH;
	spin_unlock_irq(&phba->hbalock);

	/* Flush the txq */
	lpfc_sli_cancel_iocbs(phba, &txq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

	/* Flush the txcmpq */
	lpfc_sli_cancel_iocbs(phba, &txcmplq, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
}

/**
 * lpfc_sli_brdready_s3 - Check for sli3 host ready status
 * @phba: Pointer to HBA context object.
 * @mask: Bit mask to be checked.
 *
 * This function reads the host status register and compares
 * with the provided bit mask to check if HBA completed
 * the restart. This function will wait in a loop for the
 * HBA to complete restart. If the HBA does not restart within
 * 15 iterations, the function will reset the HBA again. The
 * function returns 1 when HBA fail to restart otherwise returns
 * zero.
 **/
static int
lpfc_sli_brdready_s3(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int i = 0;
	int retval = 0;

	/* Read the HBA Host Status Register */
	if (lpfc_readl(phba->HSregaddr, &status))
		return 1;

	/*
	 * Check status register every 100ms for 5 retries, then every
	 * 500ms for 5, then every 2.5 sec for 5, then reset board and
	 * every 2.5 sec for 4.
	 * Break our of the loop if errors occurred during init.
	 */
	while (((status & mask) != mask) &&
	       !(status & HS_FFERM) &&
	       i++ < 20) {

		if (i <= 5)
			msleep(10);
		else if (i <= 10)
			msleep(500);
		else
			msleep(2500);

		if (i == 15) {
				/* Do post */
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		/* Read the HBA Host Status Register */
		if (lpfc_readl(phba->HSregaddr, &status)) {
			retval = 1;
			break;
		}
	}

	/* Check to see if any errors occurred during init */
	if ((status & HS_FFERM) || (i >= 20)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2751 Adapter failed to restart, "
				"status reg x%x, FW Data: A8 x%x AC x%x\n",
				status,
				readl(phba->MBslimaddr + 0xa8),
				readl(phba->MBslimaddr + 0xac));
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	}

	return retval;
}

/**
 * lpfc_sli_brdready_s4 - Check for sli4 host ready status
 * @phba: Pointer to HBA context object.
 * @mask: Bit mask to be checked.
 *
 * This function checks the host status register to check if HBA is
 * ready. This function will wait in a loop for the HBA to be ready
 * If the HBA is not ready , the function will will reset the HBA PCI
 * function again. The function returns 1 when HBA fail to be ready
 * otherwise returns zero.
 **/
static int
lpfc_sli_brdready_s4(struct lpfc_hba *phba, uint32_t mask)
{
	uint32_t status;
	int retval = 0;

	/* Read the HBA Host Status Register */
	status = lpfc_sli4_post_status_check(phba);

	if (status) {
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		status = lpfc_sli4_post_status_check(phba);
	}

	/* Check to see if any errors occurred during init */
	if (status) {
		phba->link_state = LPFC_HBA_ERROR;
		retval = 1;
	} else
		phba->sli4_hba.intr_enable = 0;

	return retval;
}

/**
 * lpfc_sli_brdready - Wrapper func for checking the hba readyness
 * @phba: Pointer to HBA context object.
 * @mask: Bit mask to be checked.
 *
 * This routine wraps the actual SLI3 or SLI4 hba readyness check routine
 * from the API jump table function pointer from the lpfc_hba struct.
 **/
int
lpfc_sli_brdready(struct lpfc_hba *phba, uint32_t mask)
{
	return phba->lpfc_sli_brdready(phba, mask);
}

#define BARRIER_TEST_PATTERN (0xdeadbeef)

/**
 * lpfc_reset_barrier - Make HBA ready for HBA reset
 * @phba: Pointer to HBA context object.
 *
 * This function is called before resetting an HBA. This function is called
 * with hbalock held and requests HBA to quiesce DMAs before a reset.
 **/
void lpfc_reset_barrier(struct lpfc_hba *phba)
{
	uint32_t __iomem *resp_buf;
	uint32_t __iomem *mbox_buf;
	volatile uint32_t mbox;
	uint32_t hc_copy, ha_copy, resp_data;
	int  i;
	uint8_t hdrtype;

	pci_read_config_byte(phba->pcidev, PCI_HEADER_TYPE, &hdrtype);
	if (hdrtype != 0x80 ||
	    (FC_JEDEC_ID(phba->vpd.rev.biuRev) != HELIOS_JEDEC_ID &&
	     FC_JEDEC_ID(phba->vpd.rev.biuRev) != THOR_JEDEC_ID))
		return;

	/*
	 * Tell the other part of the chip to suspend temporarily all
	 * its DMA activity.
	 */
	resp_buf = phba->MBslimaddr;

	/* Disable the error attention */
	if (lpfc_readl(phba->HCregaddr, &hc_copy))
		return;
	writel((hc_copy & ~HC_ERINT_ENA), phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	phba->link_flag |= LS_IGNORE_ERATT;

	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return;
	if (ha_copy & HA_ERATT) {
		/* Clear Chip error bit */
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

	mbox = 0;
	((MAILBOX_t *)&mbox)->mbxCommand = MBX_KILL_BOARD;
	((MAILBOX_t *)&mbox)->mbxOwner = OWN_CHIP;

	writel(BARRIER_TEST_PATTERN, (resp_buf + 1));
	mbox_buf = phba->MBslimaddr;
	writel(mbox, mbox_buf);

	for (i = 0; i < 50; i++) {
		if (lpfc_readl((resp_buf + 1), &resp_data))
			return;
		if (resp_data != ~(BARRIER_TEST_PATTERN))
			mdelay(1);
		else
			break;
	}
	resp_data = 0;
	if (lpfc_readl((resp_buf + 1), &resp_data))
		return;
	if (resp_data  != ~(BARRIER_TEST_PATTERN)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE ||
		    phba->pport->stopped)
			goto restore_hc;
		else
			goto clear_errat;
	}

	((MAILBOX_t *)&mbox)->mbxOwner = OWN_HOST;
	resp_data = 0;
	for (i = 0; i < 500; i++) {
		if (lpfc_readl(resp_buf, &resp_data))
			return;
		if (resp_data != mbox)
			mdelay(1);
		else
			break;
	}

clear_errat:

	while (++i < 500) {
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return;
		if (!(ha_copy & HA_ERATT))
			mdelay(1);
		else
			break;
	}

	if (readl(phba->HAregaddr) & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}

restore_hc:
	phba->link_flag &= ~LS_IGNORE_ERATT;
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
}

/**
 * lpfc_sli_brdkill - Issue a kill_board mailbox command
 * @phba: Pointer to HBA context object.
 *
 * This function issues a kill_board mailbox command and waits for
 * the error attention interrupt. This function is called for stopping
 * the firmware processing. The caller is not required to hold any
 * locks. This function calls lpfc_hba_down_post function to free
 * any pending commands after the kill. The function will return 1 when it
 * fails to kill the board else will return 0.
 **/
int
lpfc_sli_brdkill(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	LPFC_MBOXQ_t *pmb;
	uint32_t status;
	uint32_t ha_copy;
	int retval;
	int i = 0;

	psli = &phba->sli;

	/* Kill HBA */
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0329 Kill HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb)
		return 1;

	/* Disable the error attention */
	spin_lock_irq(&phba->hbalock);
	if (lpfc_readl(phba->HCregaddr, &status)) {
		spin_unlock_irq(&phba->hbalock);
		mempool_free(pmb, phba->mbox_mem_pool);
		return 1;
	}
	status &= ~HC_ERINT_ENA;
	writel(status, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */
	phba->link_flag |= LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_kill_board(phba, pmb);
	pmb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	retval = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);

	if (retval != MBX_SUCCESS) {
		if (retval != MBX_BUSY)
			mempool_free(pmb, phba->mbox_mem_pool);
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2752 KILL_BOARD command failed retval %d\n",
				retval);
		spin_lock_irq(&phba->hbalock);
		phba->link_flag &= ~LS_IGNORE_ERATT;
		spin_unlock_irq(&phba->hbalock);
		return 1;
	}

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	mempool_free(pmb, phba->mbox_mem_pool);

	/* There is no completion for a KILL_BOARD mbox cmd. Check for an error
	 * attention every 100ms for 3 seconds. If we don't get ERATT after
	 * 3 seconds we still set HBA_ERROR state because the status of the
	 * board is now undefined.
	 */
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		return 1;
	while ((i++ < 30) && !(ha_copy & HA_ERATT)) {
		mdelay(100);
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return 1;
	}

	del_timer_sync(&psli->mbox_tmo);
	if (ha_copy & HA_ERATT) {
		writel(HA_ERATT, phba->HAregaddr);
		phba->pport->stopped = 1;
	}
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	psli->mbox_active = NULL;
	phba->link_flag &= ~LS_IGNORE_ERATT;
	spin_unlock_irq(&phba->hbalock);

	lpfc_hba_down_post(phba);
	phba->link_state = LPFC_HBA_ERROR;

	return ha_copy & HA_ERATT ? 0 : 1;
}

/**
 * lpfc_sli_brdreset - Reset a sli-2 or sli-3 HBA
 * @phba: Pointer to HBA context object.
 *
 * This function resets the HBA by writing HC_INITFF to the control
 * register. After the HBA resets, this function resets all the iocb ring
 * indices. This function disables PCI layer parity checking during
 * the reset.
 * This function returns 0 always.
 * The caller is not required to hold any locks.
 **/
int
lpfc_sli_brdreset(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	uint16_t cfg_value;
	int i;

	psli = &phba->sli;

	/* Reset HBA */
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0325 Reset HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	/* perform board reset */
	phba->fc_eventTag = 0;
	phba->link_events = 0;
	phba->pport->fc_myDID = 0;
	phba->pport->fc_prevDID = 0;

	/* Turn off parity checking and serr during the physical reset */
	pci_read_config_word(phba->pcidev, PCI_COMMAND, &cfg_value);
	pci_write_config_word(phba->pcidev, PCI_COMMAND,
			      (cfg_value &
			       ~(PCI_COMMAND_PARITY | PCI_COMMAND_SERR)));

	psli->sli_flag &= ~(LPFC_SLI_ACTIVE | LPFC_PROCESS_LA);

	/* Now toggle INITFF bit in the Host Control Register */
	writel(HC_INITFF, phba->HCregaddr);
	mdelay(1);
	readl(phba->HCregaddr); /* flush */
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */

	/* Restore PCI cmd register */
	pci_write_config_word(phba->pcidev, PCI_COMMAND, cfg_value);

	/* Initialize relevant SLI info */
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		pring->flag = 0;
		pring->sli.sli3.rspidx = 0;
		pring->sli.sli3.next_cmdidx  = 0;
		pring->sli.sli3.local_getidx = 0;
		pring->sli.sli3.cmdidx = 0;
		pring->missbufcnt = 0;
	}

	phba->link_state = LPFC_WARM_START;
	return 0;
}

/**
 * lpfc_sli4_brdreset - Reset a sli-4 HBA
 * @phba: Pointer to HBA context object.
 *
 * This function resets a SLI4 HBA. This function disables PCI layer parity
 * checking during resets the device. The caller is not required to hold
 * any locks.
 *
 * This function returns 0 always.
 **/
int
lpfc_sli4_brdreset(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	uint16_t cfg_value;
	int rc;

	/* Reset HBA */
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0295 Reset HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	/* perform board reset */
	phba->fc_eventTag = 0;
	phba->link_events = 0;
	phba->pport->fc_myDID = 0;
	phba->pport->fc_prevDID = 0;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag &= ~(LPFC_PROCESS_LA);
	phba->fcf.fcf_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	/* Now physically reset the device */
	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0389 Performing PCI function reset!\n");

	/* Turn off parity checking and serr during the physical reset */
	pci_read_config_word(phba->pcidev, PCI_COMMAND, &cfg_value);
	pci_write_config_word(phba->pcidev, PCI_COMMAND, (cfg_value &
			      ~(PCI_COMMAND_PARITY | PCI_COMMAND_SERR)));

	/* Perform FCoE PCI function reset before freeing queue memory */
	rc = lpfc_pci_function_reset(phba);
	lpfc_sli4_queue_destroy(phba);

	/* Restore PCI cmd register */
	pci_write_config_word(phba->pcidev, PCI_COMMAND, cfg_value);

	return rc;
}

/**
 * lpfc_sli_brdrestart_s3 - Restart a sli-3 hba
 * @phba: Pointer to HBA context object.
 *
 * This function is called in the SLI initialization code path to
 * restart the HBA. The caller is not required to hold any lock.
 * This function writes MBX_RESTART mailbox command to the SLIM and
 * resets the HBA. At the end of the function, it calls lpfc_hba_down_post
 * function to free any pending commands. The function enables
 * POST only during the first initialization. The function returns zero.
 * The function does not guarantee completion of MBX_RESTART mailbox
 * command before the return of this function.
 **/
static int
lpfc_sli_brdrestart_s3(struct lpfc_hba *phba)
{
	MAILBOX_t *mb;
	struct lpfc_sli *psli;
	volatile uint32_t word0;
	void __iomem *to_slim;
	uint32_t hba_aer_enabled;

	spin_lock_irq(&phba->hbalock);

	/* Take PCIe device Advanced Error Reporting (AER) state */
	hba_aer_enabled = phba->hba_flag & HBA_AER_ENABLED;

	psli = &phba->sli;

	/* Restart HBA */
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0337 Restart HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	word0 = 0;
	mb = (MAILBOX_t *) &word0;
	mb->mbxCommand = MBX_RESTART;
	mb->mbxHc = 1;

	lpfc_reset_barrier(phba);

	to_slim = phba->MBslimaddr;
	writel(*(uint32_t *) mb, to_slim);
	readl(to_slim); /* flush */

	/* Only skip post after fc_ffinit is completed */
	if (phba->pport->port_state)
		word0 = 1;	/* This is really setting up word1 */
	else
		word0 = 0;	/* This is really setting up word1 */
	to_slim = phba->MBslimaddr + sizeof (uint32_t);
	writel(*(uint32_t *) mb, to_slim);
	readl(to_slim); /* flush */

	lpfc_sli_brdreset(phba);
	phba->pport->stopped = 0;
	phba->link_state = LPFC_INIT_START;
	phba->hba_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	memset(&psli->lnk_stat_offsets, 0, sizeof(psli->lnk_stat_offsets));
	psli->stats_start = get_seconds();

	/* Give the INITFF and Post time to settle. */
	mdelay(100);

	/* Reset HBA AER if it was enabled, note hba_flag was reset above */
	if (hba_aer_enabled)
		pci_disable_pcie_error_reporting(phba->pcidev);

	lpfc_hba_down_post(phba);

	return 0;
}

/**
 * lpfc_sli_brdrestart_s4 - Restart the sli-4 hba
 * @phba: Pointer to HBA context object.
 *
 * This function is called in the SLI initialization code path to restart
 * a SLI4 HBA. The caller is not required to hold any lock.
 * At the end of the function, it calls lpfc_hba_down_post function to
 * free any pending commands.
 **/
static int
lpfc_sli_brdrestart_s4(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	uint32_t hba_aer_enabled;
	int rc;

	/* Restart HBA */
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0296 Restart HBA Data: x%x x%x\n",
			phba->pport->port_state, psli->sli_flag);

	/* Take PCIe device Advanced Error Reporting (AER) state */
	hba_aer_enabled = phba->hba_flag & HBA_AER_ENABLED;

	rc = lpfc_sli4_brdreset(phba);

	spin_lock_irq(&phba->hbalock);
	phba->pport->stopped = 0;
	phba->link_state = LPFC_INIT_START;
	phba->hba_flag = 0;
	spin_unlock_irq(&phba->hbalock);

	memset(&psli->lnk_stat_offsets, 0, sizeof(psli->lnk_stat_offsets));
	psli->stats_start = get_seconds();

	/* Reset HBA AER if it was enabled, note hba_flag was reset above */
	if (hba_aer_enabled)
		pci_disable_pcie_error_reporting(phba->pcidev);

	lpfc_hba_down_post(phba);

	return rc;
}

/**
 * lpfc_sli_brdrestart - Wrapper func for restarting hba
 * @phba: Pointer to HBA context object.
 *
 * This routine wraps the actual SLI3 or SLI4 hba restart routine from the
 * API jump table function pointer from the lpfc_hba struct.
**/
int
lpfc_sli_brdrestart(struct lpfc_hba *phba)
{
	return phba->lpfc_sli_brdrestart(phba);
}

/**
 * lpfc_sli_chipset_init - Wait for the restart of the HBA after a restart
 * @phba: Pointer to HBA context object.
 *
 * This function is called after a HBA restart to wait for successful
 * restart of the HBA. Successful restart of the HBA is indicated by
 * HS_FFRDY and HS_MBRDY bits. If the HBA fails to restart even after 15
 * iteration, the function will restart the HBA again. The function returns
 * zero if HBA successfully restarted else returns negative error code.
 **/
static int
lpfc_sli_chipset_init(struct lpfc_hba *phba)
{
	uint32_t status, i = 0;

	/* Read the HBA Host Status Register */
	if (lpfc_readl(phba->HSregaddr, &status))
		return -EIO;

	/* Check status register to see what current state is */
	i = 0;
	while ((status & (HS_FFRDY | HS_MBRDY)) != (HS_FFRDY | HS_MBRDY)) {

		/* Check every 10ms for 10 retries, then every 100ms for 90
		 * retries, then every 1 sec for 50 retires for a total of
		 * ~60 seconds before reset the board again and check every
		 * 1 sec for 50 retries. The up to 60 seconds before the
		 * board ready is required by the Falcon FIPS zeroization
		 * complete, and any reset the board in between shall cause
		 * restart of zeroization, further delay the board ready.
		 */
		if (i++ >= 200) {
			/* Adapter failed to init, timeout, status reg
			   <status> */
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0436 Adapter failed to init, "
					"timeout, status reg x%x, "
					"FW Data: A8 x%x AC x%x\n", status,
					readl(phba->MBslimaddr + 0xa8),
					readl(phba->MBslimaddr + 0xac));
			phba->link_state = LPFC_HBA_ERROR;
			return -ETIMEDOUT;
		}

		/* Check to see if any errors occurred during init */
		if (status & HS_FFERM) {
			/* ERROR: During chipset initialization */
			/* Adapter failed to init, chipset, status reg
			   <status> */
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"0437 Adapter failed to init, "
					"chipset, status reg x%x, "
					"FW Data: A8 x%x AC x%x\n", status,
					readl(phba->MBslimaddr + 0xa8),
					readl(phba->MBslimaddr + 0xac));
			phba->link_state = LPFC_HBA_ERROR;
			return -EIO;
		}

		if (i <= 10)
			msleep(10);
		else if (i <= 100)
			msleep(100);
		else
			msleep(1000);

		if (i == 150) {
			/* Do post */
			phba->pport->port_state = LPFC_VPORT_UNKNOWN;
			lpfc_sli_brdrestart(phba);
		}
		/* Read the HBA Host Status Register */
		if (lpfc_readl(phba->HSregaddr, &status))
			return -EIO;
	}

	/* Check to see if any errors occurred during init */
	if (status & HS_FFERM) {
		/* ERROR: During chipset initialization */
		/* Adapter failed to init, chipset, status reg <status> */
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0438 Adapter failed to init, chipset, "
				"status reg x%x, "
				"FW Data: A8 x%x AC x%x\n", status,
				readl(phba->MBslimaddr + 0xa8),
				readl(phba->MBslimaddr + 0xac));
		phba->link_state = LPFC_HBA_ERROR;
		return -EIO;
	}

	/* Clear all interrupt enable conditions */
	writel(0, phba->HCregaddr);
	readl(phba->HCregaddr); /* flush */

	/* setup host attn register */
	writel(0xffffffff, phba->HAregaddr);
	readl(phba->HAregaddr); /* flush */
	return 0;
}

/**
 * lpfc_sli_hbq_count - Get the number of HBQs to be configured
 *
 * This function calculates and returns the number of HBQs required to be
 * configured.
 **/
int
lpfc_sli_hbq_count(void)
{
	return ARRAY_SIZE(lpfc_hbq_defs);
}

/**
 * lpfc_sli_hbq_entry_count - Calculate total number of hbq entries
 *
 * This function adds the number of hbq entries in every HBQ to get
 * the total number of hbq entries required for the HBA and returns
 * the total count.
 **/
static int
lpfc_sli_hbq_entry_count(void)
{
	int  hbq_count = lpfc_sli_hbq_count();
	int  count = 0;
	int  i;

	for (i = 0; i < hbq_count; ++i)
		count += lpfc_hbq_defs[i]->entry_count;
	return count;
}

/**
 * lpfc_sli_hbq_size - Calculate memory required for all hbq entries
 *
 * This function calculates amount of memory required for all hbq entries
 * to be configured and returns the total memory required.
 **/
int
lpfc_sli_hbq_size(void)
{
	return lpfc_sli_hbq_entry_count() * sizeof(struct lpfc_hbq_entry);
}

/**
 * lpfc_sli_hbq_setup - configure and initialize HBQs
 * @phba: Pointer to HBA context object.
 *
 * This function is called during the SLI initialization to configure
 * all the HBQs and post buffers to the HBQ. The caller is not
 * required to hold any locks. This function will return zero if successful
 * else it will return negative error code.
 **/
static int
lpfc_sli_hbq_setup(struct lpfc_hba *phba)
{
	int  hbq_count = lpfc_sli_hbq_count();
	LPFC_MBOXQ_t *pmb;
	MAILBOX_t *pmbox;
	uint32_t hbqno;
	uint32_t hbq_entry_index;

				/* Get a Mailbox buffer to setup mailbox
				 * commands for HBA initialization
				 */
	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);

	if (!pmb)
		return -ENOMEM;

	pmbox = &pmb->u.mb;

	/* Initialize the struct lpfc_sli_hbq structure for each hbq */
	phba->link_state = LPFC_INIT_MBX_CMDS;
	phba->hbq_in_use = 1;

	hbq_entry_index = 0;
	for (hbqno = 0; hbqno < hbq_count; ++hbqno) {
		phba->hbqs[hbqno].next_hbqPutIdx = 0;
		phba->hbqs[hbqno].hbqPutIdx      = 0;
		phba->hbqs[hbqno].local_hbqGetIdx   = 0;
		phba->hbqs[hbqno].entry_count =
			lpfc_hbq_defs[hbqno]->entry_count;
		lpfc_config_hbq(phba, hbqno, lpfc_hbq_defs[hbqno],
			hbq_entry_index, pmb);
		hbq_entry_index += phba->hbqs[hbqno].entry_count;

		if (lpfc_sli_issue_mbox(phba, pmb, MBX_POLL) != MBX_SUCCESS) {
			/* Adapter failed to init, mbxCmd <cmd> CFG_RING,
			   mbxStatus <status>, ring <num> */

			lpfc_printf_log(phba, KERN_ERR,
					LOG_SLI | LOG_VPORT,
					"1805 Adapter failed to init. "
					"Data: x%x x%x x%x\n",
					pmbox->mbxCommand,
					pmbox->mbxStatus, hbqno);

			phba->link_state = LPFC_HBA_ERROR;
			mempool_free(pmb, phba->mbox_mem_pool);
			return -ENXIO;
		}
	}
	phba->hbq_count = hbq_count;

	mempool_free(pmb, phba->mbox_mem_pool);

	/* Initially populate or replenish the HBQs */
	for (hbqno = 0; hbqno < hbq_count; ++hbqno)
		lpfc_sli_hbqbuf_init_hbqs(phba, hbqno);
	return 0;
}

/**
 * lpfc_sli4_rb_setup - Initialize and post RBs to HBA
 * @phba: Pointer to HBA context object.
 *
 * This function is called during the SLI initialization to configure
 * all the HBQs and post buffers to the HBQ. The caller is not
 * required to hold any locks. This function will return zero if successful
 * else it will return negative error code.
 **/
static int
lpfc_sli4_rb_setup(struct lpfc_hba *phba)
{
	phba->hbq_in_use = 1;
	phba->hbqs[0].entry_count = lpfc_hbq_defs[0]->entry_count;
	phba->hbq_count = 1;
	/* Initially populate or replenish the HBQs */
	lpfc_sli_hbqbuf_init_hbqs(phba, 0);
	return 0;
}

/**
 * lpfc_sli_config_port - Issue config port mailbox command
 * @phba: Pointer to HBA context object.
 * @sli_mode: sli mode - 2/3
 *
 * This function is called by the sli intialization code path
 * to issue config_port mailbox command. This function restarts the
 * HBA firmware and issues a config_port mailbox command to configure
 * the SLI interface in the sli mode specified by sli_mode
 * variable. The caller is not required to hold any locks.
 * The function returns 0 if successful, else returns negative error
 * code.
 **/
int
lpfc_sli_config_port(struct lpfc_hba *phba, int sli_mode)
{
	LPFC_MBOXQ_t *pmb;
	uint32_t resetcount = 0, rc = 0, done = 0;

	pmb = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		phba->link_state = LPFC_HBA_ERROR;
		return -ENOMEM;
	}

	phba->sli_rev = sli_mode;
	while (resetcount < 2 && !done) {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
		phba->pport->port_state = LPFC_VPORT_UNKNOWN;
		lpfc_sli_brdrestart(phba);
		rc = lpfc_sli_chipset_init(phba);
		if (rc)
			break;

		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
		resetcount++;

		/* Call pre CONFIG_PORT mailbox command initialization.  A
		 * value of 0 means the call was successful.  Any other
		 * nonzero value is a failure, but if ERESTART is returned,
		 * the driver may reset the HBA and try again.
		 */
		rc = lpfc_config_port_prep(phba);
		if (rc == -ERESTART) {
			phba->link_state = LPFC_LINK_UNKNOWN;
			continue;
		} else if (rc)
			break;

		phba->link_state = LPFC_INIT_MBX_CMDS;
		lpfc_config_port(phba, pmb);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);
		phba->sli3_options &= ~(LPFC_SLI3_NPIV_ENABLED |
					LPFC_SLI3_HBQ_ENABLED |
					LPFC_SLI3_CRP_ENABLED |
					LPFC_SLI3_BG_ENABLED |
					LPFC_SLI3_DSS_ENABLED);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0442 Adapter failed to init, mbxCmd x%x "
				"CONFIG_PORT, mbxStatus x%x Data: x%x\n",
				pmb->u.mb.mbxCommand, pmb->u.mb.mbxStatus, 0);
			spin_lock_irq(&phba->hbalock);
			phba->sli.sli_flag &= ~LPFC_SLI_ACTIVE;
			spin_unlock_irq(&phba->hbalock);
			rc = -ENXIO;
		} else {
			/* Allow asynchronous mailbox command to go through */
			spin_lock_irq(&phba->hbalock);
			phba->sli.sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
			spin_unlock_irq(&phba->hbalock);
			done = 1;

			if ((pmb->u.mb.un.varCfgPort.casabt == 1) &&
			    (pmb->u.mb.un.varCfgPort.gasabt == 0))
				lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
					"3110 Port did not grant ASABT\n");
		}
	}
	if (!done) {
		rc = -EINVAL;
		goto do_prep_failed;
	}
	if (pmb->u.mb.un.varCfgPort.sli_mode == 3) {
		if (!pmb->u.mb.un.varCfgPort.cMA) {
			rc = -ENXIO;
			goto do_prep_failed;
		}
		if (phba->max_vpi && pmb->u.mb.un.varCfgPort.gmv) {
			phba->sli3_options |= LPFC_SLI3_NPIV_ENABLED;
			phba->max_vpi = pmb->u.mb.un.varCfgPort.max_vpi;
			phba->max_vports = (phba->max_vpi > phba->max_vports) ?
				phba->max_vpi : phba->max_vports;

		} else
			phba->max_vpi = 0;
		phba->fips_level = 0;
		phba->fips_spec_rev = 0;
		if (pmb->u.mb.un.varCfgPort.gdss) {
			phba->sli3_options |= LPFC_SLI3_DSS_ENABLED;
			phba->fips_level = pmb->u.mb.un.varCfgPort.fips_level;
			phba->fips_spec_rev = pmb->u.mb.un.varCfgPort.fips_rev;
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2850 Security Crypto Active. FIPS x%d "
					"(Spec Rev: x%d)",
					phba->fips_level, phba->fips_spec_rev);
		}
		if (pmb->u.mb.un.varCfgPort.sec_err) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2856 Config Port Security Crypto "
					"Error: x%x ",
					pmb->u.mb.un.varCfgPort.sec_err);
		}
		if (pmb->u.mb.un.varCfgPort.gerbm)
			phba->sli3_options |= LPFC_SLI3_HBQ_ENABLED;
		if (pmb->u.mb.un.varCfgPort.gcrp)
			phba->sli3_options |= LPFC_SLI3_CRP_ENABLED;

		phba->hbq_get = phba->mbox->us.s3_pgp.hbq_get;
		phba->port_gp = phba->mbox->us.s3_pgp.port;

		if (phba->cfg_enable_bg) {
			if (pmb->u.mb.un.varCfgPort.gbg)
				phba->sli3_options |= LPFC_SLI3_BG_ENABLED;
			else
				lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
						"0443 Adapter did not grant "
						"BlockGuard\n");
		}
	} else {
		phba->hbq_get = NULL;
		phba->port_gp = phba->mbox->us.s2.port;
		phba->max_vpi = 0;
	}
do_prep_failed:
	mempool_free(pmb, phba->mbox_mem_pool);
	return rc;
}


/**
 * lpfc_sli_hba_setup - SLI intialization function
 * @phba: Pointer to HBA context object.
 *
 * This function is the main SLI intialization function. This function
 * is called by the HBA intialization code, HBA reset code and HBA
 * error attention handler code. Caller is not required to hold any
 * locks. This function issues config_port mailbox command to configure
 * the SLI, setup iocb rings and HBQ rings. In the end the function
 * calls the config_port_post function to issue init_link mailbox
 * command and to start the discovery. The function will return zero
 * if successful, else it will return negative error code.
 **/
int
lpfc_sli_hba_setup(struct lpfc_hba *phba)
{
	uint32_t rc;
	int  mode = 3, i;
	int longs;

	switch (lpfc_sli_mode) {
	case 2:
		if (phba->cfg_enable_npiv) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_VPORT,
				"1824 NPIV enabled: Override lpfc_sli_mode "
				"parameter (%d) to auto (0).\n",
				lpfc_sli_mode);
			break;
		}
		mode = 2;
		break;
	case 0:
	case 3:
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_VPORT,
				"1819 Unrecognized lpfc_sli_mode "
				"parameter: %d.\n", lpfc_sli_mode);

		break;
	}

	rc = lpfc_sli_config_port(phba, mode);

	if (rc && lpfc_sli_mode == 3)
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_VPORT,
				"1820 Unable to select SLI-3.  "
				"Not supported by adapter.\n");
	if (rc && mode != 2)
		rc = lpfc_sli_config_port(phba, 2);
	if (rc)
		goto lpfc_sli_hba_setup_error;

	/* Enable PCIe device Advanced Error Reporting (AER) if configured */
	if (phba->cfg_aer_support == 1 && !(phba->hba_flag & HBA_AER_ENABLED)) {
		rc = pci_enable_pcie_error_reporting(phba->pcidev);
		if (!rc) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2709 This device supports "
					"Advanced Error Reporting (AER)\n");
			spin_lock_irq(&phba->hbalock);
			phba->hba_flag |= HBA_AER_ENABLED;
			spin_unlock_irq(&phba->hbalock);
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2708 This device does not support "
					"Advanced Error Reporting (AER)\n");
			phba->cfg_aer_support = 0;
		}
	}

	if (phba->sli_rev == 3) {
		phba->iocb_cmd_size = SLI3_IOCB_CMD_SIZE;
		phba->iocb_rsp_size = SLI3_IOCB_RSP_SIZE;
	} else {
		phba->iocb_cmd_size = SLI2_IOCB_CMD_SIZE;
		phba->iocb_rsp_size = SLI2_IOCB_RSP_SIZE;
		phba->sli3_options = 0;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
			"0444 Firmware in SLI %x mode. Max_vpi %d\n",
			phba->sli_rev, phba->max_vpi);
	rc = lpfc_sli_ring_map(phba);

	if (rc)
		goto lpfc_sli_hba_setup_error;

	/* Initialize VPIs. */
	if (phba->sli_rev == LPFC_SLI_REV3) {
		/*
		 * The VPI bitmask and physical ID array are allocated
		 * and initialized once only - at driver load.  A port
		 * reset doesn't need to reinitialize this memory.
		 */
		if ((phba->vpi_bmask == NULL) && (phba->vpi_ids == NULL)) {
			longs = (phba->max_vpi + BITS_PER_LONG) / BITS_PER_LONG;
			phba->vpi_bmask = kzalloc(longs * sizeof(unsigned long),
						  GFP_KERNEL);
			if (!phba->vpi_bmask) {
				rc = -ENOMEM;
				goto lpfc_sli_hba_setup_error;
			}

			phba->vpi_ids = kzalloc(
					(phba->max_vpi+1) * sizeof(uint16_t),
					GFP_KERNEL);
			if (!phba->vpi_ids) {
				kfree(phba->vpi_bmask);
				rc = -ENOMEM;
				goto lpfc_sli_hba_setup_error;
			}
			for (i = 0; i < phba->max_vpi; i++)
				phba->vpi_ids[i] = i;
		}
	}

	/* Init HBQs */
	if (phba->sli3_options & LPFC_SLI3_HBQ_ENABLED) {
		rc = lpfc_sli_hbq_setup(phba);
		if (rc)
			goto lpfc_sli_hba_setup_error;
	}
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag |= LPFC_PROCESS_LA;
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_config_port_post(phba);
	if (rc)
		goto lpfc_sli_hba_setup_error;

	return rc;

lpfc_sli_hba_setup_error:
	phba->link_state = LPFC_HBA_ERROR;
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0445 Firmware initialization failed\n");
	return rc;
}

/**
 * lpfc_sli4_read_fcoe_params - Read fcoe params from conf region
 * @phba: Pointer to HBA context object.
 * @mboxq: mailbox pointer.
 * This function issue a dump mailbox command to read config region
 * 23 and parse the records in the region and populate driver
 * data structure.
 **/
static int
lpfc_sli4_read_fcoe_params(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_dmabuf *mp;
	struct lpfc_mqe *mqe;
	uint32_t data_length;
	int rc;

	/* Program the default value of vlan_id and fc_map */
	phba->valid_vlan = 0;
	phba->fc_map[0] = LPFC_FCOE_FCF_MAP0;
	phba->fc_map[1] = LPFC_FCOE_FCF_MAP1;
	phba->fc_map[2] = LPFC_FCOE_FCF_MAP2;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	mqe = &mboxq->u.mqe;
	if (lpfc_sli4_dump_cfg_rg23(phba, mboxq)) {
		rc = -ENOMEM;
		goto out_free_mboxq;
	}

	mp = (struct lpfc_dmabuf *) mboxq->context1;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):2571 Mailbox cmd x%x Status x%x "
			"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x "
			"x%x x%x x%x x%x x%x x%x x%x x%x x%x "
			"CQ: x%x x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0,
			bf_get(lpfc_mqe_command, mqe),
			bf_get(lpfc_mqe_status, mqe),
			mqe->un.mb_words[0], mqe->un.mb_words[1],
			mqe->un.mb_words[2], mqe->un.mb_words[3],
			mqe->un.mb_words[4], mqe->un.mb_words[5],
			mqe->un.mb_words[6], mqe->un.mb_words[7],
			mqe->un.mb_words[8], mqe->un.mb_words[9],
			mqe->un.mb_words[10], mqe->un.mb_words[11],
			mqe->un.mb_words[12], mqe->un.mb_words[13],
			mqe->un.mb_words[14], mqe->un.mb_words[15],
			mqe->un.mb_words[16], mqe->un.mb_words[50],
			mboxq->mcqe.word0,
			mboxq->mcqe.mcqe_tag0, 	mboxq->mcqe.mcqe_tag1,
			mboxq->mcqe.trailer);

	if (rc) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		rc = -EIO;
		goto out_free_mboxq;
	}
	data_length = mqe->un.mb_words[5];
	if (data_length > DMP_RGN23_SIZE) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
		rc = -EIO;
		goto out_free_mboxq;
	}

	lpfc_parse_fcoe_conf(phba, mp->virt, data_length);
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	rc = 0;

out_free_mboxq:
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

/**
 * lpfc_sli4_read_rev - Issue READ_REV and collect vpd data
 * @phba: pointer to lpfc hba data structure.
 * @mboxq: pointer to the LPFC_MBOXQ_t structure.
 * @vpd: pointer to the memory to hold resulting port vpd data.
 * @vpd_size: On input, the number of bytes allocated to @vpd.
 *	      On output, the number of data bytes in @vpd.
 *
 * This routine executes a READ_REV SLI4 mailbox command.  In
 * addition, this routine gets the port vpd data.
 *
 * Return codes
 * 	0 - successful
 * 	-ENOMEM - could not allocated memory.
 **/
static int
lpfc_sli4_read_rev(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
		    uint8_t *vpd, uint32_t *vpd_size)
{
	int rc = 0;
	uint32_t dma_size;
	struct lpfc_dmabuf *dmabuf;
	struct lpfc_mqe *mqe;

	dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
	if (!dmabuf)
		return -ENOMEM;

	/*
	 * Get a DMA buffer for the vpd data resulting from the READ_REV
	 * mailbox command.
	 */
	dma_size = *vpd_size;
	dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
					  dma_size,
					  &dmabuf->phys,
					  GFP_KERNEL);
	if (!dmabuf->virt) {
		kfree(dmabuf);
		return -ENOMEM;
	}
	memset(dmabuf->virt, 0, dma_size);

	/*
	 * The SLI4 implementation of READ_REV conflicts at word1,
	 * bits 31:16 and SLI4 adds vpd functionality not present
	 * in SLI3.  This code corrects the conflicts.
	 */
	lpfc_read_rev(phba, mboxq);
	mqe = &mboxq->u.mqe;
	mqe->un.read_rev.vpd_paddr_high = putPaddrHigh(dmabuf->phys);
	mqe->un.read_rev.vpd_paddr_low = putPaddrLow(dmabuf->phys);
	mqe->un.read_rev.word1 &= 0x0000FFFF;
	bf_set(lpfc_mbx_rd_rev_vpd, &mqe->un.read_rev, 1);
	bf_set(lpfc_mbx_rd_rev_avail_len, &mqe->un.read_rev, dma_size);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc) {
		dma_free_coherent(&phba->pcidev->dev, dma_size,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
		return -EIO;
	}

	/*
	 * The available vpd length cannot be bigger than the
	 * DMA buffer passed to the port.  Catch the less than
	 * case and update the caller's size.
	 */
	if (mqe->un.read_rev.avail_vpd_len < *vpd_size)
		*vpd_size = mqe->un.read_rev.avail_vpd_len;

	memcpy(vpd, dmabuf->virt, *vpd_size);

	dma_free_coherent(&phba->pcidev->dev, dma_size,
			  dmabuf->virt, dmabuf->phys);
	kfree(dmabuf);
	return 0;
}

/**
 * lpfc_sli4_retrieve_pport_name - Retrieve SLI4 device physical port name
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine retrieves SLI4 device physical port name this PCI function
 * is attached to.
 *
 * Return codes
 *      0 - successful
 *      otherwise - failed to retrieve physical port name
 **/
static int
lpfc_sli4_retrieve_pport_name(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_get_cntl_attributes *mbx_cntl_attr;
	struct lpfc_controller_attribute *cntl_attr;
	struct lpfc_mbx_get_port_name *get_port_name;
	void *virtaddr = NULL;
	uint32_t alloclen, reqlen;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	char cport_name = 0;
	int rc;

	/* We assume nothing at this point */
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_INVAL;
	phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_NON;

	mboxq = (LPFC_MBOXQ_t *)mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;
	/* obtain link type and link number via READ_CONFIG */
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_INVAL;
	lpfc_sli4_read_config(phba);
	if (phba->sli4_hba.lnk_info.lnk_dv == LPFC_LNK_DAT_VAL)
		goto retrieve_ppname;

	/* obtain link type and link number via COMMON_GET_CNTL_ATTRIBUTES */
	reqlen = sizeof(struct lpfc_mbx_get_cntl_attributes);
	alloclen = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
			LPFC_MBOX_OPCODE_GET_CNTL_ATTRIBUTES, reqlen,
			LPFC_SLI4_MBX_NEMBED);
	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"3084 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory size "
				"(%d)\n", alloclen, reqlen);
		rc = -ENOMEM;
		goto out_free_mboxq;
	}
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	virtaddr = mboxq->sge_array->addr[0];
	mbx_cntl_attr = (struct lpfc_mbx_get_cntl_attributes *)virtaddr;
	shdr = &mbx_cntl_attr->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3085 Mailbox x%x (x%x/x%x) failed, "
				"rc:x%x, status:x%x, add_status:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				rc, shdr_status, shdr_add_status);
		rc = -ENXIO;
		goto out_free_mboxq;
	}
	cntl_attr = &mbx_cntl_attr->cntl_attr;
	phba->sli4_hba.lnk_info.lnk_dv = LPFC_LNK_DAT_VAL;
	phba->sli4_hba.lnk_info.lnk_tp =
		bf_get(lpfc_cntl_attr_lnk_type, cntl_attr);
	phba->sli4_hba.lnk_info.lnk_no =
		bf_get(lpfc_cntl_attr_lnk_numb, cntl_attr);
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3086 lnk_type:%d, lnk_numb:%d\n",
			phba->sli4_hba.lnk_info.lnk_tp,
			phba->sli4_hba.lnk_info.lnk_no);

retrieve_ppname:
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_COMMON,
		LPFC_MBOX_OPCODE_GET_PORT_NAME,
		sizeof(struct lpfc_mbx_get_port_name) -
		sizeof(struct lpfc_sli4_cfg_mhdr),
		LPFC_SLI4_MBX_EMBED);
	get_port_name = &mboxq->u.mqe.un.get_port_name;
	shdr = (union lpfc_sli4_cfg_shdr *)&get_port_name->header.cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request, LPFC_OPCODE_VERSION_1);
	bf_set(lpfc_mbx_get_port_name_lnk_type, &get_port_name->u.request,
		phba->sli4_hba.lnk_info.lnk_tp);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3087 Mailbox x%x (x%x/x%x) failed: "
				"rc:x%x, status:x%x, add_status:x%x\n",
				bf_get(lpfc_mqe_command, &mboxq->u.mqe),
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				rc, shdr_status, shdr_add_status);
		rc = -ENXIO;
		goto out_free_mboxq;
	}
	switch (phba->sli4_hba.lnk_info.lnk_no) {
	case LPFC_LINK_NUMBER_0:
		cport_name = bf_get(lpfc_mbx_get_port_name_name0,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_1:
		cport_name = bf_get(lpfc_mbx_get_port_name_name1,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_2:
		cport_name = bf_get(lpfc_mbx_get_port_name_name2,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	case LPFC_LINK_NUMBER_3:
		cport_name = bf_get(lpfc_mbx_get_port_name_name3,
				&get_port_name->u.response);
		phba->sli4_hba.pport_name_sta = LPFC_SLI4_PPNAME_GET;
		break;
	default:
		break;
	}

	if (phba->sli4_hba.pport_name_sta == LPFC_SLI4_PPNAME_GET) {
		phba->Port[0] = cport_name;
		phba->Port[1] = '\0';
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3091 SLI get port name: %s\n", phba->Port);
	}

out_free_mboxq:
	if (rc != MBX_TIMEOUT) {
		if (bf_get(lpfc_mqe_command, &mboxq->u.mqe) == MBX_SLI4_CONFIG)
			lpfc_sli4_mbox_cmd_free(phba, mboxq);
		else
			mempool_free(mboxq, phba->mbox_mem_pool);
	}
	return rc;
}

/**
 * lpfc_sli4_arm_cqeq_intr - Arm sli-4 device completion and event queues
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is called to explicitly arm the SLI4 device's completion and
 * event queues
 **/
static void
lpfc_sli4_arm_cqeq_intr(struct lpfc_hba *phba)
{
	int fcp_eqidx;

	lpfc_sli4_cq_release(phba->sli4_hba.mbx_cq, LPFC_QUEUE_REARM);
	lpfc_sli4_cq_release(phba->sli4_hba.els_cq, LPFC_QUEUE_REARM);
	fcp_eqidx = 0;
	if (phba->sli4_hba.fcp_cq) {
		do {
			lpfc_sli4_cq_release(phba->sli4_hba.fcp_cq[fcp_eqidx],
					     LPFC_QUEUE_REARM);
		} while (++fcp_eqidx < phba->cfg_fcp_io_channel);
	}
	if (phba->sli4_hba.hba_eq) {
		for (fcp_eqidx = 0; fcp_eqidx < phba->cfg_fcp_io_channel;
		     fcp_eqidx++)
			lpfc_sli4_eq_release(phba->sli4_hba.hba_eq[fcp_eqidx],
					     LPFC_QUEUE_REARM);
	}
}

/**
 * lpfc_sli4_get_avail_extnt_rsrc - Get available resource extent count.
 * @phba: Pointer to HBA context object.
 * @type: The resource extent type.
 * @extnt_count: buffer to hold port available extent count.
 * @extnt_size: buffer to hold element count per extent.
 *
 * This function calls the port and retrievs the number of available
 * extents and their size for a particular extent type.
 *
 * Returns: 0 if successful.  Nonzero otherwise.
 **/
int
lpfc_sli4_get_avail_extnt_rsrc(struct lpfc_hba *phba, uint16_t type,
			       uint16_t *extnt_count, uint16_t *extnt_size)
{
	int rc = 0;
	uint32_t length;
	uint32_t mbox_tmo;
	struct lpfc_mbx_get_rsrc_extent_info *rsrc_info;
	LPFC_MBOXQ_t *mbox;

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	/* Find out how many extents are available for this resource type */
	length = (sizeof(struct lpfc_mbx_get_rsrc_extent_info) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_GET_RSRC_EXTENT_INFO,
			 length, LPFC_SLI4_MBX_EMBED);

	/* Send an extents count of 0 - the GET doesn't use it. */
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, 0, type,
					LPFC_SLI4_MBX_EMBED);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	rsrc_info = &mbox->u.mqe.un.rsrc_extent_info;
	if (bf_get(lpfc_mbox_hdr_status,
		   &rsrc_info->header.cfg_shdr.response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
				"2930 Failed to get resource extents "
				"Status 0x%x Add'l Status 0x%x\n",
				bf_get(lpfc_mbox_hdr_status,
				       &rsrc_info->header.cfg_shdr.response),
				bf_get(lpfc_mbox_hdr_add_status,
				       &rsrc_info->header.cfg_shdr.response));
		rc = -EIO;
		goto err_exit;
	}

	*extnt_count = bf_get(lpfc_mbx_get_rsrc_extent_info_cnt,
			      &rsrc_info->u.rsp);
	*extnt_size = bf_get(lpfc_mbx_get_rsrc_extent_info_size,
			     &rsrc_info->u.rsp);

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3162 Retrieved extents type-%d from port: count:%d, "
			"size:%d\n", type, *extnt_count, *extnt_size);

err_exit:
	mempool_free(mbox, phba->mbox_mem_pool);
	return rc;
}

/**
 * lpfc_sli4_chk_avail_extnt_rsrc - Check for available SLI4 resource extents.
 * @phba: Pointer to HBA context object.
 * @type: The extent type to check.
 *
 * This function reads the current available extents from the port and checks
 * if the extent count or extent size has changed since the last access.
 * Callers use this routine post port reset to understand if there is a
 * extent reprovisioning requirement.
 *
 * Returns:
 *   -Error: error indicates problem.
 *   1: Extent count or size has changed.
 *   0: No changes.
 **/
static int
lpfc_sli4_chk_avail_extnt_rsrc(struct lpfc_hba *phba, uint16_t type)
{
	uint16_t curr_ext_cnt, rsrc_ext_cnt;
	uint16_t size_diff, rsrc_ext_size;
	int rc = 0;
	struct lpfc_rsrc_blks *rsrc_entry;
	struct list_head *rsrc_blk_list = NULL;

	size_diff = 0;
	curr_ext_cnt = 0;
	rc = lpfc_sli4_get_avail_extnt_rsrc(phba, type,
					    &rsrc_ext_cnt,
					    &rsrc_ext_size);
	if (unlikely(rc))
		return -EIO;

	switch (type) {
	case LPFC_RSC_TYPE_FCOE_RPI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VPI:
		rsrc_blk_list = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		rsrc_blk_list = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	default:
		break;
	}

	list_for_each_entry(rsrc_entry, rsrc_blk_list, list) {
		curr_ext_cnt++;
		if (rsrc_entry->rsrc_size != rsrc_ext_size)
			size_diff++;
	}

	if (curr_ext_cnt != rsrc_ext_cnt || size_diff != 0)
		rc = 1;

	return rc;
}

/**
 * lpfc_sli4_cfg_post_extnts -
 * @phba: Pointer to HBA context object.
 * @extnt_cnt - number of available extents.
 * @type - the extent type (rpi, xri, vfi, vpi).
 * @emb - buffer to hold either MBX_EMBED or MBX_NEMBED operation.
 * @mbox - pointer to the caller's allocated mailbox structure.
 *
 * This function executes the extents allocation request.  It also
 * takes care of the amount of memory needed to allocate or get the
 * allocated extents. It is the caller's responsibility to evaluate
 * the response.
 *
 * Returns:
 *   -Error:  Error value describes the condition found.
 *   0: if successful
 **/
static int
lpfc_sli4_cfg_post_extnts(struct lpfc_hba *phba, uint16_t extnt_cnt,
			  uint16_t type, bool *emb, LPFC_MBOXQ_t *mbox)
{
	int rc = 0;
	uint32_t req_len;
	uint32_t emb_len;
	uint32_t alloc_len, mbox_tmo;

	/* Calculate the total requested length of the dma memory */
	req_len = extnt_cnt * sizeof(uint16_t);

	/*
	 * Calculate the size of an embedded mailbox.  The uint32_t
	 * accounts for extents-specific word.
	 */
	emb_len = sizeof(MAILBOX_t) - sizeof(struct mbox_header) -
		sizeof(uint32_t);

	/*
	 * Presume the allocation and response will fit into an embedded
	 * mailbox.  If not true, reconfigure to a non-embedded mailbox.
	 */
	*emb = LPFC_SLI4_MBX_EMBED;
	if (req_len > emb_len) {
		req_len = extnt_cnt * sizeof(uint16_t) +
			sizeof(union lpfc_sli4_cfg_shdr) +
			sizeof(uint32_t);
		*emb = LPFC_SLI4_MBX_NEMBED;
	}

	alloc_len = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
				     LPFC_MBOX_OPCODE_ALLOC_RSRC_EXTENT,
				     req_len, *emb);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2982 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		return -ENOMEM;
	}
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, extnt_cnt, type, *emb);
	if (unlikely(rc))
		return -EIO;

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	if (unlikely(rc))
		rc = -EIO;
	return rc;
}

/**
 * lpfc_sli4_alloc_extent - Allocate an SLI4 resource extent.
 * @phba: Pointer to HBA context object.
 * @type:  The resource extent type to allocate.
 *
 * This function allocates the number of elements for the specified
 * resource type.
 **/
static int
lpfc_sli4_alloc_extent(struct lpfc_hba *phba, uint16_t type)
{
	bool emb = false;
	uint16_t rsrc_id_cnt, rsrc_cnt, rsrc_size;
	uint16_t rsrc_id, rsrc_start, j, k;
	uint16_t *ids;
	int i, rc;
	unsigned long longs;
	unsigned long *bmask;
	struct lpfc_rsrc_blks *rsrc_blks;
	LPFC_MBOXQ_t *mbox;
	uint32_t length;
	struct lpfc_id_range *id_array = NULL;
	void *virtaddr = NULL;
	struct lpfc_mbx_nembed_rsrc_extent *n_rsrc;
	struct lpfc_mbx_alloc_rsrc_extents *rsrc_ext;
	struct list_head *ext_blk_list;

	rc = lpfc_sli4_get_avail_extnt_rsrc(phba, type,
					    &rsrc_cnt,
					    &rsrc_size);
	if (unlikely(rc))
		return -EIO;

	if ((rsrc_cnt == 0) || (rsrc_size == 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
			"3009 No available Resource Extents "
			"for resource type 0x%x: Count: 0x%x, "
			"Size 0x%x\n", type, rsrc_cnt,
			rsrc_size);
		return -ENOMEM;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_INIT | LOG_SLI,
			"2903 Post resource extents type-0x%x: "
			"count:%d, size %d\n", type, rsrc_cnt, rsrc_size);

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	rc = lpfc_sli4_cfg_post_extnts(phba, rsrc_cnt, type, &emb, mbox);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	/*
	 * Figure out where the response is located.  Then get local pointers
	 * to the response data.  The port does not guarantee to respond to
	 * all extents counts request so update the local variable with the
	 * allocated count from the port.
	 */
	if (emb == LPFC_SLI4_MBX_EMBED) {
		rsrc_ext = &mbox->u.mqe.un.alloc_rsrc_extents;
		id_array = &rsrc_ext->u.rsp.id[0];
		rsrc_cnt = bf_get(lpfc_mbx_rsrc_cnt, &rsrc_ext->u.rsp);
	} else {
		virtaddr = mbox->sge_array->addr[0];
		n_rsrc = (struct lpfc_mbx_nembed_rsrc_extent *) virtaddr;
		rsrc_cnt = bf_get(lpfc_mbx_rsrc_cnt, n_rsrc);
		id_array = &n_rsrc->id;
	}

	longs = ((rsrc_cnt * rsrc_size) + BITS_PER_LONG - 1) / BITS_PER_LONG;
	rsrc_id_cnt = rsrc_cnt * rsrc_size;

	/*
	 * Based on the resource size and count, correct the base and max
	 * resource values.
	 */
	length = sizeof(struct lpfc_rsrc_blks);
	switch (type) {
	case LPFC_RSC_TYPE_FCOE_RPI:
		phba->sli4_hba.rpi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.rpi_ids = kzalloc(rsrc_id_cnt *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_ids)) {
			kfree(phba->sli4_hba.rpi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		/*
		 * The next_rpi was initialized with the maximum available
		 * count but the port may allocate a smaller number.  Catch
		 * that case and update the next_rpi.
		 */
		phba->sli4_hba.next_rpi = rsrc_id_cnt;

		/* Initialize local ptrs for common extent processing later. */
		bmask = phba->sli4_hba.rpi_bmask;
		ids = phba->sli4_hba.rpi_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VPI:
		phba->vpi_bmask = kzalloc(longs *
					  sizeof(unsigned long),
					  GFP_KERNEL);
		if (unlikely(!phba->vpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->vpi_ids = kzalloc(rsrc_id_cnt *
					 sizeof(uint16_t),
					 GFP_KERNEL);
		if (unlikely(!phba->vpi_ids)) {
			kfree(phba->vpi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		/* Initialize local ptrs for common extent processing later. */
		bmask = phba->vpi_bmask;
		ids = phba->vpi_ids;
		ext_blk_list = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		phba->sli4_hba.xri_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.max_cfg_param.xri_used = 0;
		phba->sli4_hba.xri_ids = kzalloc(rsrc_id_cnt *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_ids)) {
			kfree(phba->sli4_hba.xri_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		/* Initialize local ptrs for common extent processing later. */
		bmask = phba->sli4_hba.xri_bmask;
		ids = phba->sli4_hba.xri_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		phba->sli4_hba.vfi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.vfi_ids = kzalloc(rsrc_id_cnt *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_ids)) {
			kfree(phba->sli4_hba.vfi_bmask);
			rc = -ENOMEM;
			goto err_exit;
		}

		/* Initialize local ptrs for common extent processing later. */
		bmask = phba->sli4_hba.vfi_bmask;
		ids = phba->sli4_hba.vfi_ids;
		ext_blk_list = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	default:
		/* Unsupported Opcode.  Fail call. */
		id_array = NULL;
		bmask = NULL;
		ids = NULL;
		ext_blk_list = NULL;
		goto err_exit;
	}

	/*
	 * Complete initializing the extent configuration with the
	 * allocated ids assigned to this function.  The bitmask serves
	 * as an index into the array and manages the available ids.  The
	 * array just stores the ids communicated to the port via the wqes.
	 */
	for (i = 0, j = 0, k = 0; i < rsrc_cnt; i++) {
		if ((i % 2) == 0)
			rsrc_id = bf_get(lpfc_mbx_rsrc_id_word4_0,
					 &id_array[k]);
		else
			rsrc_id = bf_get(lpfc_mbx_rsrc_id_word4_1,
					 &id_array[k]);

		rsrc_blks = kzalloc(length, GFP_KERNEL);
		if (unlikely(!rsrc_blks)) {
			rc = -ENOMEM;
			kfree(bmask);
			kfree(ids);
			goto err_exit;
		}
		rsrc_blks->rsrc_start = rsrc_id;
		rsrc_blks->rsrc_size = rsrc_size;
		list_add_tail(&rsrc_blks->list, ext_blk_list);
		rsrc_start = rsrc_id;
		if ((type == LPFC_RSC_TYPE_FCOE_XRI) && (j == 0))
			phba->sli4_hba.scsi_xri_start = rsrc_start +
				lpfc_sli4_get_els_iocb_cnt(phba);

		while (rsrc_id < (rsrc_start + rsrc_size)) {
			ids[j] = rsrc_id;
			rsrc_id++;
			j++;
		}
		/* Entire word processed.  Get next word.*/
		if ((i % 2) == 1)
			k++;
	}
 err_exit:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return rc;
}

/**
 * lpfc_sli4_dealloc_extent - Deallocate an SLI4 resource extent.
 * @phba: Pointer to HBA context object.
 * @type: the extent's type.
 *
 * This function deallocates all extents of a particular resource type.
 * SLI4 does not allow for deallocating a particular extent range.  It
 * is the caller's responsibility to release all kernel memory resources.
 **/
static int
lpfc_sli4_dealloc_extent(struct lpfc_hba *phba, uint16_t type)
{
	int rc;
	uint32_t length, mbox_tmo = 0;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_mbx_dealloc_rsrc_extents *dealloc_rsrc;
	struct lpfc_rsrc_blks *rsrc_blk, *rsrc_blk_next;

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	/*
	 * This function sends an embedded mailbox because it only sends the
	 * the resource type.  All extents of this type are released by the
	 * port.
	 */
	length = (sizeof(struct lpfc_mbx_dealloc_rsrc_extents) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_DEALLOC_RSRC_EXTENT,
			 length, LPFC_SLI4_MBX_EMBED);

	/* Send an extents count of 0 - the dealloc doesn't use it. */
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, 0, type,
					LPFC_SLI4_MBX_EMBED);
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}

	dealloc_rsrc = &mbox->u.mqe.un.dealloc_rsrc_extents;
	if (bf_get(lpfc_mbox_hdr_status,
		   &dealloc_rsrc->header.cfg_shdr.response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
				"2919 Failed to release resource extents "
				"for type %d - Status 0x%x Add'l Status 0x%x. "
				"Resource memory not released.\n",
				type,
				bf_get(lpfc_mbox_hdr_status,
				    &dealloc_rsrc->header.cfg_shdr.response),
				bf_get(lpfc_mbox_hdr_add_status,
				    &dealloc_rsrc->header.cfg_shdr.response));
		rc = -EIO;
		goto out_free_mbox;
	}

	/* Release kernel memory resources for the specific type. */
	switch (type) {
	case LPFC_RSC_TYPE_FCOE_VPI:
		kfree(phba->vpi_bmask);
		kfree(phba->vpi_ids);
		bf_set(lpfc_vpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->lpfc_vpi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		phba->sli4_hba.max_cfg_param.vpi_used = 0;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		kfree(phba->sli4_hba.xri_bmask);
		kfree(phba->sli4_hba.xri_ids);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_xri_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		kfree(phba->sli4_hba.vfi_bmask);
		kfree(phba->sli4_hba.vfi_ids);
		bf_set(lpfc_vfi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_vfi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	case LPFC_RSC_TYPE_FCOE_RPI:
		/* RPI bitmask and physical id array are cleaned up earlier. */
		list_for_each_entry_safe(rsrc_blk, rsrc_blk_next,
				    &phba->sli4_hba.lpfc_rpi_blk_list, list) {
			list_del_init(&rsrc_blk->list);
			kfree(rsrc_blk);
		}
		break;
	default:
		break;
	}

	bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);

 out_free_mbox:
	mempool_free(mbox, phba->mbox_mem_pool);
	return rc;
}

/**
 * lpfc_sli4_alloc_resource_identifiers - Allocate all SLI4 resource extents.
 * @phba: Pointer to HBA context object.
 *
 * This function allocates all SLI4 resource identifiers.
 **/
int
lpfc_sli4_alloc_resource_identifiers(struct lpfc_hba *phba)
{
	int i, rc, error = 0;
	uint16_t count, base;
	unsigned long longs;

	if (!phba->sli4_hba.rpi_hdrs_in_use)
		phba->sli4_hba.next_rpi = phba->sli4_hba.max_cfg_param.max_rpi;
	if (phba->sli4_hba.extents_in_use) {
		/*
		 * The port supports resource extents. The XRI, VPI, VFI, RPI
		 * resource extent count must be read and allocated before
		 * provisioning the resource id arrays.
		 */
		if (bf_get(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags) ==
		    LPFC_IDX_RSRC_RDY) {
			/*
			 * Extent-based resources are set - the driver could
			 * be in a port reset. Figure out if any corrective
			 * actions need to be taken.
			 */
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_VFI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_VPI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_XRI);
			if (rc != 0)
				error++;
			rc = lpfc_sli4_chk_avail_extnt_rsrc(phba,
						 LPFC_RSC_TYPE_FCOE_RPI);
			if (rc != 0)
				error++;

			/*
			 * It's possible that the number of resources
			 * provided to this port instance changed between
			 * resets.  Detect this condition and reallocate
			 * resources.  Otherwise, there is no action.
			 */
			if (error) {
				lpfc_printf_log(phba, KERN_INFO,
						LOG_MBOX | LOG_INIT,
						"2931 Detected extent resource "
						"change.  Reallocating all "
						"extents.\n");
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_VFI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_VPI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_XRI);
				rc = lpfc_sli4_dealloc_extent(phba,
						 LPFC_RSC_TYPE_FCOE_RPI);
			} else
				return 0;
		}

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_VFI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_VPI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_RPI);
		if (unlikely(rc))
			goto err_exit;

		rc = lpfc_sli4_alloc_extent(phba, LPFC_RSC_TYPE_FCOE_XRI);
		if (unlikely(rc))
			goto err_exit;
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_IDX_RSRC_RDY);
		return rc;
	} else {
		/*
		 * The port does not support resource extents.  The XRI, VPI,
		 * VFI, RPI resource ids were determined from READ_CONFIG.
		 * Just allocate the bitmasks and provision the resource id
		 * arrays.  If a port reset is active, the resources don't
		 * need any action - just exit.
		 */
		if (bf_get(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags) ==
		    LPFC_IDX_RSRC_RDY) {
			lpfc_sli4_dealloc_resource_identifiers(phba);
			lpfc_sli4_remove_rpis(phba);
		}
		/* RPIs. */
		count = phba->sli4_hba.max_cfg_param.max_rpi;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3279 Invalid provisioning of "
					"rpi:%d\n", count);
			rc = -EINVAL;
			goto err_exit;
		}
		base = phba->sli4_hba.max_cfg_param.rpi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.rpi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_bmask)) {
			rc = -ENOMEM;
			goto err_exit;
		}
		phba->sli4_hba.rpi_ids = kzalloc(count *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.rpi_ids)) {
			rc = -ENOMEM;
			goto free_rpi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.rpi_ids[i] = base + i;

		/* VPIs. */
		count = phba->sli4_hba.max_cfg_param.max_vpi;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3280 Invalid provisioning of "
					"vpi:%d\n", count);
			rc = -EINVAL;
			goto free_rpi_ids;
		}
		base = phba->sli4_hba.max_cfg_param.vpi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->vpi_bmask = kzalloc(longs *
					  sizeof(unsigned long),
					  GFP_KERNEL);
		if (unlikely(!phba->vpi_bmask)) {
			rc = -ENOMEM;
			goto free_rpi_ids;
		}
		phba->vpi_ids = kzalloc(count *
					sizeof(uint16_t),
					GFP_KERNEL);
		if (unlikely(!phba->vpi_ids)) {
			rc = -ENOMEM;
			goto free_vpi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->vpi_ids[i] = base + i;

		/* XRIs. */
		count = phba->sli4_hba.max_cfg_param.max_xri;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3281 Invalid provisioning of "
					"xri:%d\n", count);
			rc = -EINVAL;
			goto free_vpi_ids;
		}
		base = phba->sli4_hba.max_cfg_param.xri_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.xri_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_bmask)) {
			rc = -ENOMEM;
			goto free_vpi_ids;
		}
		phba->sli4_hba.max_cfg_param.xri_used = 0;
		phba->sli4_hba.xri_ids = kzalloc(count *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.xri_ids)) {
			rc = -ENOMEM;
			goto free_xri_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.xri_ids[i] = base + i;

		/* VFIs. */
		count = phba->sli4_hba.max_cfg_param.max_vfi;
		if (count <= 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"3282 Invalid provisioning of "
					"vfi:%d\n", count);
			rc = -EINVAL;
			goto free_xri_ids;
		}
		base = phba->sli4_hba.max_cfg_param.vfi_base;
		longs = (count + BITS_PER_LONG - 1) / BITS_PER_LONG;
		phba->sli4_hba.vfi_bmask = kzalloc(longs *
						   sizeof(unsigned long),
						   GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_bmask)) {
			rc = -ENOMEM;
			goto free_xri_ids;
		}
		phba->sli4_hba.vfi_ids = kzalloc(count *
						 sizeof(uint16_t),
						 GFP_KERNEL);
		if (unlikely(!phba->sli4_hba.vfi_ids)) {
			rc = -ENOMEM;
			goto free_vfi_bmask;
		}

		for (i = 0; i < count; i++)
			phba->sli4_hba.vfi_ids[i] = base + i;

		/*
		 * Mark all resources ready.  An HBA reset doesn't need
		 * to reset the initialization.
		 */
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags,
		       LPFC_IDX_RSRC_RDY);
		return 0;
	}

 free_vfi_bmask:
	kfree(phba->sli4_hba.vfi_bmask);
 free_xri_ids:
	kfree(phba->sli4_hba.xri_ids);
 free_xri_bmask:
	kfree(phba->sli4_hba.xri_bmask);
 free_vpi_ids:
	kfree(phba->vpi_ids);
 free_vpi_bmask:
	kfree(phba->vpi_bmask);
 free_rpi_ids:
	kfree(phba->sli4_hba.rpi_ids);
 free_rpi_bmask:
	kfree(phba->sli4_hba.rpi_bmask);
 err_exit:
	return rc;
}

/**
 * lpfc_sli4_dealloc_resource_identifiers - Deallocate all SLI4 resource extents.
 * @phba: Pointer to HBA context object.
 *
 * This function allocates the number of elements for the specified
 * resource type.
 **/
int
lpfc_sli4_dealloc_resource_identifiers(struct lpfc_hba *phba)
{
	if (phba->sli4_hba.extents_in_use) {
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_VPI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_RPI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_XRI);
		lpfc_sli4_dealloc_extent(phba, LPFC_RSC_TYPE_FCOE_VFI);
	} else {
		kfree(phba->vpi_bmask);
		phba->sli4_hba.max_cfg_param.vpi_used = 0;
		kfree(phba->vpi_ids);
		bf_set(lpfc_vpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		kfree(phba->sli4_hba.xri_bmask);
		kfree(phba->sli4_hba.xri_ids);
		kfree(phba->sli4_hba.vfi_bmask);
		kfree(phba->sli4_hba.vfi_ids);
		bf_set(lpfc_vfi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
		bf_set(lpfc_idx_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
	}

	return 0;
}

/**
 * lpfc_sli4_get_allocated_extnts - Get the port's allocated extents.
 * @phba: Pointer to HBA context object.
 * @type: The resource extent type.
 * @extnt_count: buffer to hold port extent count response
 * @extnt_size: buffer to hold port extent size response.
 *
 * This function calls the port to read the host allocated extents
 * for a particular type.
 **/
int
lpfc_sli4_get_allocated_extnts(struct lpfc_hba *phba, uint16_t type,
			       uint16_t *extnt_cnt, uint16_t *extnt_size)
{
	bool emb;
	int rc = 0;
	uint16_t curr_blks = 0;
	uint32_t req_len, emb_len;
	uint32_t alloc_len, mbox_tmo;
	struct list_head *blk_list_head;
	struct lpfc_rsrc_blks *rsrc_blk;
	LPFC_MBOXQ_t *mbox;
	void *virtaddr = NULL;
	struct lpfc_mbx_nembed_rsrc_extent *n_rsrc;
	struct lpfc_mbx_alloc_rsrc_extents *rsrc_ext;
	union  lpfc_sli4_cfg_shdr *shdr;

	switch (type) {
	case LPFC_RSC_TYPE_FCOE_VPI:
		blk_list_head = &phba->lpfc_vpi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_XRI:
		blk_list_head = &phba->sli4_hba.lpfc_xri_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_VFI:
		blk_list_head = &phba->sli4_hba.lpfc_vfi_blk_list;
		break;
	case LPFC_RSC_TYPE_FCOE_RPI:
		blk_list_head = &phba->sli4_hba.lpfc_rpi_blk_list;
		break;
	default:
		return -EIO;
	}

	/* Count the number of extents currently allocatd for this type. */
	list_for_each_entry(rsrc_blk, blk_list_head, list) {
		if (curr_blks == 0) {
			/*
			 * The GET_ALLOCATED mailbox does not return the size,
			 * just the count.  The size should be just the size
			 * stored in the current allocated block and all sizes
			 * for an extent type are the same so set the return
			 * value now.
			 */
			*extnt_size = rsrc_blk->rsrc_size;
		}
		curr_blks++;
	}

	/* Calculate the total requested length of the dma memory. */
	req_len = curr_blks * sizeof(uint16_t);

	/*
	 * Calculate the size of an embedded mailbox.  The uint32_t
	 * accounts for extents-specific word.
	 */
	emb_len = sizeof(MAILBOX_t) - sizeof(struct mbox_header) -
		sizeof(uint32_t);

	/*
	 * Presume the allocation and response will fit into an embedded
	 * mailbox.  If not true, reconfigure to a non-embedded mailbox.
	 */
	emb = LPFC_SLI4_MBX_EMBED;
	req_len = emb_len;
	if (req_len > emb_len) {
		req_len = curr_blks * sizeof(uint16_t) +
			sizeof(union lpfc_sli4_cfg_shdr) +
			sizeof(uint32_t);
		emb = LPFC_SLI4_MBX_NEMBED;
	}

	mbox = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	memset(mbox, 0, sizeof(LPFC_MBOXQ_t));

	alloc_len = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
				     LPFC_MBOX_OPCODE_GET_ALLOC_RSRC_EXTENT,
				     req_len, emb);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2983 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		rc = -ENOMEM;
		goto err_exit;
	}
	rc = lpfc_sli4_mbox_rsrc_extent(phba, mbox, curr_blks, type, emb);
	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}

	if (unlikely(rc)) {
		rc = -EIO;
		goto err_exit;
	}

	/*
	 * Figure out where the response is located.  Then get local pointers
	 * to the response data.  The port does not guarantee to respond to
	 * all extents counts request so update the local variable with the
	 * allocated count from the port.
	 */
	if (emb == LPFC_SLI4_MBX_EMBED) {
		rsrc_ext = &mbox->u.mqe.un.alloc_rsrc_extents;
		shdr = &rsrc_ext->header.cfg_shdr;
		*extnt_cnt = bf_get(lpfc_mbx_rsrc_cnt, &rsrc_ext->u.rsp);
	} else {
		virtaddr = mbox->sge_array->addr[0];
		n_rsrc = (struct lpfc_mbx_nembed_rsrc_extent *) virtaddr;
		shdr = &n_rsrc->cfg_shdr;
		*extnt_cnt = bf_get(lpfc_mbx_rsrc_cnt, n_rsrc);
	}

	if (bf_get(lpfc_mbox_hdr_status, &shdr->response)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_INIT,
			"2984 Failed to read allocated resources "
			"for type %d - Status 0x%x Add'l Status 0x%x.\n",
			type,
			bf_get(lpfc_mbox_hdr_status, &shdr->response),
			bf_get(lpfc_mbox_hdr_add_status, &shdr->response));
		rc = -EIO;
		goto err_exit;
	}
 err_exit:
	lpfc_sli4_mbox_cmd_free(phba, mbox);
	return rc;
}

/**
 * lpfc_sli4_repost_els_sgl_list - Repsot the els buffers sgl pages as block
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine walks the list of els buffers that have been allocated and
 * repost them to the port by using SGL block post. This is needed after a
 * pci_function_reset/warm_start or start. It attempts to construct blocks
 * of els buffer sgls which contains contiguous xris and uses the non-embedded
 * SGL block post mailbox commands to post them to the port. For single els
 * buffer sgl with non-contiguous xri, if any, it shall use embedded SGL post
 * mailbox command for posting.
 *
 * Returns: 0 = success, non-zero failure.
 **/
static int
lpfc_sli4_repost_els_sgl_list(struct lpfc_hba *phba)
{
	struct lpfc_sglq *sglq_entry = NULL;
	struct lpfc_sglq *sglq_entry_next = NULL;
	struct lpfc_sglq *sglq_entry_first = NULL;
	int status, total_cnt, post_cnt = 0, num_posted = 0, block_cnt = 0;
	int last_xritag = NO_XRI;
	LIST_HEAD(prep_sgl_list);
	LIST_HEAD(blck_sgl_list);
	LIST_HEAD(allc_sgl_list);
	LIST_HEAD(post_sgl_list);
	LIST_HEAD(free_sgl_list);

	spin_lock_irq(&phba->hbalock);
	list_splice_init(&phba->sli4_hba.lpfc_sgl_list, &allc_sgl_list);
	spin_unlock_irq(&phba->hbalock);

	total_cnt = phba->sli4_hba.els_xri_cnt;
	list_for_each_entry_safe(sglq_entry, sglq_entry_next,
				 &allc_sgl_list, list) {
		list_del_init(&sglq_entry->list);
		block_cnt++;
		if ((last_xritag != NO_XRI) &&
		    (sglq_entry->sli4_xritag != last_xritag + 1)) {
			/* a hole in xri block, form a sgl posting block */
			list_splice_init(&prep_sgl_list, &blck_sgl_list);
			post_cnt = block_cnt - 1;
			/* prepare list for next posting block */
			list_add_tail(&sglq_entry->list, &prep_sgl_list);
			block_cnt = 1;
		} else {
			/* prepare list for next posting block */
			list_add_tail(&sglq_entry->list, &prep_sgl_list);
			/* enough sgls for non-embed sgl mbox command */
			if (block_cnt == LPFC_NEMBED_MBOX_SGL_CNT) {
				list_splice_init(&prep_sgl_list,
						 &blck_sgl_list);
				post_cnt = block_cnt;
				block_cnt = 0;
			}
		}
		num_posted++;

		/* keep track of last sgl's xritag */
		last_xritag = sglq_entry->sli4_xritag;

		/* end of repost sgl list condition for els buffers */
		if (num_posted == phba->sli4_hba.els_xri_cnt) {
			if (post_cnt == 0) {
				list_splice_init(&prep_sgl_list,
						 &blck_sgl_list);
				post_cnt = block_cnt;
			} else if (block_cnt == 1) {
				status = lpfc_sli4_post_sgl(phba,
						sglq_entry->phys, 0,
						sglq_entry->sli4_xritag);
				if (!status) {
					/* successful, put sgl to posted list */
					list_add_tail(&sglq_entry->list,
						      &post_sgl_list);
				} else {
					/* Failure, put sgl to free list */
					lpfc_printf_log(phba, KERN_WARNING,
						LOG_SLI,
						"3159 Failed to post els "
						"sgl, xritag:x%x\n",
						sglq_entry->sli4_xritag);
					list_add_tail(&sglq_entry->list,
						      &free_sgl_list);
					total_cnt--;
				}
			}
		}

		/* continue until a nembed page worth of sgls */
		if (post_cnt == 0)
			continue;

		/* post the els buffer list sgls as a block */
		status = lpfc_sli4_post_els_sgl_list(phba, &blck_sgl_list,
						     post_cnt);

		if (!status) {
			/* success, put sgl list to posted sgl list */
			list_splice_init(&blck_sgl_list, &post_sgl_list);
		} else {
			/* Failure, put sgl list to free sgl list */
			sglq_entry_first = list_first_entry(&blck_sgl_list,
							    struct lpfc_sglq,
							    list);
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"3160 Failed to post els sgl-list, "
					"xritag:x%x-x%x\n",
					sglq_entry_first->sli4_xritag,
					(sglq_entry_first->sli4_xritag +
					 post_cnt - 1));
			list_splice_init(&blck_sgl_list, &free_sgl_list);
			total_cnt -= post_cnt;
		}

		/* don't reset xirtag due to hole in xri block */
		if (block_cnt == 0)
			last_xritag = NO_XRI;

		/* reset els sgl post count for next round of posting */
		post_cnt = 0;
	}
	/* update the number of XRIs posted for ELS */
	phba->sli4_hba.els_xri_cnt = total_cnt;

	/* free the els sgls failed to post */
	lpfc_free_sgl_list(phba, &free_sgl_list);

	/* push els sgls posted to the availble list */
	if (!list_empty(&post_sgl_list)) {
		spin_lock_irq(&phba->hbalock);
		list_splice_init(&post_sgl_list,
				 &phba->sli4_hba.lpfc_sgl_list);
		spin_unlock_irq(&phba->hbalock);
	} else {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"3161 Failure to post els sgl to port.\n");
		return -EIO;
	}
	return 0;
}

/**
 * lpfc_sli4_hba_setup - SLI4 device intialization PCI function
 * @phba: Pointer to HBA context object.
 *
 * This function is the main SLI4 device intialization PCI function. This
 * function is called by the HBA intialization code, HBA reset code and
 * HBA error attention handler code. Caller is not required to hold any
 * locks.
 **/
int
lpfc_sli4_hba_setup(struct lpfc_hba *phba)
{
	int rc;
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mqe *mqe;
	uint8_t *vpd;
	uint32_t vpd_size;
	uint32_t ftr_rsp = 0;
	struct Scsi_Host *shost = lpfc_shost_from_vport(phba->pport);
	struct lpfc_vport *vport = phba->pport;
	struct lpfc_dmabuf *mp;

	/* Perform a PCI function reset to start from clean */
	rc = lpfc_pci_function_reset(phba);
	if (unlikely(rc))
		return -ENODEV;

	/* Check the HBA Host Status Register for readyness */
	rc = lpfc_sli4_post_status_check(phba);
	if (unlikely(rc))
		return -ENODEV;
	else {
		spin_lock_irq(&phba->hbalock);
		phba->sli.sli_flag |= LPFC_SLI_ACTIVE;
		spin_unlock_irq(&phba->hbalock);
	}

	/*
	 * Allocate a single mailbox container for initializing the
	 * port.
	 */
	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	/* Issue READ_REV to collect vpd and FW information. */
	vpd_size = SLI4_PAGE_SIZE;
	vpd = kzalloc(vpd_size, GFP_KERNEL);
	if (!vpd) {
		rc = -ENOMEM;
		goto out_free_mbox;
	}

	rc = lpfc_sli4_read_rev(phba, mboxq, vpd, &vpd_size);
	if (unlikely(rc)) {
		kfree(vpd);
		goto out_free_mbox;
	}
	mqe = &mboxq->u.mqe;
	phba->sli_rev = bf_get(lpfc_mbx_rd_rev_sli_lvl, &mqe->un.read_rev);
	if (bf_get(lpfc_mbx_rd_rev_fcoe, &mqe->un.read_rev))
		phba->hba_flag |= HBA_FCOE_MODE;
	else
		phba->hba_flag &= ~HBA_FCOE_MODE;

	if (bf_get(lpfc_mbx_rd_rev_cee_ver, &mqe->un.read_rev) ==
		LPFC_DCBX_CEE_MODE)
		phba->hba_flag |= HBA_FIP_SUPPORT;
	else
		phba->hba_flag &= ~HBA_FIP_SUPPORT;

	phba->hba_flag &= ~HBA_FCP_IOQ_FLUSH;

	if (phba->sli_rev != LPFC_SLI_REV4) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
			"0376 READ_REV Error. SLI Level %d "
			"FCoE enabled %d\n",
			phba->sli_rev, phba->hba_flag & HBA_FCOE_MODE);
		rc = -EIO;
		kfree(vpd);
		goto out_free_mbox;
	}

	/*
	 * Continue initialization with default values even if driver failed
	 * to read FCoE param config regions, only read parameters if the
	 * board is FCoE
	 */
	if (phba->hba_flag & HBA_FCOE_MODE &&
	    lpfc_sli4_read_fcoe_params(phba))
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_INIT,
			"2570 Failed to read FCoE parameters\n");

	/*
	 * Retrieve sli4 device physical port name, failure of doing it
	 * is considered as non-fatal.
	 */
	rc = lpfc_sli4_retrieve_pport_name(phba);
	if (!rc)
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"3080 Successful retrieving SLI4 device "
				"physical port name: %s.\n", phba->Port);

	/*
	 * Evaluate the read rev and vpd data. Populate the driver
	 * state with the results. If this routine fails, the failure
	 * is not fatal as the driver will use generic values.
	 */
	rc = lpfc_parse_vpd(phba, vpd, vpd_size);
	if (unlikely(!rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0377 Error %d parsing vpd. "
				"Using defaults.\n", rc);
		rc = 0;
	}
	kfree(vpd);

	/* Save information as VPD data */
	phba->vpd.rev.biuRev = mqe->un.read_rev.first_hw_rev;
	phba->vpd.rev.smRev = mqe->un.read_rev.second_hw_rev;
	phba->vpd.rev.endecRev = mqe->un.read_rev.third_hw_rev;
	phba->vpd.rev.fcphHigh = bf_get(lpfc_mbx_rd_rev_fcph_high,
					 &mqe->un.read_rev);
	phba->vpd.rev.fcphLow = bf_get(lpfc_mbx_rd_rev_fcph_low,
				       &mqe->un.read_rev);
	phba->vpd.rev.feaLevelHigh = bf_get(lpfc_mbx_rd_rev_ftr_lvl_high,
					    &mqe->un.read_rev);
	phba->vpd.rev.feaLevelLow = bf_get(lpfc_mbx_rd_rev_ftr_lvl_low,
					   &mqe->un.read_rev);
	phba->vpd.rev.sli1FwRev = mqe->un.read_rev.fw_id_rev;
	memcpy(phba->vpd.rev.sli1FwName, mqe->un.read_rev.fw_name, 16);
	phba->vpd.rev.sli2FwRev = mqe->un.read_rev.ulp_fw_id_rev;
	memcpy(phba->vpd.rev.sli2FwName, mqe->un.read_rev.ulp_fw_name, 16);
	phba->vpd.rev.opFwRev = mqe->un.read_rev.fw_id_rev;
	memcpy(phba->vpd.rev.opFwName, mqe->un.read_rev.fw_name, 16);
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0380 READ_REV Status x%x "
			"fw_rev:%s fcphHi:%x fcphLo:%x flHi:%x flLo:%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0,
			bf_get(lpfc_mqe_status, mqe),
			phba->vpd.rev.opFwName,
			phba->vpd.rev.fcphHigh, phba->vpd.rev.fcphLow,
			phba->vpd.rev.feaLevelHigh, phba->vpd.rev.feaLevelLow);

	/*
	 * Discover the port's supported feature set and match it against the
	 * hosts requests.
	 */
	lpfc_request_features(phba, mboxq);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (unlikely(rc)) {
		rc = -EIO;
		goto out_free_mbox;
	}

	/*
	 * The port must support FCP initiator mode as this is the
	 * only mode running in the host.
	 */
	if (!(bf_get(lpfc_mbx_rq_ftr_rsp_fcpi, &mqe->un.req_ftrs))) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"0378 No support for fcpi mode.\n");
		ftr_rsp++;
	}
	if (bf_get(lpfc_mbx_rq_ftr_rsp_perfh, &mqe->un.req_ftrs))
		phba->sli3_options |= LPFC_SLI4_PERFH_ENABLED;
	else
		phba->sli3_options &= ~LPFC_SLI4_PERFH_ENABLED;
	/*
	 * If the port cannot support the host's requested features
	 * then turn off the global config parameters to disable the
	 * feature in the driver.  This is not a fatal error.
	 */
	phba->sli3_options &= ~LPFC_SLI3_BG_ENABLED;
	if (phba->cfg_enable_bg) {
		if (bf_get(lpfc_mbx_rq_ftr_rsp_dif, &mqe->un.req_ftrs))
			phba->sli3_options |= LPFC_SLI3_BG_ENABLED;
		else
			ftr_rsp++;
	}

	if (phba->max_vpi && phba->cfg_enable_npiv &&
	    !(bf_get(lpfc_mbx_rq_ftr_rsp_npiv, &mqe->un.req_ftrs)))
		ftr_rsp++;

	if (ftr_rsp) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"0379 Feature Mismatch Data: x%08x %08x "
				"x%x x%x x%x\n", mqe->un.req_ftrs.word2,
				mqe->un.req_ftrs.word3, phba->cfg_enable_bg,
				phba->cfg_enable_npiv, phba->max_vpi);
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_dif, &mqe->un.req_ftrs)))
			phba->cfg_enable_bg = 0;
		if (!(bf_get(lpfc_mbx_rq_ftr_rsp_npiv, &mqe->un.req_ftrs)))
			phba->cfg_enable_npiv = 0;
	}

	/* These SLI3 features are assumed in SLI4 */
	spin_lock_irq(&phba->hbalock);
	phba->sli3_options |= (LPFC_SLI3_NPIV_ENABLED | LPFC_SLI3_HBQ_ENABLED);
	spin_unlock_irq(&phba->hbalock);

	/*
	 * Allocate all resources (xri,rpi,vpi,vfi) now.  Subsequent
	 * calls depends on these resources to complete port setup.
	 */
	rc = lpfc_sli4_alloc_resource_identifiers(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"2920 Failed to alloc Resource IDs "
				"rc = x%x\n", rc);
		goto out_free_mbox;
	}

	/* Read the port's service parameters. */
	rc = lpfc_read_sparam(phba, mboxq, vport->vpi);
	if (rc) {
		phba->link_state = LPFC_HBA_ERROR;
		rc = -ENOMEM;
		goto out_free_mbox;
	}

	mboxq->vport = vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	mp = (struct lpfc_dmabuf *) mboxq->context1;
	if (rc == MBX_SUCCESS) {
		memcpy(&vport->fc_sparam, mp->virt, sizeof(struct serv_parm));
		rc = 0;
	}

	/*
	 * This memory was allocated by the lpfc_read_sparam routine. Release
	 * it to the mbuf pool.
	 */
	lpfc_mbuf_free(phba, mp->virt, mp->phys);
	kfree(mp);
	mboxq->context1 = NULL;
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0382 READ_SPARAM command failed "
				"status %d, mbxStatus x%x\n",
				rc, bf_get(lpfc_mqe_status, mqe));
		phba->link_state = LPFC_HBA_ERROR;
		rc = -EIO;
		goto out_free_mbox;
	}

	lpfc_update_vport_wwn(vport);

	/* Update the fc_host data structures with new wwn. */
	fc_host_node_name(shost) = wwn_to_u64(vport->fc_nodename.u.wwn);
	fc_host_port_name(shost) = wwn_to_u64(vport->fc_portname.u.wwn);

	/* update host els and scsi xri-sgl sizes and mappings */
	rc = lpfc_sli4_xri_sgl_update(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"1400 Failed to update xri-sgl size and "
				"mapping: %d\n", rc);
		goto out_free_mbox;
	}

	/* register the els sgl pool to the port */
	rc = lpfc_sli4_repost_els_sgl_list(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0582 Error %d during els sgl post "
				"operation\n", rc);
		rc = -ENODEV;
		goto out_free_mbox;
	}

	/* register the allocated scsi sgl pool to the port */
	rc = lpfc_sli4_repost_scsi_sgl_list(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0383 Error %d during scsi sgl post "
				"operation\n", rc);
		/* Some Scsi buffers were moved to the abort scsi list */
		/* A pci function reset will repost them */
		rc = -ENODEV;
		goto out_free_mbox;
	}

	/* Post the rpi header region to the device. */
	rc = lpfc_sli4_post_all_rpi_hdrs(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0393 Error %d during rpi post operation\n",
				rc);
		rc = -ENODEV;
		goto out_free_mbox;
	}
	lpfc_sli4_node_prep(phba);

	/* Create all the SLI4 queues */
	rc = lpfc_sli4_queue_create(phba);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3089 Failed to allocate queues\n");
		rc = -ENODEV;
		goto out_stop_timers;
	}
	/* Set up all the queues to the device */
	rc = lpfc_sli4_queue_setup(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0381 Error %d during queue setup.\n ", rc);
		goto out_destroy_queue;
	}

	/* Arm the CQs and then EQs on device */
	lpfc_sli4_arm_cqeq_intr(phba);

	/* Indicate device interrupt mode */
	phba->sli4_hba.intr_enable = 1;

	/* Allow asynchronous mailbox command to go through */
	spin_lock_irq(&phba->hbalock);
	phba->sli.sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);

	/* Post receive buffers to the device */
	lpfc_sli4_rb_setup(phba);

	/* Reset HBA FCF states after HBA reset */
	phba->fcf.fcf_flag = 0;
	phba->fcf.current_rec.flag = 0;

	/* Start the ELS watchdog timer */
	mod_timer(&vport->els_tmofunc,
		  jiffies + msecs_to_jiffies(1000 * (phba->fc_ratov * 2)));

	/* Start heart beat timer */
	mod_timer(&phba->hb_tmofunc,
		  jiffies + msecs_to_jiffies(1000 * LPFC_HB_MBOX_INTERVAL));
	phba->hb_outstanding = 0;
	phba->last_completion_time = jiffies;

	/* Start error attention (ERATT) polling timer */
	mod_timer(&phba->eratt_poll,
		  jiffies + msecs_to_jiffies(1000 * LPFC_ERATT_POLL_INTERVAL));

	/* Enable PCIe device Advanced Error Reporting (AER) if configured */
	if (phba->cfg_aer_support == 1 && !(phba->hba_flag & HBA_AER_ENABLED)) {
		rc = pci_enable_pcie_error_reporting(phba->pcidev);
		if (!rc) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2829 This device supports "
					"Advanced Error Reporting (AER)\n");
			spin_lock_irq(&phba->hbalock);
			phba->hba_flag |= HBA_AER_ENABLED;
			spin_unlock_irq(&phba->hbalock);
		} else {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2830 This device does not support "
					"Advanced Error Reporting (AER)\n");
			phba->cfg_aer_support = 0;
		}
		rc = 0;
	}

	if (!(phba->hba_flag & HBA_FCOE_MODE)) {
		/*
		 * The FC Port needs to register FCFI (index 0)
		 */
		lpfc_reg_fcfi(phba, mboxq);
		mboxq->vport = phba->pport;
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS)
			goto out_unset_queue;
		rc = 0;
		phba->fcf.fcfi = bf_get(lpfc_reg_fcfi_fcfi,
					&mboxq->u.mqe.un.reg_fcfi);

		/* Check if the port is configured to be disabled */
		lpfc_sli_read_link_ste(phba);
	}

	/*
	 * The port is ready, set the host's link state to LINK_DOWN
	 * in preparation for link interrupts.
	 */
	spin_lock_irq(&phba->hbalock);
	phba->link_state = LPFC_LINK_DOWN;
	spin_unlock_irq(&phba->hbalock);
	if (!(phba->hba_flag & HBA_FCOE_MODE) &&
	    (phba->hba_flag & LINK_DISABLED)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_SLI,
				"3103 Adapter Link is disabled.\n");
		lpfc_down_link(phba, mboxq);
		rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT | LOG_SLI,
					"3104 Adapter failed to issue "
					"DOWN_LINK mbox cmd, rc:x%x\n", rc);
			goto out_unset_queue;
		}
	} else if (phba->cfg_suppress_link_up == LPFC_INITIALIZE_LINK) {
		/* don't perform init_link on SLI4 FC port loopback test */
		if (!(phba->link_flag & LS_LOOPBACK_MODE)) {
			rc = phba->lpfc_hba_init_link(phba, MBX_NOWAIT);
			if (rc)
				goto out_unset_queue;
		}
	}
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
out_unset_queue:
	/* Unset all the queues set up in this routine when error out */
	lpfc_sli4_queue_unset(phba);
out_destroy_queue:
	lpfc_sli4_queue_destroy(phba);
out_stop_timers:
	lpfc_stop_hba_timers(phba);
out_free_mbox:
	mempool_free(mboxq, phba->mbox_mem_pool);
	return rc;
}

/**
 * lpfc_mbox_timeout - Timeout call back function for mbox timer
 * @ptr: context object - pointer to hba structure.
 *
 * This is the callback function for mailbox timer. The mailbox
 * timer is armed when a new mailbox command is issued and the timer
 * is deleted when the mailbox complete. The function is called by
 * the kernel timer code when a mailbox does not complete within
 * expected time. This function wakes up the worker thread to
 * process the mailbox timeout and returns. All the processing is
 * done by the worker thread function lpfc_mbox_timeout_handler.
 **/
void
lpfc_mbox_timeout(unsigned long ptr)
{
	struct lpfc_hba  *phba = (struct lpfc_hba *) ptr;
	unsigned long iflag;
	uint32_t tmo_posted;

	spin_lock_irqsave(&phba->pport->work_port_lock, iflag);
	tmo_posted = phba->pport->work_port_events & WORKER_MBOX_TMO;
	if (!tmo_posted)
		phba->pport->work_port_events |= WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflag);

	if (!tmo_posted)
		lpfc_worker_wake_up(phba);
	return;
}


/**
 * lpfc_mbox_timeout_handler - Worker thread function to handle mailbox timeout
 * @phba: Pointer to HBA context object.
 *
 * This function is called from worker thread when a mailbox command times out.
 * The caller is not required to hold any locks. This function will reset the
 * HBA and recover all the pending commands.
 **/
void
lpfc_mbox_timeout_handler(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *pmbox = phba->sli.mbox_active;
	MAILBOX_t *mb = &pmbox->u.mb;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;

	/* Check the pmbox pointer first.  There is a race condition
	 * between the mbox timeout handler getting executed in the
	 * worklist and the mailbox actually completing. When this
	 * race condition occurs, the mbox_active will be NULL.
	 */
	spin_lock_irq(&phba->hbalock);
	if (pmbox == NULL) {
		lpfc_printf_log(phba, KERN_WARNING,
				LOG_MBOX | LOG_SLI,
				"0353 Active Mailbox cleared - mailbox timeout "
				"exiting\n");
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	/* Mbox cmd <mbxCommand> timeout */
	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
			"0310 Mailbox command x%x timeout Data: x%x x%x x%p\n",
			mb->mbxCommand,
			phba->pport->port_state,
			phba->sli.sli_flag,
			phba->sli.mbox_active);
	spin_unlock_irq(&phba->hbalock);

	/* Setting state unknown so lpfc_sli_abort_iocb_ring
	 * would get IOCB_ERROR from lpfc_sli_issue_iocb, allowing
	 * it to fail all outstanding SCSI IO.
	 */
	spin_lock_irq(&phba->pport->work_port_lock);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irq(&phba->pport->work_port_lock);
	spin_lock_irq(&phba->hbalock);
	phba->link_state = LPFC_LINK_UNKNOWN;
	psli->sli_flag &= ~LPFC_SLI_ACTIVE;
	spin_unlock_irq(&phba->hbalock);

	pring = &psli->ring[psli->fcp_ring];
	lpfc_sli_abort_iocb_ring(phba, pring);

	lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
			"0345 Resetting board due to mailbox timeout\n");

	/* Reset the HBA device */
	lpfc_reset_hba(phba);
}

/**
 * lpfc_sli_issue_mbox_s3 - Issue an SLI3 mailbox command to firmware
 * @phba: Pointer to HBA context object.
 * @pmbox: Pointer to mailbox object.
 * @flag: Flag indicating how the mailbox need to be processed.
 *
 * This function is called by discovery code and HBA management code
 * to submit a mailbox command to firmware with SLI-3 interface spec. This
 * function gets the hbalock to protect the data structures.
 * The mailbox command can be submitted in polling mode, in which case
 * this function will wait in a polling loop for the completion of the
 * mailbox.
 * If the mailbox is submitted in no_wait mode (not polling) the
 * function will submit the command and returns immediately without waiting
 * for the mailbox completion. The no_wait is supported only when HBA
 * is in SLI2/SLI3 mode - interrupts are enabled.
 * The SLI interface allows only one mailbox pending at a time. If the
 * mailbox is issued in polling mode and there is already a mailbox
 * pending, then the function will return an error. If the mailbox is issued
 * in NO_WAIT mode and there is a mailbox pending already, the function
 * will return MBX_BUSY after queuing the mailbox into mailbox queue.
 * The sli layer owns the mailbox object until the completion of mailbox
 * command if this function return MBX_BUSY or MBX_SUCCESS. For all other
 * return codes the caller owns the mailbox command after the return of
 * the function.
 **/
static int
lpfc_sli_issue_mbox_s3(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox,
		       uint32_t flag)
{
	MAILBOX_t *mbx;
	struct lpfc_sli *psli = &phba->sli;
	uint32_t status, evtctr;
	uint32_t ha_copy, hc_copy;
	int i;
	unsigned long timeout;
	unsigned long drvr_flag = 0;
	uint32_t word0, ldata;
	void __iomem *to_slim;
	int processing_queue = 0;

	spin_lock_irqsave(&phba->hbalock, drvr_flag);
	if (!pmbox) {
		phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		/* processing mbox queue from intr_handler */
		if (unlikely(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			return MBX_SUCCESS;
		}
		processing_queue = 1;
		pmbox = lpfc_mbox_get(phba);
		if (!pmbox) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			return MBX_SUCCESS;
		}
	}

	if (pmbox->mbox_cmpl && pmbox->mbox_cmpl != lpfc_sli_def_mbox_cmpl &&
		pmbox->mbox_cmpl != lpfc_sli_wake_mbox_wait) {
		if(!pmbox->vport) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			lpfc_printf_log(phba, KERN_ERR,
					LOG_MBOX | LOG_VPORT,
					"1806 Mbox x%x failed. No vport\n",
					pmbox->u.mb.mbxCommand);
			dump_stack();
			goto out_not_finished;
		}
	}

	/* If the PCI channel is in offline state, do not post mbox. */
	if (unlikely(pci_channel_offline(phba->pcidev))) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
		goto out_not_finished;
	}

	/* If HBA has a deferred error attention, fail the iocb. */
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
		goto out_not_finished;
	}

	psli = &phba->sli;

	mbx = &pmbox->u.mb;
	status = MBX_SUCCESS;

	if (phba->link_state == LPFC_HBA_ERROR) {
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

		/* Mbox command <mbxCommand> cannot issue */
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):0311 Mailbox command x%x cannot "
				"issue Data: x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0,
				pmbox->u.mb.mbxCommand, psli->sli_flag, flag);
		goto out_not_finished;
	}

	if (mbx->mbxCommand != MBX_KILL_BOARD && flag & MBX_NOWAIT) {
		if (lpfc_readl(phba->HCregaddr, &hc_copy) ||
			!(hc_copy & HC_MBINT_ENA)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2528 Mailbox command x%x cannot "
				"issue Data: x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0,
				pmbox->u.mb.mbxCommand, psli->sli_flag, flag);
			goto out_not_finished;
		}
	}

	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		/* Polling for a mbox command when another one is already active
		 * is not allowed in SLI. Also, the driver must have established
		 * SLI2 mode to queue and process multiple mbox commands.
		 */

		if (flag & MBX_POLL) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

			/* Mbox command <mbxCommand> cannot issue */
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):2529 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}

		if (!(psli->sli_flag & LPFC_SLI_ACTIVE)) {
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			/* Mbox command <mbxCommand> cannot issue */
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):2530 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}

		/* Another mailbox command is still being processed, queue this
		 * command to be processed later.
		 */
		lpfc_mbox_put(phba, pmbox);

		/* Mbox cmd issue - BUSY */
		lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
				"(%d):0308 Mbox cmd issue - BUSY Data: "
				"x%x x%x x%x x%x\n",
				pmbox->vport ? pmbox->vport->vpi : 0xffffff,
				mbx->mbxCommand, phba->pport->port_state,
				psli->sli_flag, flag);

		psli->slistat.mbox_busy++;
		spin_unlock_irqrestore(&phba->hbalock, drvr_flag);

		if (pmbox->vport) {
			lpfc_debugfs_disc_trc(pmbox->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Bsy vport:  cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}
		else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Bsy:        cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}

		return MBX_BUSY;
	}

	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;

	/* If we are not polling, we MUST be in SLI2 mode */
	if (flag != MBX_POLL) {
		if (!(psli->sli_flag & LPFC_SLI_ACTIVE) &&
		    (mbx->mbxCommand != MBX_KILL_BOARD)) {
			psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
			spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
			/* Mbox command <mbxCommand> cannot issue */
			lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
					"(%d):2531 Mailbox command x%x "
					"cannot issue Data: x%x x%x\n",
					pmbox->vport ? pmbox->vport->vpi : 0,
					pmbox->u.mb.mbxCommand,
					psli->sli_flag, flag);
			goto out_not_finished;
		}
		/* timeout active mbox command */
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, pmbox) *
					   1000);
		mod_timer(&psli->mbox_tmo, jiffies + timeout);
	}

	/* Mailbox cmd <cmd> issue */
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0309 Mailbox cmd x%x issue Data: x%x x%x "
			"x%x\n",
			pmbox->vport ? pmbox->vport->vpi : 0,
			mbx->mbxCommand, phba->pport->port_state,
			psli->sli_flag, flag);

	if (mbx->mbxCommand != MBX_HEARTBEAT) {
		if (pmbox->vport) {
			lpfc_debugfs_disc_trc(pmbox->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Send vport: cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}
		else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Send:       cmd:x%x mb:x%x x%x",
				(uint32_t)mbx->mbxCommand,
				mbx->un.varWords[0], mbx->un.varWords[1]);
		}
	}

	psli->slistat.mbox_cmd++;
	evtctr = psli->slistat.mbox_event;

	/* next set own bit for the adapter and copy over command word */
	mbx->mbxOwner = OWN_CHIP;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		/* Populate mbox extension offset word. */
		if (pmbox->in_ext_byte_len || pmbox->out_ext_byte_len) {
			*(((uint32_t *)mbx) + pmbox->mbox_offset_word)
				= (uint8_t *)phba->mbox_ext
				  - (uint8_t *)phba->mbox;
		}

		/* Copy the mailbox extension data */
		if (pmbox->in_ext_byte_len && pmbox->context2) {
			lpfc_sli_pcimem_bcopy(pmbox->context2,
				(uint8_t *)phba->mbox_ext,
				pmbox->in_ext_byte_len);
		}
		/* Copy command data to host SLIM area */
		lpfc_sli_pcimem_bcopy(mbx, phba->mbox, MAILBOX_CMD_SIZE);
	} else {
		/* Populate mbox extension offset word. */
		if (pmbox->in_ext_byte_len || pmbox->out_ext_byte_len)
			*(((uint32_t *)mbx) + pmbox->mbox_offset_word)
				= MAILBOX_HBA_EXT_OFFSET;

		/* Copy the mailbox extension data */
		if (pmbox->in_ext_byte_len && pmbox->context2) {
			lpfc_memcpy_to_slim(phba->MBslimaddr +
				MAILBOX_HBA_EXT_OFFSET,
				pmbox->context2, pmbox->in_ext_byte_len);

		}
		if (mbx->mbxCommand == MBX_CONFIG_PORT) {
			/* copy command data into host mbox for cmpl */
			lpfc_sli_pcimem_bcopy(mbx, phba->mbox, MAILBOX_CMD_SIZE);
		}

		/* First copy mbox command data to HBA SLIM, skip past first
		   word */
		to_slim = phba->MBslimaddr + sizeof (uint32_t);
		lpfc_memcpy_to_slim(to_slim, &mbx->un.varWords[0],
			    MAILBOX_CMD_SIZE - sizeof (uint32_t));

		/* Next copy over first word, with mbxOwner set */
		ldata = *((uint32_t *)mbx);
		to_slim = phba->MBslimaddr;
		writel(ldata, to_slim);
		readl(to_slim); /* flush */

		if (mbx->mbxCommand == MBX_CONFIG_PORT) {
			/* switch over to host mailbox */
			psli->sli_flag |= LPFC_SLI_ACTIVE;
		}
	}

	wmb();

	switch (flag) {
	case MBX_NOWAIT:
		/* Set up reference to mailbox command */
		psli->mbox_active = pmbox;
		/* Interrupt board to do it */
		writel(CA_MBATT, phba->CAregaddr);
		readl(phba->CAregaddr); /* flush */
		/* Don't wait for it to finish, just return */
		break;

	case MBX_POLL:
		/* Set up null reference to mailbox command */
		psli->mbox_active = NULL;
		/* Interrupt board to do it */
		writel(CA_MBATT, phba->CAregaddr);
		readl(phba->CAregaddr); /* flush */

		if (psli->sli_flag & LPFC_SLI_ACTIVE) {
			/* First read mbox status word */
			word0 = *((uint32_t *)phba->mbox);
			word0 = le32_to_cpu(word0);
		} else {
			/* First read mbox status word */
			if (lpfc_readl(phba->MBslimaddr, &word0)) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}
		}

		/* Read the HBA Host Attention Register */
		if (lpfc_readl(phba->HAregaddr, &ha_copy)) {
			spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
			goto out_not_finished;
		}
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, pmbox) *
							1000) + jiffies;
		i = 0;
		/* Wait for command to complete */
		while (((word0 & OWN_CHIP) == OWN_CHIP) ||
		       (!(ha_copy & HA_MBATT) &&
			(phba->link_state > LPFC_WARM_START))) {
			if (time_after(jiffies, timeout)) {
				psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}

			/* Check if we took a mbox interrupt while we were
			   polling */
			if (((word0 & OWN_CHIP) != OWN_CHIP)
			    && (evtctr != psli->slistat.mbox_event))
				break;

			if (i++ > 10) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				msleep(1);
				spin_lock_irqsave(&phba->hbalock, drvr_flag);
			}

			if (psli->sli_flag & LPFC_SLI_ACTIVE) {
				/* First copy command data */
				word0 = *((uint32_t *)phba->mbox);
				word0 = le32_to_cpu(word0);
				if (mbx->mbxCommand == MBX_CONFIG_PORT) {
					MAILBOX_t *slimmb;
					uint32_t slimword0;
					/* Check real SLIM for any errors */
					slimword0 = readl(phba->MBslimaddr);
					slimmb = (MAILBOX_t *) & slimword0;
					if (((slimword0 & OWN_CHIP) != OWN_CHIP)
					    && slimmb->mbxStatus) {
						psli->sli_flag &=
						    ~LPFC_SLI_ACTIVE;
						word0 = slimword0;
					}
				}
			} else {
				/* First copy command data */
				word0 = readl(phba->MBslimaddr);
			}
			/* Read the HBA Host Attention Register */
			if (lpfc_readl(phba->HAregaddr, &ha_copy)) {
				spin_unlock_irqrestore(&phba->hbalock,
						       drvr_flag);
				goto out_not_finished;
			}
		}

		if (psli->sli_flag & LPFC_SLI_ACTIVE) {
			/* copy results back to user */
			lpfc_sli_pcimem_bcopy(phba->mbox, mbx, MAILBOX_CMD_SIZE);
			/* Copy the mailbox extension data */
			if (pmbox->out_ext_byte_len && pmbox->context2) {
				lpfc_sli_pcimem_bcopy(phba->mbox_ext,
						      pmbox->context2,
						      pmbox->out_ext_byte_len);
			}
		} else {
			/* First copy command data */
			lpfc_memcpy_from_slim(mbx, phba->MBslimaddr,
							MAILBOX_CMD_SIZE);
			/* Copy the mailbox extension data */
			if (pmbox->out_ext_byte_len && pmbox->context2) {
				lpfc_memcpy_from_slim(pmbox->context2,
					phba->MBslimaddr +
					MAILBOX_HBA_EXT_OFFSET,
					pmbox->out_ext_byte_len);
			}
		}

		writel(HA_MBATT, phba->HAregaddr);
		readl(phba->HAregaddr); /* flush */

		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		status = mbx->mbxStatus;
	}

	spin_unlock_irqrestore(&phba->hbalock, drvr_flag);
	return status;

out_not_finished:
	if (processing_queue) {
		pmbox->u.mb.mbxStatus = MBX_NOT_FINISHED;
		lpfc_mbox_cmpl_put(phba, pmbox);
	}
	return MBX_NOT_FINISHED;
}

/**
 * lpfc_sli4_async_mbox_block - Block posting SLI4 asynchronous mailbox command
 * @phba: Pointer to HBA context object.
 *
 * The function blocks the posting of SLI4 asynchronous mailbox commands from
 * the driver internal pending mailbox queue. It will then try to wait out the
 * possible outstanding mailbox command before return.
 *
 * Returns:
 * 	0 - the outstanding mailbox command completed; otherwise, the wait for
 * 	the outstanding mailbox command timed out.
 **/
static int
lpfc_sli4_async_mbox_block(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	int rc = 0;
	unsigned long timeout = 0;

	/* Mark the asynchronous mailbox command posting as blocked */
	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;
	/* Determine how long we might wait for the active mailbox
	 * command to be gracefully completed by firmware.
	 */
	if (phba->sli.mbox_active)
		timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
						phba->sli.mbox_active) *
						1000) + jiffies;
	spin_unlock_irq(&phba->hbalock);

	/* Wait for the outstnading mailbox command to complete */
	while (phba->sli.mbox_active) {
		/* Check active mailbox complete status every 2ms */
		msleep(2);
		if (time_after(jiffies, timeout)) {
			/* Timeout, marked the outstanding cmd not complete */
			rc = 1;
			break;
		}
	}

	/* Can not cleanly block async mailbox command, fails it */
	if (rc) {
		spin_lock_irq(&phba->hbalock);
		psli->sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
		spin_unlock_irq(&phba->hbalock);
	}
	return rc;
}

/**
 * lpfc_sli4_async_mbox_unblock - Block posting SLI4 async mailbox command
 * @phba: Pointer to HBA context object.
 *
 * The function unblocks and resume posting of SLI4 asynchronous mailbox
 * commands from the driver internal pending mailbox queue. It makes sure
 * that there is no outstanding mailbox command before resuming posting
 * asynchronous mailbox commands. If, for any reason, there is outstanding
 * mailbox command, it will try to wait it out before resuming asynchronous
 * mailbox command posting.
 **/
static void
lpfc_sli4_async_mbox_unblock(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;

	spin_lock_irq(&phba->hbalock);
	if (!(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
		/* Asynchronous mailbox posting is not blocked, do nothing */
		spin_unlock_irq(&phba->hbalock);
		return;
	}

	/* Outstanding synchronous mailbox command is guaranteed to be done,
	 * successful or timeout, after timing-out the outstanding mailbox
	 * command shall always be removed, so just unblock posting async
	 * mailbox command and resume
	 */
	psli->sli_flag &= ~LPFC_SLI_ASYNC_MBX_BLK;
	spin_unlock_irq(&phba->hbalock);

	/* wake up worker thread to post asynchronlous mailbox command */
	lpfc_worker_wake_up(phba);
}

/**
 * lpfc_sli4_wait_bmbx_ready - Wait for bootstrap mailbox register ready
 * @phba: Pointer to HBA context object.
 * @mboxq: Pointer to mailbox object.
 *
 * The function waits for the bootstrap mailbox register ready bit from
 * port for twice the regular mailbox command timeout value.
 *
 *      0 - no timeout on waiting for bootstrap mailbox register ready.
 *      MBXERR_ERROR - wait for bootstrap mailbox register timed out.
 **/
static int
lpfc_sli4_wait_bmbx_ready(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	uint32_t db_ready;
	unsigned long timeout;
	struct lpfc_register bmbx_reg;

	timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba, mboxq)
				   * 1000) + jiffies;

	do {
		bmbx_reg.word0 = readl(phba->sli4_hba.BMBXregaddr);
		db_ready = bf_get(lpfc_bmbx_rdy, &bmbx_reg);
		if (!db_ready)
			msleep(2);

		if (time_after(jiffies, timeout))
			return MBXERR_ERROR;
	} while (!db_ready);

	return 0;
}

/**
 * lpfc_sli4_post_sync_mbox - Post an SLI4 mailbox to the bootstrap mailbox
 * @phba: Pointer to HBA context object.
 * @mboxq: Pointer to mailbox object.
 *
 * The function posts a mailbox to the port.  The mailbox is expected
 * to be comletely filled in and ready for the port to operate on it.
 * This routine executes a synchronous completion operation on the
 * mailbox by polling for its completion.
 *
 * The caller must not be holding any locks when calling this routine.
 *
 * Returns:
 *	MBX_SUCCESS - mailbox posted successfully
 *	Any of the MBX error values.
 **/
static int
lpfc_sli4_post_sync_mbox(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	int rc = MBX_SUCCESS;
	unsigned long iflag;
	uint32_t mcqe_status;
	uint32_t mbx_cmnd;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_mqe *mb = &mboxq->u.mqe;
	struct lpfc_bmbx_create *mbox_rgn;
	struct dma_address *dma_address;

	/*
	 * Only one mailbox can be active to the bootstrap mailbox region
	 * at a time and there is no queueing provided.
	 */
	spin_lock_irqsave(&phba->hbalock, iflag);
	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2532 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, MBX_POLL);
		return MBXERR_ERROR;
	}
	/* The server grabs the token and owns it until release */
	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;
	phba->sli.mbox_active = mboxq;
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	/* wait for bootstrap mbox register for readyness */
	rc = lpfc_sli4_wait_bmbx_ready(phba, mboxq);
	if (rc)
		goto exit;

	/*
	 * Initialize the bootstrap memory region to avoid stale data areas
	 * in the mailbox post.  Then copy the caller's mailbox contents to
	 * the bmbx mailbox region.
	 */
	mbx_cmnd = bf_get(lpfc_mqe_command, mb);
	memset(phba->sli4_hba.bmbx.avirt, 0, sizeof(struct lpfc_bmbx_create));
	lpfc_sli_pcimem_bcopy(mb, phba->sli4_hba.bmbx.avirt,
			      sizeof(struct lpfc_mqe));

	/* Post the high mailbox dma address to the port and wait for ready. */
	dma_address = &phba->sli4_hba.bmbx.dma_address;
	writel(dma_address->addr_hi, phba->sli4_hba.BMBXregaddr);

	/* wait for bootstrap mbox register for hi-address write done */
	rc = lpfc_sli4_wait_bmbx_ready(phba, mboxq);
	if (rc)
		goto exit;

	/* Post the low mailbox dma address to the port. */
	writel(dma_address->addr_lo, phba->sli4_hba.BMBXregaddr);

	/* wait for bootstrap mbox register for low address write done */
	rc = lpfc_sli4_wait_bmbx_ready(phba, mboxq);
	if (rc)
		goto exit;

	/*
	 * Read the CQ to ensure the mailbox has completed.
	 * If so, update the mailbox status so that the upper layers
	 * can complete the request normally.
	 */
	lpfc_sli_pcimem_bcopy(phba->sli4_hba.bmbx.avirt, mb,
			      sizeof(struct lpfc_mqe));
	mbox_rgn = (struct lpfc_bmbx_create *) phba->sli4_hba.bmbx.avirt;
	lpfc_sli_pcimem_bcopy(&mbox_rgn->mcqe, &mboxq->mcqe,
			      sizeof(struct lpfc_mcqe));
	mcqe_status = bf_get(lpfc_mcqe_status, &mbox_rgn->mcqe);
	/*
	 * When the CQE status indicates a failure and the mailbox status
	 * indicates success then copy the CQE status into the mailbox status
	 * (and prefix it with x4000).
	 */
	if (mcqe_status != MB_CQE_STATUS_SUCCESS) {
		if (bf_get(lpfc_mqe_status, mb) == MBX_SUCCESS)
			bf_set(lpfc_mqe_status, mb,
			       (LPFC_MBX_ERROR_RANGE | mcqe_status));
		rc = MBXERR_ERROR;
	} else
		lpfc_sli4_swap_str(phba, mboxq);

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0356 Mailbox cmd x%x (x%x/x%x) Status x%x "
			"Data: x%x x%x x%x x%x x%x x%x x%x x%x x%x x%x x%x"
			" x%x x%x CQ: x%x x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0, mbx_cmnd,
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			bf_get(lpfc_mqe_status, mb),
			mb->un.mb_words[0], mb->un.mb_words[1],
			mb->un.mb_words[2], mb->un.mb_words[3],
			mb->un.mb_words[4], mb->un.mb_words[5],
			mb->un.mb_words[6], mb->un.mb_words[7],
			mb->un.mb_words[8], mb->un.mb_words[9],
			mb->un.mb_words[10], mb->un.mb_words[11],
			mb->un.mb_words[12], mboxq->mcqe.word0,
			mboxq->mcqe.mcqe_tag0, 	mboxq->mcqe.mcqe_tag1,
			mboxq->mcqe.trailer);
exit:
	/* We are holding the token, no needed for lock when release */
	spin_lock_irqsave(&phba->hbalock, iflag);
	psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	phba->sli.mbox_active = NULL;
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return rc;
}

/**
 * lpfc_sli_issue_mbox_s4 - Issue an SLI4 mailbox command to firmware
 * @phba: Pointer to HBA context object.
 * @pmbox: Pointer to mailbox object.
 * @flag: Flag indicating how the mailbox need to be processed.
 *
 * This function is called by discovery code and HBA management code to submit
 * a mailbox command to firmware with SLI-4 interface spec.
 *
 * Return codes the caller owns the mailbox command after the return of the
 * function.
 **/
static int
lpfc_sli_issue_mbox_s4(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq,
		       uint32_t flag)
{
	struct lpfc_sli *psli = &phba->sli;
	unsigned long iflags;
	int rc;

	/* dump from issue mailbox command if setup */
	lpfc_idiag_mbxacc_dump_issue_mbox(phba, &mboxq->u.mb);

	rc = lpfc_mbox_dev_check(phba);
	if (unlikely(rc)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2544 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		goto out_not_finished;
	}

	/* Detect polling mode and jump to a handler */
	if (!phba->sli4_hba.intr_enable) {
		if (flag == MBX_POLL)
			rc = lpfc_sli4_post_sync_mbox(phba, mboxq);
		else
			rc = -EIO;
		if (rc != MBX_SUCCESS)
			lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
					"(%d):2541 Mailbox command x%x "
					"(x%x/x%x) failure: "
					"mqe_sta: x%x mcqe_sta: x%x/x%x "
					"Data: x%x x%x\n,",
					mboxq->vport ? mboxq->vport->vpi : 0,
					mboxq->u.mb.mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									mboxq),
					lpfc_sli_config_mbox_opcode_get(phba,
									mboxq),
					bf_get(lpfc_mqe_status, &mboxq->u.mqe),
					bf_get(lpfc_mcqe_status, &mboxq->mcqe),
					bf_get(lpfc_mcqe_ext_status,
					       &mboxq->mcqe),
					psli->sli_flag, flag);
		return rc;
	} else if (flag == MBX_POLL) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_MBOX | LOG_SLI,
				"(%d):2542 Try to issue mailbox command "
				"x%x (x%x/x%x) synchronously ahead of async"
				"mailbox command queue: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		/* Try to block the asynchronous mailbox posting */
		rc = lpfc_sli4_async_mbox_block(phba);
		if (!rc) {
			/* Successfully blocked, now issue sync mbox cmd */
			rc = lpfc_sli4_post_sync_mbox(phba, mboxq);
			if (rc != MBX_SUCCESS)
				lpfc_printf_log(phba, KERN_WARNING,
					LOG_MBOX | LOG_SLI,
					"(%d):2597 Sync Mailbox command "
					"x%x (x%x/x%x) failure: "
					"mqe_sta: x%x mcqe_sta: x%x/x%x "
					"Data: x%x x%x\n,",
					mboxq->vport ? mboxq->vport->vpi : 0,
					mboxq->u.mb.mbxCommand,
					lpfc_sli_config_mbox_subsys_get(phba,
									mboxq),
					lpfc_sli_config_mbox_opcode_get(phba,
									mboxq),
					bf_get(lpfc_mqe_status, &mboxq->u.mqe),
					bf_get(lpfc_mcqe_status, &mboxq->mcqe),
					bf_get(lpfc_mcqe_ext_status,
					       &mboxq->mcqe),
					psli->sli_flag, flag);
			/* Unblock the async mailbox posting afterward */
			lpfc_sli4_async_mbox_unblock(phba);
		}
		return rc;
	}

	/* Now, interrupt mode asynchrous mailbox command */
	rc = lpfc_mbox_cmd_check(phba, mboxq);
	if (rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2543 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, flag);
		goto out_not_finished;
	}

	/* Put the mailbox command to the driver internal FIFO */
	psli->slistat.mbox_busy++;
	spin_lock_irqsave(&phba->hbalock, iflags);
	lpfc_mbox_put(phba, mboxq);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0354 Mbox cmd issue - Enqueue Data: "
			"x%x (x%x/x%x) x%x x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0xffffff,
			bf_get(lpfc_mqe_command, &mboxq->u.mqe),
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			phba->pport->port_state,
			psli->sli_flag, MBX_NOWAIT);
	/* Wake up worker thread to transport mailbox command from head */
	lpfc_worker_wake_up(phba);

	return MBX_BUSY;

out_not_finished:
	return MBX_NOT_FINISHED;
}

/**
 * lpfc_sli4_post_async_mbox - Post an SLI4 mailbox command to device
 * @phba: Pointer to HBA context object.
 *
 * This function is called by worker thread to send a mailbox command to
 * SLI4 HBA firmware.
 *
 **/
int
lpfc_sli4_post_async_mbox(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *mboxq;
	int rc = MBX_SUCCESS;
	unsigned long iflags;
	struct lpfc_mqe *mqe;
	uint32_t mbx_cmnd;

	/* Check interrupt mode before post async mailbox command */
	if (unlikely(!phba->sli4_hba.intr_enable))
		return MBX_NOT_FINISHED;

	/* Check for mailbox command service token */
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (unlikely(psli->sli_flag & LPFC_SLI_ASYNC_MBX_BLK)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_NOT_FINISHED;
	}
	if (psli->sli_flag & LPFC_SLI_MBOX_ACTIVE) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_NOT_FINISHED;
	}
	if (unlikely(phba->sli.mbox_active)) {
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"0384 There is pending active mailbox cmd\n");
		return MBX_NOT_FINISHED;
	}
	/* Take the mailbox command service token */
	psli->sli_flag |= LPFC_SLI_MBOX_ACTIVE;

	/* Get the next mailbox command from head of queue */
	mboxq = lpfc_mbox_get(phba);

	/* If no more mailbox command waiting for post, we're done */
	if (!mboxq) {
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		return MBX_SUCCESS;
	}
	phba->sli.mbox_active = mboxq;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	/* Check device readiness for posting mailbox command */
	rc = lpfc_mbox_dev_check(phba);
	if (unlikely(rc))
		/* Driver clean routine will clean up pending mailbox */
		goto out_not_finished;

	/* Prepare the mbox command to be posted */
	mqe = &mboxq->u.mqe;
	mbx_cmnd = bf_get(lpfc_mqe_command, mqe);

	/* Start timer for the mbox_tmo and log some mailbox post messages */
	mod_timer(&psli->mbox_tmo, (jiffies +
		  msecs_to_jiffies(1000 * lpfc_mbox_tmo_val(phba, mboxq))));

	lpfc_printf_log(phba, KERN_INFO, LOG_MBOX | LOG_SLI,
			"(%d):0355 Mailbox cmd x%x (x%x/x%x) issue Data: "
			"x%x x%x\n",
			mboxq->vport ? mboxq->vport->vpi : 0, mbx_cmnd,
			lpfc_sli_config_mbox_subsys_get(phba, mboxq),
			lpfc_sli_config_mbox_opcode_get(phba, mboxq),
			phba->pport->port_state, psli->sli_flag);

	if (mbx_cmnd != MBX_HEARTBEAT) {
		if (mboxq->vport) {
			lpfc_debugfs_disc_trc(mboxq->vport,
				LPFC_DISC_TRC_MBOX_VPORT,
				"MBOX Send vport: cmd:x%x mb:x%x x%x",
				mbx_cmnd, mqe->un.mb_words[0],
				mqe->un.mb_words[1]);
		} else {
			lpfc_debugfs_disc_trc(phba->pport,
				LPFC_DISC_TRC_MBOX,
				"MBOX Send: cmd:x%x mb:x%x x%x",
				mbx_cmnd, mqe->un.mb_words[0],
				mqe->un.mb_words[1]);
		}
	}
	psli->slistat.mbox_cmd++;

	/* Post the mailbox command to the port */
	rc = lpfc_sli4_mq_put(phba->sli4_hba.mbx_wq, mqe);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX | LOG_SLI,
				"(%d):2533 Mailbox command x%x (x%x/x%x) "
				"cannot issue Data: x%x x%x\n",
				mboxq->vport ? mboxq->vport->vpi : 0,
				mboxq->u.mb.mbxCommand,
				lpfc_sli_config_mbox_subsys_get(phba, mboxq),
				lpfc_sli_config_mbox_opcode_get(phba, mboxq),
				psli->sli_flag, MBX_NOWAIT);
		goto out_not_finished;
	}

	return rc;

out_not_finished:
	spin_lock_irqsave(&phba->hbalock, iflags);
	if (phba->sli.mbox_active) {
		mboxq->u.mb.mbxStatus = MBX_NOT_FINISHED;
		__lpfc_mbox_cmpl_put(phba, mboxq);
		/* Release the token */
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
		phba->sli.mbox_active = NULL;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return MBX_NOT_FINISHED;
}

/**
 * lpfc_sli_issue_mbox - Wrapper func for issuing mailbox command
 * @phba: Pointer to HBA context object.
 * @pmbox: Pointer to mailbox object.
 * @flag: Flag indicating how the mailbox need to be processed.
 *
 * This routine wraps the actual SLI3 or SLI4 mailbox issuing routine from
 * the API jump table function pointer from the lpfc_hba struct.
 *
 * Return codes the caller owns the mailbox command after the return of the
 * function.
 **/
int
lpfc_sli_issue_mbox(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmbox, uint32_t flag)
{
	return phba->lpfc_sli_issue_mbox(phba, pmbox, flag);
}

/**
 * lpfc_mbox_api_table_setup - Set up mbox api function jump table
 * @phba: The hba struct for which this call is being executed.
 * @dev_grp: The HBA PCI-Device group number.
 *
 * This routine sets up the mbox interface API function jump table in @phba
 * struct.
 * Returns: 0 - success, -ENODEV - failure.
 **/
int
lpfc_mbox_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->lpfc_sli_issue_mbox = lpfc_sli_issue_mbox_s3;
		phba->lpfc_sli_handle_slow_ring_event =
				lpfc_sli_handle_slow_ring_event_s3;
		phba->lpfc_sli_hbq_to_firmware = lpfc_sli_hbq_to_firmware_s3;
		phba->lpfc_sli_brdrestart = lpfc_sli_brdrestart_s3;
		phba->lpfc_sli_brdready = lpfc_sli_brdready_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->lpfc_sli_issue_mbox = lpfc_sli_issue_mbox_s4;
		phba->lpfc_sli_handle_slow_ring_event =
				lpfc_sli_handle_slow_ring_event_s4;
		phba->lpfc_sli_hbq_to_firmware = lpfc_sli_hbq_to_firmware_s4;
		phba->lpfc_sli_brdrestart = lpfc_sli_brdrestart_s4;
		phba->lpfc_sli_brdready = lpfc_sli_brdready_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1420 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	return 0;
}

/**
 * __lpfc_sli_ringtx_put - Add an iocb to the txq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @piocb: Pointer to address of newly added command iocb.
 *
 * This function is called with hbalock held to add a command
 * iocb to the txq when SLI layer cannot submit the command iocb
 * to the ring.
 **/
void
__lpfc_sli_ringtx_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		    struct lpfc_iocbq *piocb)
{
	/* Insert the caller's iocb in the txq tail for later processing. */
	list_add_tail(&piocb->list, &pring->txq);
}

/**
 * lpfc_sli_next_iocb - Get the next iocb in the txq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @piocb: Pointer to address of newly added command iocb.
 *
 * This function is called with hbalock held before a new
 * iocb is submitted to the firmware. This function checks
 * txq to flush the iocbs in txq to Firmware before
 * submitting new iocbs to the Firmware.
 * If there are iocbs in the txq which need to be submitted
 * to firmware, lpfc_sli_next_iocb returns the first element
 * of the txq after dequeuing it from txq.
 * If there is no iocb in the txq then the function will return
 * *piocb and *piocb is set to NULL. Caller needs to check
 * *piocb to find if there are more commands in the txq.
 **/
static struct lpfc_iocbq *
lpfc_sli_next_iocb(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		   struct lpfc_iocbq **piocb)
{
	struct lpfc_iocbq * nextiocb;

	nextiocb = lpfc_sli_ringtx_get(phba, pring);
	if (!nextiocb) {
		nextiocb = *piocb;
		*piocb = NULL;
	}

	return nextiocb;
}

/**
 * __lpfc_sli_issue_iocb_s3 - SLI3 device lockless ver of lpfc_sli_issue_iocb
 * @phba: Pointer to HBA context object.
 * @ring_number: SLI ring number to issue iocb on.
 * @piocb: Pointer to command iocb.
 * @flag: Flag indicating if this command can be put into txq.
 *
 * __lpfc_sli_issue_iocb_s3 is used by other functions in the driver to issue
 * an iocb command to an HBA with SLI-3 interface spec. If the PCI slot is
 * recovering from error state, if HBA is resetting or if LPFC_STOP_IOCB_EVENT
 * flag is turned on, the function returns IOCB_ERROR. When the link is down,
 * this function allows only iocbs for posting buffers. This function finds
 * next available slot in the command ring and posts the command to the
 * available slot and writes the port attention register to request HBA start
 * processing new iocb. If there is no slot available in the ring and
 * flag & SLI_IOCB_RET_IOCB is set, the new iocb is added to the txq, otherwise
 * the function returns IOCB_BUSY.
 *
 * This function is called with hbalock held. The function will return success
 * after it successfully submit the iocb to firmware or after adding to the
 * txq.
 **/
static int
__lpfc_sli_issue_iocb_s3(struct lpfc_hba *phba, uint32_t ring_number,
		    struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_iocbq *nextiocb;
	IOCB_t *iocb;
	struct lpfc_sli_ring *pring = &phba->sli.ring[ring_number];

	if (piocb->iocb_cmpl && (!piocb->vport) &&
	   (piocb->iocb.ulpCommand != CMD_ABORT_XRI_CN) &&
	   (piocb->iocb.ulpCommand != CMD_CLOSE_XRI_CN)) {
		lpfc_printf_log(phba, KERN_ERR,
				LOG_SLI | LOG_VPORT,
				"1807 IOCB x%x failed. No vport\n",
				piocb->iocb.ulpCommand);
		dump_stack();
		return IOCB_ERROR;
	}


	/* If the PCI channel is in offline state, do not post iocbs. */
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return IOCB_ERROR;

	/* If HBA has a deferred error attention, fail the iocb. */
	if (unlikely(phba->hba_flag & DEFER_ERATT))
		return IOCB_ERROR;

	/*
	 * We should never get an IOCB if we are in a < LINK_DOWN state
	 */
	if (unlikely(phba->link_state < LPFC_LINK_DOWN))
		return IOCB_ERROR;

	/*
	 * Check to see if we are blocking IOCB processing because of a
	 * outstanding event.
	 */
	if (unlikely(pring->flag & LPFC_STOP_IOCB_EVENT))
		goto iocb_busy;

	if (unlikely(phba->link_state == LPFC_LINK_DOWN)) {
		/*
		 * Only CREATE_XRI, CLOSE_XRI, and QUE_RING_BUF
		 * can be issued if the link is not up.
		 */
		switch (piocb->iocb.ulpCommand) {
		case CMD_GEN_REQUEST64_CR:
		case CMD_GEN_REQUEST64_CX:
			if (!(phba->sli.sli_flag & LPFC_MENLO_MAINT) ||
				(piocb->iocb.un.genreq64.w5.hcsw.Rctl !=
					FC_RCTL_DD_UNSOL_CMD) ||
				(piocb->iocb.un.genreq64.w5.hcsw.Type !=
					MENLO_TRANSPORT_TYPE))

				goto iocb_busy;
			break;
		case CMD_QUE_RING_BUF_CN:
		case CMD_QUE_RING_BUF64_CN:
			/*
			 * For IOCBs, like QUE_RING_BUF, that have no rsp ring
			 * completion, iocb_cmpl MUST be 0.
			 */
			if (piocb->iocb_cmpl)
				piocb->iocb_cmpl = NULL;
			/*FALLTHROUGH*/
		case CMD_CREATE_XRI_CR:
		case CMD_CLOSE_XRI_CN:
		case CMD_CLOSE_XRI_CX:
			break;
		default:
			goto iocb_busy;
		}

	/*
	 * For FCP commands, we must be in a state where we can process link
	 * attention events.
	 */
	} else if (unlikely(pring->ringno == phba->sli.fcp_ring &&
			    !(phba->sli.sli_flag & LPFC_PROCESS_LA))) {
		goto iocb_busy;
	}

	while ((iocb = lpfc_sli_next_iocb_slot(phba, pring)) &&
	       (nextiocb = lpfc_sli_next_iocb(phba, pring, &piocb)))
		lpfc_sli_submit_iocb(phba, pring, iocb, nextiocb);

	if (iocb)
		lpfc_sli_update_ring(phba, pring);
	else
		lpfc_sli_update_full_ring(phba, pring);

	if (!piocb)
		return IOCB_SUCCESS;

	goto out_busy;

 iocb_busy:
	pring->stats.iocb_cmd_delay++;

 out_busy:

	if (!(flag & SLI_IOCB_RET_IOCB)) {
		__lpfc_sli_ringtx_put(phba, pring, piocb);
		return IOCB_SUCCESS;
	}

	return IOCB_BUSY;
}

/**
 * lpfc_sli4_bpl2sgl - Convert the bpl/bde to a sgl.
 * @phba: Pointer to HBA context object.
 * @piocb: Pointer to command iocb.
 * @sglq: Pointer to the scatter gather queue object.
 *
 * This routine converts the bpl or bde that is in the IOCB
 * to a sgl list for the sli4 hardware. The physical address
 * of the bpl/bde is converted back to a virtual address.
 * If the IOCB contains a BPL then the list of BDE's is
 * converted to sli4_sge's. If the IOCB contains a single
 * BDE then it is converted to a single sli_sge.
 * The IOCB is still in cpu endianess so the contents of
 * the bpl can be used without byte swapping.
 *
 * Returns valid XRI = Success, NO_XRI = Failure.
**/
static uint16_t
lpfc_sli4_bpl2sgl(struct lpfc_hba *phba, struct lpfc_iocbq *piocbq,
		struct lpfc_sglq *sglq)
{
	uint16_t xritag = NO_XRI;
	struct ulp_bde64 *bpl = NULL;
	struct ulp_bde64 bde;
	struct sli4_sge *sgl  = NULL;
	struct lpfc_dmabuf *dmabuf;
	IOCB_t *icmd;
	int numBdes = 0;
	int i = 0;
	uint32_t offset = 0; /* accumulated offset in the sg request list */
	int inbound = 0; /* number of sg reply entries inbound from firmware */

	if (!piocbq || !sglq)
		return xritag;

	sgl  = (struct sli4_sge *)sglq->sgl;
	icmd = &piocbq->iocb;
	if (icmd->ulpCommand == CMD_XMIT_BLS_RSP64_CX)
		return sglq->sli4_xritag;
	if (icmd->un.genreq64.bdl.bdeFlags == BUFF_TYPE_BLP_64) {
		numBdes = icmd->un.genreq64.bdl.bdeSize /
				sizeof(struct ulp_bde64);
		/* The addrHigh and addrLow fields within the IOCB
		 * have not been byteswapped yet so there is no
		 * need to swap them back.
		 */
		if (piocbq->context3)
			dmabuf = (struct lpfc_dmabuf *)piocbq->context3;
		else
			return xritag;

		bpl  = (struct ulp_bde64 *)dmabuf->virt;
		if (!bpl)
			return xritag;

		for (i = 0; i < numBdes; i++) {
			/* Should already be byte swapped. */
			sgl->addr_hi = bpl->addrHigh;
			sgl->addr_lo = bpl->addrLow;

			sgl->word2 = le32_to_cpu(sgl->word2);
			if ((i+1) == numBdes)
				bf_set(lpfc_sli4_sge_last, sgl, 1);
			else
				bf_set(lpfc_sli4_sge_last, sgl, 0);
			/* swap the size field back to the cpu so we
			 * can assign it to the sgl.
			 */
			bde.tus.w = le32_to_cpu(bpl->tus.w);
			sgl->sge_len = cpu_to_le32(bde.tus.f.bdeSize);
			/* The offsets in the sgl need to be accumulated
			 * separately for the request and reply lists.
			 * The request is always first, the reply follows.
			 */
			if (piocbq->iocb.ulpCommand == CMD_GEN_REQUEST64_CR) {
				/* add up the reply sg entries */
				if (bpl->tus.f.bdeFlags == BUFF_TYPE_BDE_64I)
					inbound++;
				/* first inbound? reset the offset */
				if (inbound == 1)
					offset = 0;
				bf_set(lpfc_sli4_sge_offset, sgl, offset);
				bf_set(lpfc_sli4_sge_type, sgl,
					LPFC_SGE_TYPE_DATA);
				offset += bde.tus.f.bdeSize;
			}
			sgl->word2 = cpu_to_le32(sgl->word2);
			bpl++;
			sgl++;
		}
	} else if (icmd->un.genreq64.bdl.bdeFlags == BUFF_TYPE_BDE_64) {
			/* The addrHigh and addrLow fields of the BDE have not
			 * been byteswapped yet so they need to be swapped
			 * before putting them in the sgl.
			 */
			sgl->addr_hi =
				cpu_to_le32(icmd->un.genreq64.bdl.addrHigh);
			sgl->addr_lo =
				cpu_to_le32(icmd->un.genreq64.bdl.addrLow);
			sgl->word2 = le32_to_cpu(sgl->word2);
			bf_set(lpfc_sli4_sge_last, sgl, 1);
			sgl->word2 = cpu_to_le32(sgl->word2);
			sgl->sge_len =
				cpu_to_le32(icmd->un.genreq64.bdl.bdeSize);
	}
	return sglq->sli4_xritag;
}

/**
 * lpfc_sli4_scmd_to_wqidx_distr - scsi command to SLI4 WQ index distribution
 * @phba: Pointer to HBA context object.
 *
 * This routine performs a roundrobin SCSI command to SLI4 FCP WQ index
 * distribution.  This is called by __lpfc_sli_issue_iocb_s4() with the hbalock
 * held.
 *
 * Return: index into SLI4 fast-path FCP queue index.
 **/
static inline uint32_t
lpfc_sli4_scmd_to_wqidx_distr(struct lpfc_hba *phba)
{
	struct lpfc_vector_map_info *cpup;
	int chann, cpu;

	if (phba->cfg_fcp_io_sched == LPFC_FCP_SCHED_BY_CPU) {
		cpu = smp_processor_id();
		if (cpu < phba->sli4_hba.num_present_cpu) {
			cpup = phba->sli4_hba.cpu_map;
			cpup += cpu;
			return cpup->channel_id;
		}
		chann = cpu;
	}
	chann = atomic_add_return(1, &phba->fcp_qidx);
	chann = (chann % phba->cfg_fcp_io_channel);
	return chann;
}

/**
 * lpfc_sli_iocb2wqe - Convert the IOCB to a work queue entry.
 * @phba: Pointer to HBA context object.
 * @piocb: Pointer to command iocb.
 * @wqe: Pointer to the work queue entry.
 *
 * This routine converts the iocb command to its Work Queue Entry
 * equivalent. The wqe pointer should not have any fields set when
 * this routine is called because it will memcpy over them.
 * This routine does not set the CQ_ID or the WQEC bits in the
 * wqe.
 *
 * Returns: 0 = Success, IOCB_ERROR = Failure.
 **/
static int
lpfc_sli4_iocb2wqe(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq,
		union lpfc_wqe *wqe)
{
	uint32_t xmit_len = 0, total_len = 0;
	uint8_t ct = 0;
	uint32_t fip;
	uint32_t abort_tag;
	uint8_t command_type = ELS_COMMAND_NON_FIP;
	uint8_t cmnd;
	uint16_t xritag;
	uint16_t abrt_iotag;
	struct lpfc_iocbq *abrtiocbq;
	struct ulp_bde64 *bpl = NULL;
	uint32_t els_id = LPFC_ELS_ID_DEFAULT;
	int numBdes, i;
	struct ulp_bde64 bde;
	struct lpfc_nodelist *ndlp;
	uint32_t *pcmd;
	uint32_t if_type;

	fip = phba->hba_flag & HBA_FIP_SUPPORT;
	/* The fcp commands will set command type */
	if (iocbq->iocb_flag &  LPFC_IO_FCP)
		command_type = FCP_COMMAND;
	else if (fip && (iocbq->iocb_flag & LPFC_FIP_ELS_ID_MASK))
		command_type = ELS_COMMAND_FIP;
	else
		command_type = ELS_COMMAND_NON_FIP;

	/* Some of the fields are in the right position already */
	memcpy(wqe, &iocbq->iocb, sizeof(union lpfc_wqe));
	abort_tag = (uint32_t) iocbq->iotag;
	xritag = iocbq->sli4_xritag;
	wqe->generic.wqe_com.word7 = 0; /* The ct field has moved so reset */
	/* words0-2 bpl convert bde */
	if (iocbq->iocb.un.genreq64.bdl.bdeFlags == BUFF_TYPE_BLP_64) {
		numBdes = iocbq->iocb.un.genreq64.bdl.bdeSize /
				sizeof(struct ulp_bde64);
		bpl  = (struct ulp_bde64 *)
			((struct lpfc_dmabuf *)iocbq->context3)->virt;
		if (!bpl)
			return IOCB_ERROR;

		/* Should already be byte swapped. */
		wqe->generic.bde.addrHigh =  le32_to_cpu(bpl->addrHigh);
		wqe->generic.bde.addrLow =  le32_to_cpu(bpl->addrLow);
		/* swap the size field back to the cpu so we
		 * can assign it to the sgl.
		 */
		wqe->generic.bde.tus.w  = le32_to_cpu(bpl->tus.w);
		xmit_len = wqe->generic.bde.tus.f.bdeSize;
		total_len = 0;
		for (i = 0; i < numBdes; i++) {
			bde.tus.w  = le32_to_cpu(bpl[i].tus.w);
			total_len += bde.tus.f.bdeSize;
		}
	} else
		xmit_len = iocbq->iocb.un.fcpi64.bdl.bdeSize;

	iocbq->iocb.ulpIoTag = iocbq->iotag;
	cmnd = iocbq->iocb.ulpCommand;

	switch (iocbq->iocb.ulpCommand) {
	case CMD_ELS_REQUEST64_CR:
		if (iocbq->iocb_flag & LPFC_IO_LIBDFC)
			ndlp = iocbq->context_un.ndlp;
		else
			ndlp = (struct lpfc_nodelist *)iocbq->context1;
		if (!iocbq->iocb.ulpLe) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2007 Only Limited Edition cmd Format"
				" supported 0x%x\n",
				iocbq->iocb.ulpCommand);
			return IOCB_ERROR;
		}

		wqe->els_req.payload_len = xmit_len;
		/* Els_reguest64 has a TMO */
		bf_set(wqe_tmo, &wqe->els_req.wqe_com,
			iocbq->iocb.ulpTimeout);
		/* Need a VF for word 4 set the vf bit*/
		bf_set(els_req64_vf, &wqe->els_req, 0);
		/* And a VFID for word 12 */
		bf_set(els_req64_vfid, &wqe->els_req, 0);
		ct = ((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l);
		bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
		       iocbq->iocb.ulpContext);
		bf_set(wqe_ct, &wqe->els_req.wqe_com, ct);
		bf_set(wqe_pu, &wqe->els_req.wqe_com, 0);
		/* CCP CCPE PV PRI in word10 were set in the memcpy */
		if (command_type == ELS_COMMAND_FIP)
			els_id = ((iocbq->iocb_flag & LPFC_FIP_ELS_ID_MASK)
					>> LPFC_FIP_ELS_ID_SHIFT);
		pcmd = (uint32_t *) (((struct lpfc_dmabuf *)
					iocbq->context2)->virt);
		if_type = bf_get(lpfc_sli_intf_if_type,
					&phba->sli4_hba.sli_intf);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
			if (pcmd && (*pcmd == ELS_CMD_FLOGI ||
				*pcmd == ELS_CMD_SCR ||
				*pcmd == ELS_CMD_FDISC ||
				*pcmd == ELS_CMD_LOGO ||
				*pcmd == ELS_CMD_PLOGI)) {
				bf_set(els_req64_sp, &wqe->els_req, 1);
				bf_set(els_req64_sid, &wqe->els_req,
					iocbq->vport->fc_myDID);
				if ((*pcmd == ELS_CMD_FLOGI) &&
					!(phba->fc_topology ==
						LPFC_TOPOLOGY_LOOP))
					bf_set(els_req64_sid, &wqe->els_req, 0);
				bf_set(wqe_ct, &wqe->els_req.wqe_com, 1);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
					phba->vpi_ids[iocbq->vport->vpi]);
			} else if (pcmd && iocbq->context1) {
				bf_set(wqe_ct, &wqe->els_req.wqe_com, 0);
				bf_set(wqe_ctxt_tag, &wqe->els_req.wqe_com,
					phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
			}
		}
		bf_set(wqe_temp_rpi, &wqe->els_req.wqe_com,
		       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
		bf_set(wqe_els_id, &wqe->els_req.wqe_com, els_id);
		bf_set(wqe_dbde, &wqe->els_req.wqe_com, 1);
		bf_set(wqe_iod, &wqe->els_req.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_qosd, &wqe->els_req.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->els_req.wqe_com, LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->els_req.wqe_com, 0);
		break;
	case CMD_XMIT_SEQUENCE64_CX:
		bf_set(wqe_ctxt_tag, &wqe->xmit_sequence.wqe_com,
		       iocbq->iocb.un.ulpWord[3]);
		bf_set(wqe_rcvoxid, &wqe->xmit_sequence.wqe_com,
		       iocbq->iocb.unsli3.rcvsli3.ox_id);
		/* The entire sequence is transmitted for this IOCB */
		xmit_len = total_len;
		cmnd = CMD_XMIT_SEQUENCE64_CR;
		if (phba->link_flag & LS_LOOPBACK_MODE)
			bf_set(wqe_xo, &wqe->xmit_sequence.wge_ctl, 1);
	case CMD_XMIT_SEQUENCE64_CR:
		/* word3 iocb=io_tag32 wqe=reserved */
		wqe->xmit_sequence.rsvd3 = 0;
		/* word4 relative_offset memcpy */
		/* word5 r_ctl/df_ctl memcpy */
		bf_set(wqe_pu, &wqe->xmit_sequence.wqe_com, 0);
		bf_set(wqe_dbde, &wqe->xmit_sequence.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_sequence.wqe_com,
		       LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->xmit_sequence.wqe_com,
		       LPFC_WQE_LENLOC_WORD12);
		bf_set(wqe_ebde_cnt, &wqe->xmit_sequence.wqe_com, 0);
		wqe->xmit_sequence.xmit_len = xmit_len;
		command_type = OTHER_COMMAND;
		break;
	case CMD_XMIT_BCAST64_CN:
		/* word3 iocb=iotag32 wqe=seq_payload_len */
		wqe->xmit_bcast64.seq_payload_len = xmit_len;
		/* word4 iocb=rsvd wqe=rsvd */
		/* word5 iocb=rctl/type/df_ctl wqe=rctl/type/df_ctl memcpy */
		/* word6 iocb=ctxt_tag/io_tag wqe=ctxt_tag/xri */
		bf_set(wqe_ct, &wqe->xmit_bcast64.wqe_com,
			((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l));
		bf_set(wqe_dbde, &wqe->xmit_bcast64.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_bcast64.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->xmit_bcast64.wqe_com,
		       LPFC_WQE_LENLOC_WORD3);
		bf_set(wqe_ebde_cnt, &wqe->xmit_bcast64.wqe_com, 0);
		break;
	case CMD_FCP_IWRITE64_CR:
		command_type = FCP_COMMAND_DATA_OUT;
		/* word3 iocb=iotag wqe=payload_offset_len */
		/* Add the FCP_CMD and FCP_RSP sizes to get the offset */
		wqe->fcp_iwrite.payload_offset_len =
			xmit_len + sizeof(struct fcp_rsp);
		/* word4 iocb=parameter wqe=total_xfer_length memcpy */
		/* word5 iocb=initial_xfer_len wqe=initial_xfer_len memcpy */
		bf_set(wqe_erp, &wqe->fcp_iwrite.wqe_com,
		       iocbq->iocb.ulpFCP2Rcvy);
		bf_set(wqe_lnk, &wqe->fcp_iwrite.wqe_com, iocbq->iocb.ulpXS);
		/* Always open the exchange */
		bf_set(wqe_xc, &wqe->fcp_iwrite.wqe_com, 0);
		bf_set(wqe_iod, &wqe->fcp_iwrite.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_lenloc, &wqe->fcp_iwrite.wqe_com,
		       LPFC_WQE_LENLOC_WORD4);
		bf_set(wqe_ebde_cnt, &wqe->fcp_iwrite.wqe_com, 0);
		bf_set(wqe_pu, &wqe->fcp_iwrite.wqe_com, iocbq->iocb.ulpPU);
		bf_set(wqe_dbde, &wqe->fcp_iwrite.wqe_com, 1);
		break;
	case CMD_FCP_IREAD64_CR:
		/* word3 iocb=iotag wqe=payload_offset_len */
		/* Add the FCP_CMD and FCP_RSP sizes to get the offset */
		wqe->fcp_iread.payload_offset_len =
			xmit_len + sizeof(struct fcp_rsp);
		/* word4 iocb=parameter wqe=total_xfer_length memcpy */
		/* word5 iocb=initial_xfer_len wqe=initial_xfer_len memcpy */
		bf_set(wqe_erp, &wqe->fcp_iread.wqe_com,
		       iocbq->iocb.ulpFCP2Rcvy);
		bf_set(wqe_lnk, &wqe->fcp_iread.wqe_com, iocbq->iocb.ulpXS);
		/* Always open the exchange */
		bf_set(wqe_xc, &wqe->fcp_iread.wqe_com, 0);
		bf_set(wqe_iod, &wqe->fcp_iread.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_lenloc, &wqe->fcp_iread.wqe_com,
		       LPFC_WQE_LENLOC_WORD4);
		bf_set(wqe_ebde_cnt, &wqe->fcp_iread.wqe_com, 0);
		bf_set(wqe_pu, &wqe->fcp_iread.wqe_com, iocbq->iocb.ulpPU);
		bf_set(wqe_dbde, &wqe->fcp_iread.wqe_com, 1);
		break;
	case CMD_FCP_ICMND64_CR:
		/* word3 iocb=IO_TAG wqe=reserved */
		wqe->fcp_icmd.rsrvd3 = 0;
		bf_set(wqe_pu, &wqe->fcp_icmd.wqe_com, 0);
		/* Always open the exchange */
		bf_set(wqe_xc, &wqe->fcp_icmd.wqe_com, 0);
		bf_set(wqe_dbde, &wqe->fcp_icmd.wqe_com, 1);
		bf_set(wqe_iod, &wqe->fcp_icmd.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_qosd, &wqe->fcp_icmd.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->fcp_icmd.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->fcp_icmd.wqe_com, 0);
		bf_set(wqe_erp, &wqe->fcp_icmd.wqe_com,
		       iocbq->iocb.ulpFCP2Rcvy);
		break;
	case CMD_GEN_REQUEST64_CR:
		/* For this command calculate the xmit length of the
		 * request bde.
		 */
		xmit_len = 0;
		numBdes = iocbq->iocb.un.genreq64.bdl.bdeSize /
			sizeof(struct ulp_bde64);
		for (i = 0; i < numBdes; i++) {
			bde.tus.w = le32_to_cpu(bpl[i].tus.w);
			if (bde.tus.f.bdeFlags != BUFF_TYPE_BDE_64)
				break;
			xmit_len += bde.tus.f.bdeSize;
		}
		/* word3 iocb=IO_TAG wqe=request_payload_len */
		wqe->gen_req.request_payload_len = xmit_len;
		/* word4 iocb=parameter wqe=relative_offset memcpy */
		/* word5 [rctl, type, df_ctl, la] copied in memcpy */
		/* word6 context tag copied in memcpy */
		if (iocbq->iocb.ulpCt_h  || iocbq->iocb.ulpCt_l) {
			ct = ((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l);
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2015 Invalid CT %x command 0x%x\n",
				ct, iocbq->iocb.ulpCommand);
			return IOCB_ERROR;
		}
		bf_set(wqe_ct, &wqe->gen_req.wqe_com, 0);
		bf_set(wqe_tmo, &wqe->gen_req.wqe_com, iocbq->iocb.ulpTimeout);
		bf_set(wqe_pu, &wqe->gen_req.wqe_com, iocbq->iocb.ulpPU);
		bf_set(wqe_dbde, &wqe->gen_req.wqe_com, 1);
		bf_set(wqe_iod, &wqe->gen_req.wqe_com, LPFC_WQE_IOD_READ);
		bf_set(wqe_qosd, &wqe->gen_req.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->gen_req.wqe_com, LPFC_WQE_LENLOC_NONE);
		bf_set(wqe_ebde_cnt, &wqe->gen_req.wqe_com, 0);
		command_type = OTHER_COMMAND;
		break;
	case CMD_XMIT_ELS_RSP64_CX:
		ndlp = (struct lpfc_nodelist *)iocbq->context1;
		/* words0-2 BDE memcpy */
		/* word3 iocb=iotag32 wqe=response_payload_len */
		wqe->xmit_els_rsp.response_payload_len = xmit_len;
		/* word4 */
		wqe->xmit_els_rsp.word4 = 0;
		/* word5 iocb=rsvd wge=did */
		bf_set(wqe_els_did, &wqe->xmit_els_rsp.wqe_dest,
			 iocbq->iocb.un.xseq64.xmit_els_remoteID);

		if_type = bf_get(lpfc_sli_intf_if_type,
					&phba->sli4_hba.sli_intf);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_2) {
			if (iocbq->vport->fc_flag & FC_PT2PT) {
				bf_set(els_rsp64_sp, &wqe->xmit_els_rsp, 1);
				bf_set(els_rsp64_sid, &wqe->xmit_els_rsp,
					iocbq->vport->fc_myDID);
				if (iocbq->vport->fc_myDID == Fabric_DID) {
					bf_set(wqe_els_did,
						&wqe->xmit_els_rsp.wqe_dest, 0);
				}
			}
		}
		bf_set(wqe_ct, &wqe->xmit_els_rsp.wqe_com,
		       ((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l));
		bf_set(wqe_pu, &wqe->xmit_els_rsp.wqe_com, iocbq->iocb.ulpPU);
		bf_set(wqe_rcvoxid, &wqe->xmit_els_rsp.wqe_com,
		       iocbq->iocb.unsli3.rcvsli3.ox_id);
		if (!iocbq->iocb.ulpCt_h && iocbq->iocb.ulpCt_l)
			bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
			       phba->vpi_ids[iocbq->vport->vpi]);
		bf_set(wqe_dbde, &wqe->xmit_els_rsp.wqe_com, 1);
		bf_set(wqe_iod, &wqe->xmit_els_rsp.wqe_com, LPFC_WQE_IOD_WRITE);
		bf_set(wqe_qosd, &wqe->xmit_els_rsp.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->xmit_els_rsp.wqe_com,
		       LPFC_WQE_LENLOC_WORD3);
		bf_set(wqe_ebde_cnt, &wqe->xmit_els_rsp.wqe_com, 0);
		bf_set(wqe_rsp_temp_rpi, &wqe->xmit_els_rsp,
		       phba->sli4_hba.rpi_ids[ndlp->nlp_rpi]);
		pcmd = (uint32_t *) (((struct lpfc_dmabuf *)
					iocbq->context2)->virt);
		if (phba->fc_topology == LPFC_TOPOLOGY_LOOP) {
				bf_set(els_rsp64_sp, &wqe->xmit_els_rsp, 1);
				bf_set(els_rsp64_sid, &wqe->xmit_els_rsp,
					iocbq->vport->fc_myDID);
				bf_set(wqe_ct, &wqe->xmit_els_rsp.wqe_com, 1);
				bf_set(wqe_ctxt_tag, &wqe->xmit_els_rsp.wqe_com,
					phba->vpi_ids[phba->pport->vpi]);
		}
		command_type = OTHER_COMMAND;
		break;
	case CMD_CLOSE_XRI_CN:
	case CMD_ABORT_XRI_CN:
	case CMD_ABORT_XRI_CX:
		/* words 0-2 memcpy should be 0 rserved */
		/* port will send abts */
		abrt_iotag = iocbq->iocb.un.acxri.abortContextTag;
		if (abrt_iotag != 0 && abrt_iotag <= phba->sli.last_iotag) {
			abrtiocbq = phba->sli.iocbq_lookup[abrt_iotag];
			fip = abrtiocbq->iocb_flag & LPFC_FIP_ELS_ID_MASK;
		} else
			fip = 0;

		if ((iocbq->iocb.ulpCommand == CMD_CLOSE_XRI_CN) || fip)
			/*
			 * The link is down, or the command was ELS_FIP
			 * so the fw does not need to send abts
			 * on the wire.
			 */
			bf_set(abort_cmd_ia, &wqe->abort_cmd, 1);
		else
			bf_set(abort_cmd_ia, &wqe->abort_cmd, 0);
		bf_set(abort_cmd_criteria, &wqe->abort_cmd, T_XRI_TAG);
		/* word5 iocb=CONTEXT_TAG|IO_TAG wqe=reserved */
		wqe->abort_cmd.rsrvd5 = 0;
		bf_set(wqe_ct, &wqe->abort_cmd.wqe_com,
			((iocbq->iocb.ulpCt_h << 1) | iocbq->iocb.ulpCt_l));
		abort_tag = iocbq->iocb.un.acxri.abortIoTag;
		/*
		 * The abort handler will send us CMD_ABORT_XRI_CN or
		 * CMD_CLOSE_XRI_CN and the fw only accepts CMD_ABORT_XRI_CX
		 */
		bf_set(wqe_cmnd, &wqe->abort_cmd.wqe_com, CMD_ABORT_XRI_CX);
		bf_set(wqe_qosd, &wqe->abort_cmd.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->abort_cmd.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		cmnd = CMD_ABORT_XRI_CX;
		command_type = OTHER_COMMAND;
		xritag = 0;
		break;
	case CMD_XMIT_BLS_RSP64_CX:
		ndlp = (struct lpfc_nodelist *)iocbq->context1;
		/* As BLS ABTS RSP WQE is very different from other WQEs,
		 * we re-construct this WQE here based on information in
		 * iocbq from scratch.
		 */
		memset(wqe, 0, sizeof(union lpfc_wqe));
		/* OX_ID is invariable to who sent ABTS to CT exchange */
		bf_set(xmit_bls_rsp64_oxid, &wqe->xmit_bls_rsp,
		       bf_get(lpfc_abts_oxid, &iocbq->iocb.un.bls_rsp));
		if (bf_get(lpfc_abts_orig, &iocbq->iocb.un.bls_rsp) ==
		    LPFC_ABTS_UNSOL_INT) {
			/* ABTS sent by initiator to CT exchange, the
			 * RX_ID field will be filled with the newly
			 * allocated responder XRI.
			 */
			bf_set(xmit_bls_rsp64_rxid, &wqe->xmit_bls_rsp,
			       iocbq->sli4_xritag);
		} else {
			/* ABTS sent by responder to CT exchange, the
			 * RX_ID field will be filled with the responder
			 * RX_ID from ABTS.
			 */
			bf_set(xmit_bls_rsp64_rxid, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_abts_rxid, &iocbq->iocb.un.bls_rsp));
		}
		bf_set(xmit_bls_rsp64_seqcnthi, &wqe->xmit_bls_rsp, 0xffff);
		bf_set(wqe_xmit_bls_pt, &wqe->xmit_bls_rsp.wqe_dest, 0x1);

		/* Use CT=VPI */
		bf_set(wqe_els_did, &wqe->xmit_bls_rsp.wqe_dest,
			ndlp->nlp_DID);
		bf_set(xmit_bls_rsp64_temprpi, &wqe->xmit_bls_rsp,
			iocbq->iocb.ulpContext);
		bf_set(wqe_ct, &wqe->xmit_bls_rsp.wqe_com, 1);
		bf_set(wqe_ctxt_tag, &wqe->xmit_bls_rsp.wqe_com,
			phba->vpi_ids[phba->pport->vpi]);
		bf_set(wqe_qosd, &wqe->xmit_bls_rsp.wqe_com, 1);
		bf_set(wqe_lenloc, &wqe->xmit_bls_rsp.wqe_com,
		       LPFC_WQE_LENLOC_NONE);
		/* Overwrite the pre-set comnd type with OTHER_COMMAND */
		command_type = OTHER_COMMAND;
		if (iocbq->iocb.un.xseq64.w5.hcsw.Rctl == FC_RCTL_BA_RJT) {
			bf_set(xmit_bls_rsp64_rjt_vspec, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_vndr_code, &iocbq->iocb.un.bls_rsp));
			bf_set(xmit_bls_rsp64_rjt_expc, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_rsn_expln, &iocbq->iocb.un.bls_rsp));
			bf_set(xmit_bls_rsp64_rjt_rsnc, &wqe->xmit_bls_rsp,
			       bf_get(lpfc_rsn_code, &iocbq->iocb.un.bls_rsp));
		}

		break;
	case CMD_XRI_ABORTED_CX:
	case CMD_CREATE_XRI_CR: /* Do we expect to use this? */
	case CMD_IOCB_FCP_IBIDIR64_CR: /* bidirectional xfer */
	case CMD_FCP_TSEND64_CX: /* Target mode send xfer-ready */
	case CMD_FCP_TRSP64_CX: /* Target mode rcv */
	case CMD_FCP_AUTO_TRSP_CX: /* Auto target rsp */
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2014 Invalid command 0x%x\n",
				iocbq->iocb.ulpCommand);
		return IOCB_ERROR;
		break;
	}

	if (iocbq->iocb_flag & LPFC_IO_DIF_PASS)
		bf_set(wqe_dif, &wqe->generic.wqe_com, LPFC_WQE_DIF_PASSTHRU);
	else if (iocbq->iocb_flag & LPFC_IO_DIF_STRIP)
		bf_set(wqe_dif, &wqe->generic.wqe_com, LPFC_WQE_DIF_STRIP);
	else if (iocbq->iocb_flag & LPFC_IO_DIF_INSERT)
		bf_set(wqe_dif, &wqe->generic.wqe_com, LPFC_WQE_DIF_INSERT);
	iocbq->iocb_flag &= ~(LPFC_IO_DIF_PASS | LPFC_IO_DIF_STRIP |
			      LPFC_IO_DIF_INSERT);
	bf_set(wqe_xri_tag, &wqe->generic.wqe_com, xritag);
	bf_set(wqe_reqtag, &wqe->generic.wqe_com, iocbq->iotag);
	wqe->generic.wqe_com.abort_tag = abort_tag;
	bf_set(wqe_cmd_type, &wqe->generic.wqe_com, command_type);
	bf_set(wqe_cmnd, &wqe->generic.wqe_com, cmnd);
	bf_set(wqe_class, &wqe->generic.wqe_com, iocbq->iocb.ulpClass);
	bf_set(wqe_cqid, &wqe->generic.wqe_com, LPFC_WQE_CQ_ID_DEFAULT);
	return 0;
}

/**
 * __lpfc_sli_issue_iocb_s4 - SLI4 device lockless ver of lpfc_sli_issue_iocb
 * @phba: Pointer to HBA context object.
 * @ring_number: SLI ring number to issue iocb on.
 * @piocb: Pointer to command iocb.
 * @flag: Flag indicating if this command can be put into txq.
 *
 * __lpfc_sli_issue_iocb_s4 is used by other functions in the driver to issue
 * an iocb command to an HBA with SLI-4 interface spec.
 *
 * This function is called with hbalock held. The function will return success
 * after it successfully submit the iocb to firmware or after adding to the
 * txq.
 **/
static int
__lpfc_sli_issue_iocb_s4(struct lpfc_hba *phba, uint32_t ring_number,
			 struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_sglq *sglq;
	union lpfc_wqe wqe;
	struct lpfc_sli_ring *pring = &phba->sli.ring[ring_number];

	if (piocb->sli4_xritag == NO_XRI) {
		if (piocb->iocb.ulpCommand == CMD_ABORT_XRI_CN ||
		    piocb->iocb.ulpCommand == CMD_CLOSE_XRI_CN)
			sglq = NULL;
		else {
			if (!list_empty(&pring->txq)) {
				if (!(flag & SLI_IOCB_RET_IOCB)) {
					__lpfc_sli_ringtx_put(phba,
						pring, piocb);
					return IOCB_SUCCESS;
				} else {
					return IOCB_BUSY;
				}
			} else {
				sglq = __lpfc_sli_get_sglq(phba, piocb);
				if (!sglq) {
					if (!(flag & SLI_IOCB_RET_IOCB)) {
						__lpfc_sli_ringtx_put(phba,
								pring,
								piocb);
						return IOCB_SUCCESS;
					} else
						return IOCB_BUSY;
				}
			}
		}
	} else if (piocb->iocb_flag &  LPFC_IO_FCP) {
		/* These IO's already have an XRI and a mapped sgl. */
		sglq = NULL;
	} else {
		/*
		 * This is a continuation of a commandi,(CX) so this
		 * sglq is on the active list
		 */
		sglq = __lpfc_get_active_sglq(phba, piocb->sli4_lxritag);
		if (!sglq)
			return IOCB_ERROR;
	}

	if (sglq) {
		piocb->sli4_lxritag = sglq->sli4_lxritag;
		piocb->sli4_xritag = sglq->sli4_xritag;
		if (NO_XRI == lpfc_sli4_bpl2sgl(phba, piocb, sglq))
			return IOCB_ERROR;
	}

	if (lpfc_sli4_iocb2wqe(phba, piocb, &wqe))
		return IOCB_ERROR;

	if ((piocb->iocb_flag & LPFC_IO_FCP) ||
		(piocb->iocb_flag & LPFC_USE_FCPWQIDX)) {
		if (unlikely(!phba->sli4_hba.fcp_wq))
			return IOCB_ERROR;
		if (lpfc_sli4_wq_put(phba->sli4_hba.fcp_wq[piocb->fcp_wqidx],
				     &wqe))
			return IOCB_ERROR;
	} else {
		if (unlikely(!phba->sli4_hba.els_wq))
			return IOCB_ERROR;
		if (lpfc_sli4_wq_put(phba->sli4_hba.els_wq, &wqe))
			return IOCB_ERROR;
	}
	lpfc_sli_ringtxcmpl_put(phba, pring, piocb);

	return 0;
}

/**
 * __lpfc_sli_issue_iocb - Wrapper func of lockless version for issuing iocb
 *
 * This routine wraps the actual lockless version for issusing IOCB function
 * pointer from the lpfc_hba struct.
 *
 * Return codes:
 * 	IOCB_ERROR - Error
 * 	IOCB_SUCCESS - Success
 * 	IOCB_BUSY - Busy
 **/
int
__lpfc_sli_issue_iocb(struct lpfc_hba *phba, uint32_t ring_number,
		struct lpfc_iocbq *piocb, uint32_t flag)
{
	return phba->__lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
}

/**
 * lpfc_sli_api_table_setup - Set up sli api function jump table
 * @phba: The hba struct for which this call is being executed.
 * @dev_grp: The HBA PCI-Device group number.
 *
 * This routine sets up the SLI interface API function jump table in @phba
 * struct.
 * Returns: 0 - success, -ENODEV - failure.
 **/
int
lpfc_sli_api_table_setup(struct lpfc_hba *phba, uint8_t dev_grp)
{

	switch (dev_grp) {
	case LPFC_PCI_DEV_LP:
		phba->__lpfc_sli_issue_iocb = __lpfc_sli_issue_iocb_s3;
		phba->__lpfc_sli_release_iocbq = __lpfc_sli_release_iocbq_s3;
		break;
	case LPFC_PCI_DEV_OC:
		phba->__lpfc_sli_issue_iocb = __lpfc_sli_issue_iocb_s4;
		phba->__lpfc_sli_release_iocbq = __lpfc_sli_release_iocbq_s4;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"1419 Invalid HBA PCI-device group: 0x%x\n",
				dev_grp);
		return -ENODEV;
		break;
	}
	phba->lpfc_get_iocb_from_iocbq = lpfc_get_iocb_from_iocbq;
	return 0;
}

/**
 * lpfc_sli_issue_iocb - Wrapper function for __lpfc_sli_issue_iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @piocb: Pointer to command iocb.
 * @flag: Flag indicating if this command can be put into txq.
 *
 * lpfc_sli_issue_iocb is a wrapper around __lpfc_sli_issue_iocb
 * function. This function gets the hbalock and calls
 * __lpfc_sli_issue_iocb function and will return the error returned
 * by __lpfc_sli_issue_iocb function. This wrapper is used by
 * functions which do not hold hbalock.
 **/
int
lpfc_sli_issue_iocb(struct lpfc_hba *phba, uint32_t ring_number,
		    struct lpfc_iocbq *piocb, uint32_t flag)
{
	struct lpfc_fcp_eq_hdl *fcp_eq_hdl;
	struct lpfc_sli_ring *pring;
	struct lpfc_queue *fpeq;
	struct lpfc_eqe *eqe;
	unsigned long iflags;
	int rc, idx;

	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (piocb->iocb_flag &  LPFC_IO_FCP) {
			if (unlikely(!phba->sli4_hba.fcp_wq))
				return IOCB_ERROR;
			idx = lpfc_sli4_scmd_to_wqidx_distr(phba);
			piocb->fcp_wqidx = idx;
			ring_number = MAX_SLI3_CONFIGURED_RINGS + idx;

			pring = &phba->sli.ring[ring_number];
			spin_lock_irqsave(&pring->ring_lock, iflags);
			rc = __lpfc_sli_issue_iocb(phba, ring_number, piocb,
				flag);
			spin_unlock_irqrestore(&pring->ring_lock, iflags);

			if (lpfc_fcp_look_ahead) {
				fcp_eq_hdl = &phba->sli4_hba.fcp_eq_hdl[idx];

				if (atomic_dec_and_test(&fcp_eq_hdl->
					fcp_eq_in_use)) {

					/* Get associated EQ with this index */
					fpeq = phba->sli4_hba.hba_eq[idx];

					/* Turn off interrupts from this EQ */
					lpfc_sli4_eq_clr_intr(fpeq);

					/*
					 * Process all the events on FCP EQ
					 */
					while ((eqe = lpfc_sli4_eq_get(fpeq))) {
						lpfc_sli4_hba_handle_eqe(phba,
							eqe, idx);
						fpeq->EQ_processed++;
					}

					/* Always clear and re-arm the EQ */
					lpfc_sli4_eq_release(fpeq,
						LPFC_QUEUE_REARM);
				}
				atomic_inc(&fcp_eq_hdl->fcp_eq_in_use);
			}
		} else {
			pring = &phba->sli.ring[ring_number];
			spin_lock_irqsave(&pring->ring_lock, iflags);
			rc = __lpfc_sli_issue_iocb(phba, ring_number, piocb,
				flag);
			spin_unlock_irqrestore(&pring->ring_lock, iflags);

		}
	} else {
		/* For now, SLI2/3 will still use hbalock */
		spin_lock_irqsave(&phba->hbalock, iflags);
		rc = __lpfc_sli_issue_iocb(phba, ring_number, piocb, flag);
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}
	return rc;
}

/**
 * lpfc_extra_ring_setup - Extra ring setup function
 * @phba: Pointer to HBA context object.
 *
 * This function is called while driver attaches with the
 * HBA to setup the extra ring. The extra ring is used
 * only when driver needs to support target mode functionality
 * or IP over FC functionalities.
 *
 * This function is called with no lock held.
 **/
static int
lpfc_extra_ring_setup( struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;

	psli = &phba->sli;

	/* Adjust cmd/rsp ring iocb entries more evenly */

	/* Take some away from the FCP ring */
	pring = &psli->ring[psli->fcp_ring];
	pring->sli.sli3.numCiocb -= SLI2_IOCB_CMD_R1XTRA_ENTRIES;
	pring->sli.sli3.numRiocb -= SLI2_IOCB_RSP_R1XTRA_ENTRIES;
	pring->sli.sli3.numCiocb -= SLI2_IOCB_CMD_R3XTRA_ENTRIES;
	pring->sli.sli3.numRiocb -= SLI2_IOCB_RSP_R3XTRA_ENTRIES;

	/* and give them to the extra ring */
	pring = &psli->ring[psli->extra_ring];

	pring->sli.sli3.numCiocb += SLI2_IOCB_CMD_R1XTRA_ENTRIES;
	pring->sli.sli3.numRiocb += SLI2_IOCB_RSP_R1XTRA_ENTRIES;
	pring->sli.sli3.numCiocb += SLI2_IOCB_CMD_R3XTRA_ENTRIES;
	pring->sli.sli3.numRiocb += SLI2_IOCB_RSP_R3XTRA_ENTRIES;

	/* Setup default profile for this ring */
	pring->iotag_max = 4096;
	pring->num_mask = 1;
	pring->prt[0].profile = 0;      /* Mask 0 */
	pring->prt[0].rctl = phba->cfg_multi_ring_rctl;
	pring->prt[0].type = phba->cfg_multi_ring_type;
	pring->prt[0].lpfc_sli_rcv_unsol_event = NULL;
	return 0;
}

/* lpfc_sli_abts_err_handler - handle a failed ABTS request from an SLI3 port.
 * @phba: Pointer to HBA context object.
 * @iocbq: Pointer to iocb object.
 *
 * The async_event handler calls this routine when it receives
 * an ASYNC_STATUS_CN event from the port.  The port generates
 * this event when an Abort Sequence request to an rport fails
 * twice in succession.  The abort could be originated by the
 * driver or by the port.  The ABTS could have been for an ELS
 * or FCP IO.  The port only generates this event when an ABTS
 * fails to complete after one retry.
 */
static void
lpfc_sli_abts_err_handler(struct lpfc_hba *phba,
			  struct lpfc_iocbq *iocbq)
{
	struct lpfc_nodelist *ndlp = NULL;
	uint16_t rpi = 0, vpi = 0;
	struct lpfc_vport *vport = NULL;

	/* The rpi in the ulpContext is vport-sensitive. */
	vpi = iocbq->iocb.un.asyncstat.sub_ctxt_tag;
	rpi = iocbq->iocb.ulpContext;

	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"3092 Port generated ABTS async event "
			"on vpi %d rpi %d status 0x%x\n",
			vpi, rpi, iocbq->iocb.ulpStatus);

	vport = lpfc_find_vport_by_vpid(phba, vpi);
	if (!vport)
		goto err_exit;
	ndlp = lpfc_findnode_rpi(vport, rpi);
	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp))
		goto err_exit;

	if (iocbq->iocb.ulpStatus == IOSTAT_LOCAL_REJECT)
		lpfc_sli_abts_recover_port(vport, ndlp);
	return;

 err_exit:
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3095 Event Context not found, no "
			"action on vpi %d rpi %d status 0x%x, reason 0x%x\n",
			iocbq->iocb.ulpContext, iocbq->iocb.ulpStatus,
			vpi, rpi);
}

/* lpfc_sli4_abts_err_handler - handle a failed ABTS request from an SLI4 port.
 * @phba: pointer to HBA context object.
 * @ndlp: nodelist pointer for the impacted rport.
 * @axri: pointer to the wcqe containing the failed exchange.
 *
 * The driver calls this routine when it receives an ABORT_XRI_FCP CQE from the
 * port.  The port generates this event when an abort exchange request to an
 * rport fails twice in succession with no reply.  The abort could be originated
 * by the driver or by the port.  The ABTS could have been for an ELS or FCP IO.
 */
void
lpfc_sli4_abts_err_handler(struct lpfc_hba *phba,
			   struct lpfc_nodelist *ndlp,
			   struct sli4_wcqe_xri_aborted *axri)
{
	struct lpfc_vport *vport;
	uint32_t ext_status = 0;

	if (!ndlp || !NLP_CHK_NODE_ACT(ndlp)) {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"3115 Node Context not found, driver "
				"ignoring abts err event\n");
		return;
	}

	vport = ndlp->vport;
	lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
			"3116 Port generated FCP XRI ABORT event on "
			"vpi %d rpi %d xri x%x status 0x%x parameter x%x\n",
			ndlp->vport->vpi, ndlp->nlp_rpi,
			bf_get(lpfc_wcqe_xa_xri, axri),
			bf_get(lpfc_wcqe_xa_status, axri),
			axri->parameter);

	/*
	 * Catch the ABTS protocol failure case.  Older OCe FW releases returned
	 * LOCAL_REJECT and 0 for a failed ABTS exchange and later OCe and
	 * LPe FW releases returned LOCAL_REJECT and SEQUENCE_TIMEOUT.
	 */
	ext_status = axri->parameter & IOERR_PARAM_MASK;
	if ((bf_get(lpfc_wcqe_xa_status, axri) == IOSTAT_LOCAL_REJECT) &&
	    ((ext_status == IOERR_SEQUENCE_TIMEOUT) || (ext_status == 0)))
		lpfc_sli_abts_recover_port(vport, ndlp);
}

/**
 * lpfc_sli_async_event_handler - ASYNC iocb handler function
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @iocbq: Pointer to iocb object.
 *
 * This function is called by the slow ring event handler
 * function when there is an ASYNC event iocb in the ring.
 * This function is called with no lock held.
 * Currently this function handles only temperature related
 * ASYNC events. The function decodes the temperature sensor
 * event message and posts events for the management applications.
 **/
static void
lpfc_sli_async_event_handler(struct lpfc_hba * phba,
	struct lpfc_sli_ring * pring, struct lpfc_iocbq * iocbq)
{
	IOCB_t *icmd;
	uint16_t evt_code;
	struct temp_event temp_event_data;
	struct Scsi_Host *shost;
	uint32_t *iocb_w;

	icmd = &iocbq->iocb;
	evt_code = icmd->un.asyncstat.evt_code;

	switch (evt_code) {
	case ASYNC_TEMP_WARN:
	case ASYNC_TEMP_SAFE:
		temp_event_data.data = (uint32_t) icmd->ulpContext;
		temp_event_data.event_type = FC_REG_TEMPERATURE_EVENT;
		if (evt_code == ASYNC_TEMP_WARN) {
			temp_event_data.event_code = LPFC_THRESHOLD_TEMP;
			lpfc_printf_log(phba, KERN_ERR, LOG_TEMP,
				"0347 Adapter is very hot, please take "
				"corrective action. temperature : %d Celsius\n",
				(uint32_t) icmd->ulpContext);
		} else {
			temp_event_data.event_code = LPFC_NORMAL_TEMP;
			lpfc_printf_log(phba, KERN_ERR, LOG_TEMP,
				"0340 Adapter temperature is OK now. "
				"temperature : %d Celsius\n",
				(uint32_t) icmd->ulpContext);
		}

		/* Send temperature change event to applications */
		shost = lpfc_shost_from_vport(phba->pport);
		fc_host_post_vendor_event(shost, fc_get_event_number(),
			sizeof(temp_event_data), (char *) &temp_event_data,
			LPFC_NL_VENDOR_ID);
		break;
	case ASYNC_STATUS_CN:
		lpfc_sli_abts_err_handler(phba, iocbq);
		break;
	default:
		iocb_w = (uint32_t *) icmd;
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0346 Ring %d handler: unexpected ASYNC_STATUS"
			" evt_code 0x%x\n"
			"W0  0x%08x W1  0x%08x W2  0x%08x W3  0x%08x\n"
			"W4  0x%08x W5  0x%08x W6  0x%08x W7  0x%08x\n"
			"W8  0x%08x W9  0x%08x W10 0x%08x W11 0x%08x\n"
			"W12 0x%08x W13 0x%08x W14 0x%08x W15 0x%08x\n",
			pring->ringno, icmd->un.asyncstat.evt_code,
			iocb_w[0], iocb_w[1], iocb_w[2], iocb_w[3],
			iocb_w[4], iocb_w[5], iocb_w[6], iocb_w[7],
			iocb_w[8], iocb_w[9], iocb_w[10], iocb_w[11],
			iocb_w[12], iocb_w[13], iocb_w[14], iocb_w[15]);

		break;
	}
}


/**
 * lpfc_sli_setup - SLI ring setup function
 * @phba: Pointer to HBA context object.
 *
 * lpfc_sli_setup sets up rings of the SLI interface with
 * number of iocbs per ring and iotags. This function is
 * called while driver attach to the HBA and before the
 * interrupts are enabled. So there is no need for locking.
 *
 * This function always returns 0.
 **/
int
lpfc_sli_setup(struct lpfc_hba *phba)
{
	int i, totiocbsize = 0;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;

	psli->num_rings = MAX_SLI3_CONFIGURED_RINGS;
	if (phba->sli_rev == LPFC_SLI_REV4)
		psli->num_rings += phba->cfg_fcp_io_channel;
	psli->sli_flag = 0;
	psli->fcp_ring = LPFC_FCP_RING;
	psli->next_ring = LPFC_FCP_NEXT_RING;
	psli->extra_ring = LPFC_EXTRA_RING;

	psli->iocbq_lookup = NULL;
	psli->iocbq_lookup_len = 0;
	psli->last_iotag = 0;

	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		switch (i) {
		case LPFC_FCP_RING:	/* ring 0 - FCP */
			/* numCiocb and numRiocb are used in config_port */
			pring->sli.sli3.numCiocb = SLI2_IOCB_CMD_R0_ENTRIES;
			pring->sli.sli3.numRiocb = SLI2_IOCB_RSP_R0_ENTRIES;
			pring->sli.sli3.numCiocb +=
				SLI2_IOCB_CMD_R1XTRA_ENTRIES;
			pring->sli.sli3.numRiocb +=
				SLI2_IOCB_RSP_R1XTRA_ENTRIES;
			pring->sli.sli3.numCiocb +=
				SLI2_IOCB_CMD_R3XTRA_ENTRIES;
			pring->sli.sli3.numRiocb +=
				SLI2_IOCB_RSP_R3XTRA_ENTRIES;
			pring->sli.sli3.sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sli.sli3.sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->iotag_ctr = 0;
			pring->iotag_max =
			    (phba->cfg_hba_queue_depth * 2);
			pring->fast_iotag = pring->iotag_max;
			pring->num_mask = 0;
			break;
		case LPFC_EXTRA_RING:	/* ring 1 - EXTRA */
			/* numCiocb and numRiocb are used in config_port */
			pring->sli.sli3.numCiocb = SLI2_IOCB_CMD_R1_ENTRIES;
			pring->sli.sli3.numRiocb = SLI2_IOCB_RSP_R1_ENTRIES;
			pring->sli.sli3.sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sli.sli3.sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->iotag_max = phba->cfg_hba_queue_depth;
			pring->num_mask = 0;
			break;
		case LPFC_ELS_RING:	/* ring 2 - ELS / CT */
			/* numCiocb and numRiocb are used in config_port */
			pring->sli.sli3.numCiocb = SLI2_IOCB_CMD_R2_ENTRIES;
			pring->sli.sli3.numRiocb = SLI2_IOCB_RSP_R2_ENTRIES;
			pring->sli.sli3.sizeCiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_CMD_SIZE :
							SLI2_IOCB_CMD_SIZE;
			pring->sli.sli3.sizeRiocb = (phba->sli_rev == 3) ?
							SLI3_IOCB_RSP_SIZE :
							SLI2_IOCB_RSP_SIZE;
			pring->fast_iotag = 0;
			pring->iotag_ctr = 0;
			pring->iotag_max = 4096;
			pring->lpfc_sli_rcv_async_status =
				lpfc_sli_async_event_handler;
			pring->num_mask = LPFC_MAX_RING_MASK;
			pring->prt[0].profile = 0;	/* Mask 0 */
			pring->prt[0].rctl = FC_RCTL_ELS_REQ;
			pring->prt[0].type = FC_TYPE_ELS;
			pring->prt[0].lpfc_sli_rcv_unsol_event =
			    lpfc_els_unsol_event;
			pring->prt[1].profile = 0;	/* Mask 1 */
			pring->prt[1].rctl = FC_RCTL_ELS_REP;
			pring->prt[1].type = FC_TYPE_ELS;
			pring->prt[1].lpfc_sli_rcv_unsol_event =
			    lpfc_els_unsol_event;
			pring->prt[2].profile = 0;	/* Mask 2 */
			/* NameServer Inquiry */
			pring->prt[2].rctl = FC_RCTL_DD_UNSOL_CTL;
			/* NameServer */
			pring->prt[2].type = FC_TYPE_CT;
			pring->prt[2].lpfc_sli_rcv_unsol_event =
			    lpfc_ct_unsol_event;
			pring->prt[3].profile = 0;	/* Mask 3 */
			/* NameServer response */
			pring->prt[3].rctl = FC_RCTL_DD_SOL_CTL;
			/* NameServer */
			pring->prt[3].type = FC_TYPE_CT;
			pring->prt[3].lpfc_sli_rcv_unsol_event =
			    lpfc_ct_unsol_event;
			break;
		}
		totiocbsize += (pring->sli.sli3.numCiocb *
			pring->sli.sli3.sizeCiocb) +
			(pring->sli.sli3.numRiocb * pring->sli.sli3.sizeRiocb);
	}
	if (totiocbsize > MAX_SLIM_IOCB_SIZE) {
		/* Too many cmd / rsp ring entries in SLI2 SLIM */
		printk(KERN_ERR "%d:0462 Too many cmd / rsp ring entries in "
		       "SLI2 SLIM Data: x%x x%lx\n",
		       phba->brd_no, totiocbsize,
		       (unsigned long) MAX_SLIM_IOCB_SIZE);
	}
	if (phba->cfg_multi_ring_support == 2)
		lpfc_extra_ring_setup(phba);

	return 0;
}

/**
 * lpfc_sli_queue_setup - Queue initialization function
 * @phba: Pointer to HBA context object.
 *
 * lpfc_sli_queue_setup sets up mailbox queues and iocb queues for each
 * ring. This function also initializes ring indices of each ring.
 * This function is called during the initialization of the SLI
 * interface of an HBA.
 * This function is called with no lock held and always returns
 * 1.
 **/
int
lpfc_sli_queue_setup(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli;
	struct lpfc_sli_ring *pring;
	int i;

	psli = &phba->sli;
	spin_lock_irq(&phba->hbalock);
	INIT_LIST_HEAD(&psli->mboxq);
	INIT_LIST_HEAD(&psli->mboxq_cmpl);
	/* Initialize list headers for txq and txcmplq as double linked lists */
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		pring->ringno = i;
		pring->sli.sli3.next_cmdidx  = 0;
		pring->sli.sli3.local_getidx = 0;
		pring->sli.sli3.cmdidx = 0;
		INIT_LIST_HEAD(&pring->txq);
		INIT_LIST_HEAD(&pring->txcmplq);
		INIT_LIST_HEAD(&pring->iocb_continueq);
		INIT_LIST_HEAD(&pring->iocb_continue_saveq);
		INIT_LIST_HEAD(&pring->postbufq);
		spin_lock_init(&pring->ring_lock);
	}
	spin_unlock_irq(&phba->hbalock);
	return 1;
}

/**
 * lpfc_sli_mbox_sys_flush - Flush mailbox command sub-system
 * @phba: Pointer to HBA context object.
 *
 * This routine flushes the mailbox command subsystem. It will unconditionally
 * flush all the mailbox commands in the three possible stages in the mailbox
 * command sub-system: pending mailbox command queue; the outstanding mailbox
 * command; and completed mailbox command queue. It is caller's responsibility
 * to make sure that the driver is in the proper state to flush the mailbox
 * command sub-system. Namely, the posting of mailbox commands into the
 * pending mailbox command queue from the various clients must be stopped;
 * either the HBA is in a state that it will never works on the outstanding
 * mailbox command (such as in EEH or ERATT conditions) or the outstanding
 * mailbox command has been completed.
 **/
static void
lpfc_sli_mbox_sys_flush(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli = &phba->sli;
	LPFC_MBOXQ_t *pmb;
	unsigned long iflag;

	/* Flush all the mailbox commands in the mbox system */
	spin_lock_irqsave(&phba->hbalock, iflag);
	/* The pending mailbox command queue */
	list_splice_init(&phba->sli.mboxq, &completions);
	/* The outstanding active mailbox command */
	if (psli->mbox_active) {
		list_add_tail(&psli->mbox_active->list, &completions);
		psli->mbox_active = NULL;
		psli->sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	}
	/* The completed mailbox command queue */
	list_splice_init(&phba->sli.mboxq_cmpl, &completions);
	spin_unlock_irqrestore(&phba->hbalock, iflag);

	/* Return all flushed mailbox commands with MBX_NOT_FINISHED status */
	while (!list_empty(&completions)) {
		list_remove_head(&completions, pmb, LPFC_MBOXQ_t, list);
		pmb->u.mb.mbxStatus = MBX_NOT_FINISHED;
		if (pmb->mbox_cmpl)
			pmb->mbox_cmpl(phba, pmb);
	}
}

/**
 * lpfc_sli_host_down - Vport cleanup function
 * @vport: Pointer to virtual port object.
 *
 * lpfc_sli_host_down is called to clean up the resources
 * associated with a vport before destroying virtual
 * port data structures.
 * This function does following operations:
 * - Free discovery resources associated with this virtual
 *   port.
 * - Free iocbs associated with this virtual port in
 *   the txq.
 * - Send abort for all iocb commands associated with this
 *   vport in txcmplq.
 *
 * This function is called with no lock held and always returns 1.
 **/
int
lpfc_sli_host_down(struct lpfc_vport *vport)
{
	LIST_HEAD(completions);
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_iocbq *iocb, *next_iocb;
	int i;
	unsigned long flags = 0;
	uint16_t prev_pring_flag;

	lpfc_cleanup_discovery_resources(vport);

	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		prev_pring_flag = pring->flag;
		/* Only slow rings */
		if (pring->ringno == LPFC_ELS_RING) {
			pring->flag |= LPFC_DEFERRED_RING_EVENT;
			/* Set the lpfc data pending flag */
			set_bit(LPFC_DATA_READY, &phba->data_flags);
		}
		/*
		 * Error everything on the txq since these iocbs have not been
		 * given to the FW yet.
		 */
		list_for_each_entry_safe(iocb, next_iocb, &pring->txq, list) {
			if (iocb->vport != vport)
				continue;
			list_move_tail(&iocb->list, &completions);
		}

		/* Next issue ABTS for everything on the txcmplq */
		list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq,
									list) {
			if (iocb->vport != vport)
				continue;
			lpfc_sli_issue_abort_iotag(phba, pring, iocb);
		}

		pring->flag = prev_pring_flag;
	}

	spin_unlock_irqrestore(&phba->hbalock, flags);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);
	return 1;
}

/**
 * lpfc_sli_hba_down - Resource cleanup function for the HBA
 * @phba: Pointer to HBA context object.
 *
 * This function cleans up all iocb, buffers, mailbox commands
 * while shutting down the HBA. This function is called with no
 * lock held and always returns 1.
 * This function does the following to cleanup driver resources:
 * - Free discovery resources for each virtual port
 * - Cleanup any pending fabric iocbs
 * - Iterate through the iocb txq and free each entry
 *   in the list.
 * - Free up any buffer posted to the HBA
 * - Free mailbox commands in the mailbox queue.
 **/
int
lpfc_sli_hba_down(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	struct lpfc_dmabuf *buf_ptr;
	unsigned long flags = 0;
	int i;

	/* Shutdown the mailbox command sub-system */
	lpfc_sli_mbox_sys_shutdown(phba, LPFC_MBX_WAIT);

	lpfc_hba_down_prep(phba);

	lpfc_fabric_abort_hba(phba);

	spin_lock_irqsave(&phba->hbalock, flags);
	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		/* Only slow rings */
		if (pring->ringno == LPFC_ELS_RING) {
			pring->flag |= LPFC_DEFERRED_RING_EVENT;
			/* Set the lpfc data pending flag */
			set_bit(LPFC_DATA_READY, &phba->data_flags);
		}

		/*
		 * Error everything on the txq since these iocbs have not been
		 * given to the FW yet.
		 */
		list_splice_init(&pring->txq, &completions);
	}
	spin_unlock_irqrestore(&phba->hbalock, flags);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_DOWN);

	spin_lock_irqsave(&phba->hbalock, flags);
	list_splice_init(&phba->elsbuf, &completions);
	phba->elsbuf_cnt = 0;
	phba->elsbuf_prev_cnt = 0;
	spin_unlock_irqrestore(&phba->hbalock, flags);

	while (!list_empty(&completions)) {
		list_remove_head(&completions, buf_ptr,
			struct lpfc_dmabuf, list);
		lpfc_mbuf_free(phba, buf_ptr->virt, buf_ptr->phys);
		kfree(buf_ptr);
	}

	/* Return any active mbox cmds */
	del_timer_sync(&psli->mbox_tmo);

	spin_lock_irqsave(&phba->pport->work_port_lock, flags);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, flags);

	return 1;
}

/**
 * lpfc_sli_pcimem_bcopy - SLI memory copy function
 * @srcp: Source memory pointer.
 * @destp: Destination memory pointer.
 * @cnt: Number of words required to be copied.
 *
 * This function is used for copying data between driver memory
 * and the SLI memory. This function also changes the endianness
 * of each word if native endianness is different from SLI
 * endianness. This function can be called with or without
 * lock.
 **/
void
lpfc_sli_pcimem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint32_t *src = srcp;
	uint32_t *dest = destp;
	uint32_t ldata;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof (uint32_t)) {
		ldata = *src;
		ldata = le32_to_cpu(ldata);
		*dest = ldata;
		src++;
		dest++;
	}
}


/**
 * lpfc_sli_bemem_bcopy - SLI memory copy function
 * @srcp: Source memory pointer.
 * @destp: Destination memory pointer.
 * @cnt: Number of words required to be copied.
 *
 * This function is used for copying data between a data structure
 * with big endian representation to local endianness.
 * This function can be called with or without lock.
 **/
void
lpfc_sli_bemem_bcopy(void *srcp, void *destp, uint32_t cnt)
{
	uint32_t *src = srcp;
	uint32_t *dest = destp;
	uint32_t ldata;
	int i;

	for (i = 0; i < (int)cnt; i += sizeof(uint32_t)) {
		ldata = *src;
		ldata = be32_to_cpu(ldata);
		*dest = ldata;
		src++;
		dest++;
	}
}

/**
 * lpfc_sli_ringpostbuf_put - Function to add a buffer to postbufq
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @mp: Pointer to driver buffer object.
 *
 * This function is called with no lock held.
 * It always return zero after adding the buffer to the postbufq
 * buffer list.
 **/
int
lpfc_sli_ringpostbuf_put(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 struct lpfc_dmabuf *mp)
{
	/* Stick struct lpfc_dmabuf at end of postbufq so driver can look it up
	   later */
	spin_lock_irq(&phba->hbalock);
	list_add_tail(&mp->list, &pring->postbufq);
	pring->postbufq_cnt++;
	spin_unlock_irq(&phba->hbalock);
	return 0;
}

/**
 * lpfc_sli_get_buffer_tag - allocates a tag for a CMD_QUE_XRI64_CX buffer
 * @phba: Pointer to HBA context object.
 *
 * When HBQ is enabled, buffers are searched based on tags. This function
 * allocates a tag for buffer posted using CMD_QUE_XRI64_CX iocb. The
 * tag is bit wise or-ed with QUE_BUFTAG_BIT to make sure that the tag
 * does not conflict with tags of buffer posted for unsolicited events.
 * The function returns the allocated tag. The function is called with
 * no locks held.
 **/
uint32_t
lpfc_sli_get_buffer_tag(struct lpfc_hba *phba)
{
	spin_lock_irq(&phba->hbalock);
	phba->buffer_tag_count++;
	/*
	 * Always set the QUE_BUFTAG_BIT to distiguish between
	 * a tag assigned by HBQ.
	 */
	phba->buffer_tag_count |= QUE_BUFTAG_BIT;
	spin_unlock_irq(&phba->hbalock);
	return phba->buffer_tag_count;
}

/**
 * lpfc_sli_ring_taggedbuf_get - find HBQ buffer associated with given tag
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @tag: Buffer tag.
 *
 * Buffers posted using CMD_QUE_XRI64_CX iocb are in pring->postbufq
 * list. After HBA DMA data to these buffers, CMD_IOCB_RET_XRI64_CX
 * iocb is posted to the response ring with the tag of the buffer.
 * This function searches the pring->postbufq list using the tag
 * to find buffer associated with CMD_IOCB_RET_XRI64_CX
 * iocb. If the buffer is found then lpfc_dmabuf object of the
 * buffer is returned to the caller else NULL is returned.
 * This function is called with no lock held.
 **/
struct lpfc_dmabuf *
lpfc_sli_ring_taggedbuf_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			uint32_t tag)
{
	struct lpfc_dmabuf *mp, *next_mp;
	struct list_head *slp = &pring->postbufq;

	/* Search postbufq, from the beginning, looking for a match on tag */
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
		if (mp->buffer_tag == tag) {
			list_del_init(&mp->list);
			pring->postbufq_cnt--;
			spin_unlock_irq(&phba->hbalock);
			return mp;
		}
	}

	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0402 Cannot find virtual addr for buffer tag on "
			"ring %d Data x%lx x%p x%p x%x\n",
			pring->ringno, (unsigned long) tag,
			slp->next, slp->prev, pring->postbufq_cnt);

	return NULL;
}

/**
 * lpfc_sli_ringpostbuf_get - search buffers for unsolicited CT and ELS events
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @phys: DMA address of the buffer.
 *
 * This function searches the buffer list using the dma_address
 * of unsolicited event to find the driver's lpfc_dmabuf object
 * corresponding to the dma_address. The function returns the
 * lpfc_dmabuf object if a buffer is found else it returns NULL.
 * This function is called by the ct and els unsolicited event
 * handlers to get the buffer associated with the unsolicited
 * event.
 *
 * This function is called with no lock held.
 **/
struct lpfc_dmabuf *
lpfc_sli_ringpostbuf_get(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			 dma_addr_t phys)
{
	struct lpfc_dmabuf *mp, *next_mp;
	struct list_head *slp = &pring->postbufq;

	/* Search postbufq, from the beginning, looking for a match on phys */
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mp, next_mp, &pring->postbufq, list) {
		if (mp->phys == phys) {
			list_del_init(&mp->list);
			pring->postbufq_cnt--;
			spin_unlock_irq(&phba->hbalock);
			return mp;
		}
	}

	spin_unlock_irq(&phba->hbalock);
	lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"0410 Cannot find virtual addr for mapped buf on "
			"ring %d Data x%llx x%p x%p x%x\n",
			pring->ringno, (unsigned long long)phys,
			slp->next, slp->prev, pring->postbufq_cnt);
	return NULL;
}

/**
 * lpfc_sli_abort_els_cmpl - Completion handler for the els abort iocbs
 * @phba: Pointer to HBA context object.
 * @cmdiocb: Pointer to driver command iocb object.
 * @rspiocb: Pointer to driver response iocb object.
 *
 * This function is the completion handler for the abort iocbs for
 * ELS commands. This function is called from the ELS ring event
 * handler with no lock held. This function frees memory resources
 * associated with the abort iocb.
 **/
static void
lpfc_sli_abort_els_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	IOCB_t *irsp = &rspiocb->iocb;
	uint16_t abort_iotag, abort_context;
	struct lpfc_iocbq *abort_iocb = NULL;

	if (irsp->ulpStatus) {

		/*
		 * Assume that the port already completed and returned, or
		 * will return the iocb. Just Log the message.
		 */
		abort_context = cmdiocb->iocb.un.acxri.abortContextTag;
		abort_iotag = cmdiocb->iocb.un.acxri.abortIoTag;

		spin_lock_irq(&phba->hbalock);
		if (phba->sli_rev < LPFC_SLI_REV4) {
			if (abort_iotag != 0 &&
				abort_iotag <= phba->sli.last_iotag)
				abort_iocb =
					phba->sli.iocbq_lookup[abort_iotag];
		} else
			/* For sli4 the abort_tag is the XRI,
			 * so the abort routine puts the iotag  of the iocb
			 * being aborted in the context field of the abort
			 * IOCB.
			 */
			abort_iocb = phba->sli.iocbq_lookup[abort_context];

		lpfc_printf_log(phba, KERN_WARNING, LOG_ELS | LOG_SLI,
				"0327 Cannot abort els iocb %p "
				"with tag %x context %x, abort status %x, "
				"abort code %x\n",
				abort_iocb, abort_iotag, abort_context,
				irsp->ulpStatus, irsp->un.ulpWord[4]);

		spin_unlock_irq(&phba->hbalock);
	}
	lpfc_sli_release_iocbq(phba, cmdiocb);
	return;
}

/**
 * lpfc_ignore_els_cmpl - Completion handler for aborted ELS command
 * @phba: Pointer to HBA context object.
 * @cmdiocb: Pointer to driver command iocb object.
 * @rspiocb: Pointer to driver response iocb object.
 *
 * The function is called from SLI ring event handler with no
 * lock held. This function is the completion handler for ELS commands
 * which are aborted. The function frees memory resources used for
 * the aborted ELS commands.
 **/
static void
lpfc_ignore_els_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
		     struct lpfc_iocbq *rspiocb)
{
	IOCB_t *irsp = &rspiocb->iocb;

	/* ELS cmd tag <ulpIoTag> completes */
	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"0139 Ignoring ELS cmd tag x%x completion Data: "
			"x%x x%x x%x\n",
			irsp->ulpIoTag, irsp->ulpStatus,
			irsp->un.ulpWord[4], irsp->ulpTimeout);
	if (cmdiocb->iocb.ulpCommand == CMD_GEN_REQUEST64_CR)
		lpfc_ct_free_iocb(phba, cmdiocb);
	else
		lpfc_els_free_iocb(phba, cmdiocb);
	return;
}

/**
 * lpfc_sli_abort_iotag_issue - Issue abort for a command iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @cmdiocb: Pointer to driver command iocb object.
 *
 * This function issues an abort iocb for the provided command iocb down to
 * the port. Other than the case the outstanding command iocb is an abort
 * request, this function issues abort out unconditionally. This function is
 * called with hbalock held. The function returns 0 when it fails due to
 * memory allocation failure or when the command iocb is an abort request.
 **/
static int
lpfc_sli_abort_iotag_issue(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			   struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	struct lpfc_iocbq *abtsiocbp;
	IOCB_t *icmd = NULL;
	IOCB_t *iabt = NULL;
	int retval;
	unsigned long iflags;

	/*
	 * There are certain command types we don't want to abort.  And we
	 * don't want to abort commands that are already in the process of
	 * being aborted.
	 */
	icmd = &cmdiocb->iocb;
	if (icmd->ulpCommand == CMD_ABORT_XRI_CN ||
	    icmd->ulpCommand == CMD_CLOSE_XRI_CN ||
	    (cmdiocb->iocb_flag & LPFC_DRIVER_ABORTED) != 0)
		return 0;

	/* issue ABTS for this IOCB based on iotag */
	abtsiocbp = __lpfc_sli_get_iocbq(phba);
	if (abtsiocbp == NULL)
		return 0;

	/* This signals the response to set the correct status
	 * before calling the completion handler
	 */
	cmdiocb->iocb_flag |= LPFC_DRIVER_ABORTED;

	iabt = &abtsiocbp->iocb;
	iabt->un.acxri.abortType = ABORT_TYPE_ABTS;
	iabt->un.acxri.abortContextTag = icmd->ulpContext;
	if (phba->sli_rev == LPFC_SLI_REV4) {
		iabt->un.acxri.abortIoTag = cmdiocb->sli4_xritag;
		iabt->un.acxri.abortContextTag = cmdiocb->iotag;
	}
	else
		iabt->un.acxri.abortIoTag = icmd->ulpIoTag;
	iabt->ulpLe = 1;
	iabt->ulpClass = icmd->ulpClass;

	/* ABTS WQE must go to the same WQ as the WQE to be aborted */
	abtsiocbp->fcp_wqidx = cmdiocb->fcp_wqidx;
	if (cmdiocb->iocb_flag & LPFC_IO_FCP)
		abtsiocbp->iocb_flag |= LPFC_USE_FCPWQIDX;

	if (phba->link_state >= LPFC_LINK_UP)
		iabt->ulpCommand = CMD_ABORT_XRI_CN;
	else
		iabt->ulpCommand = CMD_CLOSE_XRI_CN;

	abtsiocbp->iocb_cmpl = lpfc_sli_abort_els_cmpl;
	abtsiocbp->vport = vport;

	lpfc_printf_vlog(vport, KERN_INFO, LOG_SLI,
			 "0339 Abort xri x%x, original iotag x%x, "
			 "abort cmd iotag x%x\n",
			 iabt->un.acxri.abortIoTag,
			 iabt->un.acxri.abortContextTag,
			 abtsiocbp->iotag);

	if (phba->sli_rev == LPFC_SLI_REV4) {
		/* Note: both hbalock and ring_lock need to be set here */
		spin_lock_irqsave(&pring->ring_lock, iflags);
		retval = __lpfc_sli_issue_iocb(phba, pring->ringno,
			abtsiocbp, 0);
		spin_unlock_irqrestore(&pring->ring_lock, iflags);
	} else {
		retval = __lpfc_sli_issue_iocb(phba, pring->ringno,
			abtsiocbp, 0);
	}

	if (retval)
		__lpfc_sli_release_iocbq(phba, abtsiocbp);

	/*
	 * Caller to this routine should check for IOCB_ERROR
	 * and handle it properly.  This routine no longer removes
	 * iocb off txcmplq and call compl in case of IOCB_ERROR.
	 */
	return retval;
}

/**
 * lpfc_sli_issue_abort_iotag - Abort function for a command iocb
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 * @cmdiocb: Pointer to driver command iocb object.
 *
 * This function issues an abort iocb for the provided command iocb. In case
 * of unloading, the abort iocb will not be issued to commands on the ELS
 * ring. Instead, the callback function shall be changed to those commands
 * so that nothing happens when them finishes. This function is called with
 * hbalock held. The function returns 0 when the command iocb is an abort
 * request.
 **/
int
lpfc_sli_issue_abort_iotag(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
			   struct lpfc_iocbq *cmdiocb)
{
	struct lpfc_vport *vport = cmdiocb->vport;
	int retval = IOCB_ERROR;
	IOCB_t *icmd = NULL;

	/*
	 * There are certain command types we don't want to abort.  And we
	 * don't want to abort commands that are already in the process of
	 * being aborted.
	 */
	icmd = &cmdiocb->iocb;
	if (icmd->ulpCommand == CMD_ABORT_XRI_CN ||
	    icmd->ulpCommand == CMD_CLOSE_XRI_CN ||
	    (cmdiocb->iocb_flag & LPFC_DRIVER_ABORTED) != 0)
		return 0;

	/*
	 * If we're unloading, don't abort iocb on the ELS ring, but change
	 * the callback so that nothing happens when it finishes.
	 */
	if ((vport->load_flag & FC_UNLOADING) &&
	    (pring->ringno == LPFC_ELS_RING)) {
		if (cmdiocb->iocb_flag & LPFC_IO_FABRIC)
			cmdiocb->fabric_iocb_cmpl = lpfc_ignore_els_cmpl;
		else
			cmdiocb->iocb_cmpl = lpfc_ignore_els_cmpl;
		goto abort_iotag_exit;
	}

	/* Now, we try to issue the abort to the cmdiocb out */
	retval = lpfc_sli_abort_iotag_issue(phba, pring, cmdiocb);

abort_iotag_exit:
	/*
	 * Caller to this routine should check for IOCB_ERROR
	 * and handle it properly.  This routine no longer removes
	 * iocb off txcmplq and call compl in case of IOCB_ERROR.
	 */
	return retval;
}

/**
 * lpfc_sli_iocb_ring_abort - Unconditionally abort all iocbs on an iocb ring
 * @phba: Pointer to HBA context object.
 * @pring: Pointer to driver SLI ring object.
 *
 * This function aborts all iocbs in the given ring and frees all the iocb
 * objects in txq. This function issues abort iocbs unconditionally for all
 * the iocb commands in txcmplq. The iocbs in the txcmplq is not guaranteed
 * to complete before the return of this function. The caller is not required
 * to hold any locks.
 **/
static void
lpfc_sli_iocb_ring_abort(struct lpfc_hba *phba, struct lpfc_sli_ring *pring)
{
	LIST_HEAD(completions);
	struct lpfc_iocbq *iocb, *next_iocb;

	if (pring->ringno == LPFC_ELS_RING)
		lpfc_fabric_abort_hba(phba);

	spin_lock_irq(&phba->hbalock);

	/* Take off all the iocbs on txq for cancelling */
	list_splice_init(&pring->txq, &completions);
	pring->txq_cnt = 0;

	/* Next issue ABTS for everything on the txcmplq */
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list)
		lpfc_sli_abort_iotag_issue(phba, pring, iocb);

	spin_unlock_irq(&phba->hbalock);

	/* Cancel all the IOCBs from the completions list */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
			      IOERR_SLI_ABORTED);
}

/**
 * lpfc_sli_hba_iocb_abort - Abort all iocbs to an hba.
 * @phba: pointer to lpfc HBA data structure.
 *
 * This routine will abort all pending and outstanding iocbs to an HBA.
 **/
void
lpfc_sli_hba_iocb_abort(struct lpfc_hba *phba)
{
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_sli_ring *pring;
	int i;

	for (i = 0; i < psli->num_rings; i++) {
		pring = &psli->ring[i];
		lpfc_sli_iocb_ring_abort(phba, pring);
	}
}

/**
 * lpfc_sli_validate_fcp_iocb - find commands associated with a vport or LUN
 * @iocbq: Pointer to driver iocb object.
 * @vport: Pointer to driver virtual port object.
 * @tgt_id: SCSI ID of the target.
 * @lun_id: LUN ID of the scsi device.
 * @ctx_cmd: LPFC_CTX_LUN/LPFC_CTX_TGT/LPFC_CTX_HOST
 *
 * This function acts as an iocb filter for functions which abort or count
 * all FCP iocbs pending on a lun/SCSI target/SCSI host. It will return
 * 0 if the filtering criteria is met for the given iocb and will return
 * 1 if the filtering criteria is not met.
 * If ctx_cmd == LPFC_CTX_LUN, the function returns 0 only if the
 * given iocb is for the SCSI device specified by vport, tgt_id and
 * lun_id parameter.
 * If ctx_cmd == LPFC_CTX_TGT,  the function returns 0 only if the
 * given iocb is for the SCSI target specified by vport and tgt_id
 * parameters.
 * If ctx_cmd == LPFC_CTX_HOST, the function returns 0 only if the
 * given iocb is for the SCSI host associated with the given vport.
 * This function is called with no locks held.
 **/
static int
lpfc_sli_validate_fcp_iocb(struct lpfc_iocbq *iocbq, struct lpfc_vport *vport,
			   uint16_t tgt_id, uint64_t lun_id,
			   lpfc_ctx_cmd ctx_cmd)
{
	struct lpfc_scsi_buf *lpfc_cmd;
	int rc = 1;

	if (!(iocbq->iocb_flag &  LPFC_IO_FCP))
		return rc;

	if (iocbq->vport != vport)
		return rc;

	lpfc_cmd = container_of(iocbq, struct lpfc_scsi_buf, cur_iocbq);

	if (lpfc_cmd->pCmd == NULL)
		return rc;

	switch (ctx_cmd) {
	case LPFC_CTX_LUN:
		if ((lpfc_cmd->rdata->pnode) &&
		    (lpfc_cmd->rdata->pnode->nlp_sid == tgt_id) &&
		    (scsilun_to_int(&lpfc_cmd->fcp_cmnd->fcp_lun) == lun_id))
			rc = 0;
		break;
	case LPFC_CTX_TGT:
		if ((lpfc_cmd->rdata->pnode) &&
		    (lpfc_cmd->rdata->pnode->nlp_sid == tgt_id))
			rc = 0;
		break;
	case LPFC_CTX_HOST:
		rc = 0;
		break;
	default:
		printk(KERN_ERR "%s: Unknown context cmd type, value %d\n",
			__func__, ctx_cmd);
		break;
	}

	return rc;
}

/**
 * lpfc_sli_sum_iocb - Function to count the number of FCP iocbs pending
 * @vport: Pointer to virtual port.
 * @tgt_id: SCSI ID of the target.
 * @lun_id: LUN ID of the scsi device.
 * @ctx_cmd: LPFC_CTX_LUN/LPFC_CTX_TGT/LPFC_CTX_HOST.
 *
 * This function returns number of FCP commands pending for the vport.
 * When ctx_cmd == LPFC_CTX_LUN, the function returns number of FCP
 * commands pending on the vport associated with SCSI device specified
 * by tgt_id and lun_id parameters.
 * When ctx_cmd == LPFC_CTX_TGT, the function returns number of FCP
 * commands pending on the vport associated with SCSI target specified
 * by tgt_id parameter.
 * When ctx_cmd == LPFC_CTX_HOST, the function returns number of FCP
 * commands pending on the vport.
 * This function returns the number of iocbs which satisfy the filter.
 * This function is called without any lock held.
 **/
int
lpfc_sli_sum_iocb(struct lpfc_vport *vport, uint16_t tgt_id, uint64_t lun_id,
		  lpfc_ctx_cmd ctx_cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *iocbq;
	int sum, i;

	for (i = 1, sum = 0; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (lpfc_sli_validate_fcp_iocb (iocbq, vport, tgt_id, lun_id,
						ctx_cmd) == 0)
			sum++;
	}

	return sum;
}

/**
 * lpfc_sli_abort_fcp_cmpl - Completion handler function for aborted FCP IOCBs
 * @phba: Pointer to HBA context object
 * @cmdiocb: Pointer to command iocb object.
 * @rspiocb: Pointer to response iocb object.
 *
 * This function is called when an aborted FCP iocb completes. This
 * function is called by the ring event handler with no lock held.
 * This function frees the iocb.
 **/
void
lpfc_sli_abort_fcp_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *cmdiocb,
			struct lpfc_iocbq *rspiocb)
{
	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"3096 ABORT_XRI_CN completing on xri x%x "
			"original iotag x%x, abort cmd iotag x%x "
			"status 0x%x, reason 0x%x\n",
			cmdiocb->iocb.un.acxri.abortContextTag,
			cmdiocb->iocb.un.acxri.abortIoTag,
			cmdiocb->iotag, rspiocb->iocb.ulpStatus,
			rspiocb->iocb.un.ulpWord[4]);
	lpfc_sli_release_iocbq(phba, cmdiocb);
	return;
}

/**
 * lpfc_sli_abort_iocb - issue abort for all commands on a host/target/LUN
 * @vport: Pointer to virtual port.
 * @pring: Pointer to driver SLI ring object.
 * @tgt_id: SCSI ID of the target.
 * @lun_id: LUN ID of the scsi device.
 * @abort_cmd: LPFC_CTX_LUN/LPFC_CTX_TGT/LPFC_CTX_HOST.
 *
 * This function sends an abort command for every SCSI command
 * associated with the given virtual port pending on the ring
 * filtered by lpfc_sli_validate_fcp_iocb function.
 * When abort_cmd == LPFC_CTX_LUN, the function sends abort only to the
 * FCP iocbs associated with lun specified by tgt_id and lun_id
 * parameters
 * When abort_cmd == LPFC_CTX_TGT, the function sends abort only to the
 * FCP iocbs associated with SCSI target specified by tgt_id parameter.
 * When abort_cmd == LPFC_CTX_HOST, the function sends abort to all
 * FCP iocbs associated with virtual port.
 * This function returns number of iocbs it failed to abort.
 * This function is called with no locks held.
 **/
int
lpfc_sli_abort_iocb(struct lpfc_vport *vport, struct lpfc_sli_ring *pring,
		    uint16_t tgt_id, uint64_t lun_id, lpfc_ctx_cmd abort_cmd)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *iocbq;
	struct lpfc_iocbq *abtsiocb;
	IOCB_t *cmd = NULL;
	int errcnt = 0, ret_val = 0;
	int i;

	for (i = 1; i <= phba->sli.last_iotag; i++) {
		iocbq = phba->sli.iocbq_lookup[i];

		if (lpfc_sli_validate_fcp_iocb(iocbq, vport, tgt_id, lun_id,
					       abort_cmd) != 0)
			continue;

		/* issue ABTS for this IOCB based on iotag */
		abtsiocb = lpfc_sli_get_iocbq(phba);
		if (abtsiocb == NULL) {
			errcnt++;
			continue;
		}

		cmd = &iocbq->iocb;
		abtsiocb->iocb.un.acxri.abortType = ABORT_TYPE_ABTS;
		abtsiocb->iocb.un.acxri.abortContextTag = cmd->ulpContext;
		if (phba->sli_rev == LPFC_SLI_REV4)
			abtsiocb->iocb.un.acxri.abortIoTag = iocbq->sli4_xritag;
		else
			abtsiocb->iocb.un.acxri.abortIoTag = cmd->ulpIoTag;
		abtsiocb->iocb.ulpLe = 1;
		abtsiocb->iocb.ulpClass = cmd->ulpClass;
		abtsiocb->vport = phba->pport;

		/* ABTS WQE must go to the same WQ as the WQE to be aborted */
		abtsiocb->fcp_wqidx = iocbq->fcp_wqidx;
		if (iocbq->iocb_flag & LPFC_IO_FCP)
			abtsiocb->iocb_flag |= LPFC_USE_FCPWQIDX;

		if (lpfc_is_link_up(phba))
			abtsiocb->iocb.ulpCommand = CMD_ABORT_XRI_CN;
		else
			abtsiocb->iocb.ulpCommand = CMD_CLOSE_XRI_CN;

		/* Setup callback routine and issue the command. */
		abtsiocb->iocb_cmpl = lpfc_sli_abort_fcp_cmpl;
		ret_val = lpfc_sli_issue_iocb(phba, pring->ringno,
					      abtsiocb, 0);
		if (ret_val == IOCB_ERROR) {
			lpfc_sli_release_iocbq(phba, abtsiocb);
			errcnt++;
			continue;
		}
	}

	return errcnt;
}

/**
 * lpfc_sli_wake_iocb_wait - lpfc_sli_issue_iocb_wait's completion handler
 * @phba: Pointer to HBA context object.
 * @cmdiocbq: Pointer to command iocb.
 * @rspiocbq: Pointer to response iocb.
 *
 * This function is the completion handler for iocbs issued using
 * lpfc_sli_issue_iocb_wait function. This function is called by the
 * ring event handler function without any lock held. This function
 * can be called from both worker thread context and interrupt
 * context. This function also can be called from other thread which
 * cleans up the SLI layer objects.
 * This function copy the contents of the response iocb to the
 * response iocb memory object provided by the caller of
 * lpfc_sli_issue_iocb_wait and then wakes up the thread which
 * sleeps for the iocb completion.
 **/
static void
lpfc_sli_wake_iocb_wait(struct lpfc_hba *phba,
			struct lpfc_iocbq *cmdiocbq,
			struct lpfc_iocbq *rspiocbq)
{
	wait_queue_head_t *pdone_q;
	unsigned long iflags;
	struct lpfc_scsi_buf *lpfc_cmd;

	spin_lock_irqsave(&phba->hbalock, iflags);
	cmdiocbq->iocb_flag |= LPFC_IO_WAKE;
	if (cmdiocbq->context2 && rspiocbq)
		memcpy(&((struct lpfc_iocbq *)cmdiocbq->context2)->iocb,
		       &rspiocbq->iocb, sizeof(IOCB_t));

	/* Set the exchange busy flag for task management commands */
	if ((cmdiocbq->iocb_flag & LPFC_IO_FCP) &&
		!(cmdiocbq->iocb_flag & LPFC_IO_LIBDFC)) {
		lpfc_cmd = container_of(cmdiocbq, struct lpfc_scsi_buf,
			cur_iocbq);
		lpfc_cmd->exch_busy = rspiocbq->iocb_flag & LPFC_EXCHANGE_BUSY;
	}

	pdone_q = cmdiocbq->context_un.wait_queue;
	if (pdone_q)
		wake_up(pdone_q);
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return;
}

/**
 * lpfc_chk_iocb_flg - Test IOCB flag with lock held.
 * @phba: Pointer to HBA context object..
 * @piocbq: Pointer to command iocb.
 * @flag: Flag to test.
 *
 * This routine grabs the hbalock and then test the iocb_flag to
 * see if the passed in flag is set.
 * Returns:
 * 1 if flag is set.
 * 0 if flag is not set.
 **/
static int
lpfc_chk_iocb_flg(struct lpfc_hba *phba,
		 struct lpfc_iocbq *piocbq, uint32_t flag)
{
	unsigned long iflags;
	int ret;

	spin_lock_irqsave(&phba->hbalock, iflags);
	ret = piocbq->iocb_flag & flag;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	return ret;

}

/**
 * lpfc_sli_issue_iocb_wait - Synchronous function to issue iocb commands
 * @phba: Pointer to HBA context object..
 * @pring: Pointer to sli ring.
 * @piocb: Pointer to command iocb.
 * @prspiocbq: Pointer to response iocb.
 * @timeout: Timeout in number of seconds.
 *
 * This function issues the iocb to firmware and waits for the
 * iocb to complete. If the iocb command is not
 * completed within timeout seconds, it returns IOCB_TIMEDOUT.
 * Caller should not free the iocb resources if this function
 * returns IOCB_TIMEDOUT.
 * The function waits for the iocb completion using an
 * non-interruptible wait.
 * This function will sleep while waiting for iocb completion.
 * So, this function should not be called from any context which
 * does not allow sleeping. Due to the same reason, this function
 * cannot be called with interrupt disabled.
 * This function assumes that the iocb completions occur while
 * this function sleep. So, this function cannot be called from
 * the thread which process iocb completion for this ring.
 * This function clears the iocb_flag of the iocb object before
 * issuing the iocb and the iocb completion handler sets this
 * flag and wakes this thread when the iocb completes.
 * The contents of the response iocb will be copied to prspiocbq
 * by the completion handler when the command completes.
 * This function returns IOCB_SUCCESS when success.
 * This function is called with no lock held.
 **/
int
lpfc_sli_issue_iocb_wait(struct lpfc_hba *phba,
			 uint32_t ring_number,
			 struct lpfc_iocbq *piocb,
			 struct lpfc_iocbq *prspiocbq,
			 uint32_t timeout)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_q);
	long timeleft, timeout_req = 0;
	int retval = IOCB_SUCCESS;
	uint32_t creg_val;
	struct lpfc_iocbq *iocb;
	int txq_cnt = 0;
	int txcmplq_cnt = 0;
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	/*
	 * If the caller has provided a response iocbq buffer, then context2
	 * is NULL or its an error.
	 */
	if (prspiocbq) {
		if (piocb->context2)
			return IOCB_ERROR;
		piocb->context2 = prspiocbq;
	}

	piocb->iocb_cmpl = lpfc_sli_wake_iocb_wait;
	piocb->context_un.wait_queue = &done_q;
	piocb->iocb_flag &= ~LPFC_IO_WAKE;

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		if (lpfc_readl(phba->HCregaddr, &creg_val))
			return IOCB_ERROR;
		creg_val |= (HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr); /* flush */
	}

	retval = lpfc_sli_issue_iocb(phba, ring_number, piocb,
				     SLI_IOCB_RET_IOCB);
	if (retval == IOCB_SUCCESS) {
		timeout_req = msecs_to_jiffies(timeout * 1000);
		timeleft = wait_event_timeout(done_q,
				lpfc_chk_iocb_flg(phba, piocb, LPFC_IO_WAKE),
				timeout_req);

		if (piocb->iocb_flag & LPFC_IO_WAKE) {
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
					"0331 IOCB wake signaled\n");
		} else if (timeleft == 0) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0338 IOCB wait timeout error - no "
					"wake response Data x%x\n", timeout);
			retval = IOCB_TIMEDOUT;
		} else {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0330 IOCB wake NOT set, "
					"Data x%x x%lx\n",
					timeout, (timeleft / jiffies));
			retval = IOCB_TIMEDOUT;
		}
	} else if (retval == IOCB_BUSY) {
		if (phba->cfg_log_verbose & LOG_SLI) {
			list_for_each_entry(iocb, &pring->txq, list) {
				txq_cnt++;
			}
			list_for_each_entry(iocb, &pring->txcmplq, list) {
				txcmplq_cnt++;
			}
			lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"2818 Max IOCBs %d txq cnt %d txcmplq cnt %d\n",
				phba->iocb_cnt, txq_cnt, txcmplq_cnt);
		}
		return retval;
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
				"0332 IOCB wait issue failed, Data x%x\n",
				retval);
		retval = IOCB_ERROR;
	}

	if (phba->cfg_poll & DISABLE_FCP_RING_INT) {
		if (lpfc_readl(phba->HCregaddr, &creg_val))
			return IOCB_ERROR;
		creg_val &= ~(HC_R0INT_ENA << LPFC_FCP_RING);
		writel(creg_val, phba->HCregaddr);
		readl(phba->HCregaddr); /* flush */
	}

	if (prspiocbq)
		piocb->context2 = NULL;

	piocb->context_un.wait_queue = NULL;
	piocb->iocb_cmpl = NULL;
	return retval;
}

/**
 * lpfc_sli_issue_mbox_wait - Synchronous function to issue mailbox
 * @phba: Pointer to HBA context object.
 * @pmboxq: Pointer to driver mailbox object.
 * @timeout: Timeout in number of seconds.
 *
 * This function issues the mailbox to firmware and waits for the
 * mailbox command to complete. If the mailbox command is not
 * completed within timeout seconds, it returns MBX_TIMEOUT.
 * The function waits for the mailbox completion using an
 * interruptible wait. If the thread is woken up due to a
 * signal, MBX_TIMEOUT error is returned to the caller. Caller
 * should not free the mailbox resources, if this function returns
 * MBX_TIMEOUT.
 * This function will sleep while waiting for mailbox completion.
 * So, this function should not be called from any context which
 * does not allow sleeping. Due to the same reason, this function
 * cannot be called with interrupt disabled.
 * This function assumes that the mailbox completion occurs while
 * this function sleep. So, this function cannot be called from
 * the worker thread which processes mailbox completion.
 * This function is called in the context of HBA management
 * applications.
 * This function returns MBX_SUCCESS when successful.
 * This function is called with no lock held.
 **/
int
lpfc_sli_issue_mbox_wait(struct lpfc_hba *phba, LPFC_MBOXQ_t *pmboxq,
			 uint32_t timeout)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(done_q);
	int retval;
	unsigned long flag;

	/* The caller must leave context1 empty. */
	if (pmboxq->context1)
		return MBX_NOT_FINISHED;

	pmboxq->mbox_flag &= ~LPFC_MBX_WAKE;
	/* setup wake call as IOCB callback */
	pmboxq->mbox_cmpl = lpfc_sli_wake_mbox_wait;
	/* setup context field to pass wait_queue pointer to wake function  */
	pmboxq->context1 = &done_q;

	/* now issue the command */
	retval = lpfc_sli_issue_mbox(phba, pmboxq, MBX_NOWAIT);
	if (retval == MBX_BUSY || retval == MBX_SUCCESS) {
		wait_event_interruptible_timeout(done_q,
				pmboxq->mbox_flag & LPFC_MBX_WAKE,
				msecs_to_jiffies(timeout * 1000));

		spin_lock_irqsave(&phba->hbalock, flag);
		pmboxq->context1 = NULL;
		/*
		 * if LPFC_MBX_WAKE flag is set the mailbox is completed
		 * else do not free the resources.
		 */
		if (pmboxq->mbox_flag & LPFC_MBX_WAKE) {
			retval = MBX_SUCCESS;
			lpfc_sli4_swap_str(phba, pmboxq);
		} else {
			retval = MBX_TIMEOUT;
			pmboxq->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		}
		spin_unlock_irqrestore(&phba->hbalock, flag);
	}

	return retval;
}

/**
 * lpfc_sli_mbox_sys_shutdown - shutdown mailbox command sub-system
 * @phba: Pointer to HBA context.
 *
 * This function is called to shutdown the driver's mailbox sub-system.
 * It first marks the mailbox sub-system is in a block state to prevent
 * the asynchronous mailbox command from issued off the pending mailbox
 * command queue. If the mailbox command sub-system shutdown is due to
 * HBA error conditions such as EEH or ERATT, this routine shall invoke
 * the mailbox sub-system flush routine to forcefully bring down the
 * mailbox sub-system. Otherwise, if it is due to normal condition (such
 * as with offline or HBA function reset), this routine will wait for the
 * outstanding mailbox command to complete before invoking the mailbox
 * sub-system flush routine to gracefully bring down mailbox sub-system.
 **/
void
lpfc_sli_mbox_sys_shutdown(struct lpfc_hba *phba, int mbx_action)
{
	struct lpfc_sli *psli = &phba->sli;
	unsigned long timeout;

	if (mbx_action == LPFC_MBX_NO_WAIT) {
		/* delay 100ms for port state */
		msleep(100);
		lpfc_sli_mbox_sys_flush(phba);
		return;
	}
	timeout = msecs_to_jiffies(LPFC_MBOX_TMO * 1000) + jiffies;

	spin_lock_irq(&phba->hbalock);
	psli->sli_flag |= LPFC_SLI_ASYNC_MBX_BLK;

	if (psli->sli_flag & LPFC_SLI_ACTIVE) {
		/* Determine how long we might wait for the active mailbox
		 * command to be gracefully completed by firmware.
		 */
		if (phba->sli.mbox_active)
			timeout = msecs_to_jiffies(lpfc_mbox_tmo_val(phba,
						phba->sli.mbox_active) *
						1000) + jiffies;
		spin_unlock_irq(&phba->hbalock);

		while (phba->sli.mbox_active) {
			/* Check active mailbox complete status every 2ms */
			msleep(2);
			if (time_after(jiffies, timeout))
				/* Timeout, let the mailbox flush routine to
				 * forcefully release active mailbox command
				 */
				break;
		}
	} else
		spin_unlock_irq(&phba->hbalock);

	lpfc_sli_mbox_sys_flush(phba);
}

/**
 * lpfc_sli_eratt_read - read sli-3 error attention events
 * @phba: Pointer to HBA context.
 *
 * This function is called to read the SLI3 device error attention registers
 * for possible error attention events. The caller must hold the hostlock
 * with spin_lock_irq().
 *
 * This function returns 1 when there is Error Attention in the Host Attention
 * Register and returns 0 otherwise.
 **/
static int
lpfc_sli_eratt_read(struct lpfc_hba *phba)
{
	uint32_t ha_copy;

	/* Read chip Host Attention (HA) register */
	if (lpfc_readl(phba->HAregaddr, &ha_copy))
		goto unplug_err;

	if (ha_copy & HA_ERATT) {
		/* Read host status register to retrieve error event */
		if (lpfc_sli_read_hs(phba))
			goto unplug_err;

		/* Check if there is a deferred error condition is active */
		if ((HS_FFER1 & phba->work_hs) &&
		    ((HS_FFER2 | HS_FFER3 | HS_FFER4 | HS_FFER5 |
		      HS_FFER6 | HS_FFER7 | HS_FFER8) & phba->work_hs)) {
			phba->hba_flag |= DEFER_ERATT;
			/* Clear all interrupt enable conditions */
			writel(0, phba->HCregaddr);
			readl(phba->HCregaddr);
		}

		/* Set the driver HA work bitmap */
		phba->work_ha |= HA_ERATT;
		/* Indicate polling handles this ERATT */
		phba->hba_flag |= HBA_ERATT_HANDLED;
		return 1;
	}
	return 0;

unplug_err:
	/* Set the driver HS work bitmap */
	phba->work_hs |= UNPLUG_ERR;
	/* Set the driver HA work bitmap */
	phba->work_ha |= HA_ERATT;
	/* Indicate polling handles this ERATT */
	phba->hba_flag |= HBA_ERATT_HANDLED;
	return 1;
}

/**
 * lpfc_sli4_eratt_read - read sli-4 error attention events
 * @phba: Pointer to HBA context.
 *
 * This function is called to read the SLI4 device error attention registers
 * for possible error attention events. The caller must hold the hostlock
 * with spin_lock_irq().
 *
 * This function returns 1 when there is Error Attention in the Host Attention
 * Register and returns 0 otherwise.
 **/
static int
lpfc_sli4_eratt_read(struct lpfc_hba *phba)
{
	uint32_t uerr_sta_hi, uerr_sta_lo;
	uint32_t if_type, portsmphr;
	struct lpfc_register portstat_reg;

	/*
	 * For now, use the SLI4 device internal unrecoverable error
	 * registers for error attention. This can be changed later.
	 */
	if_type = bf_get(lpfc_sli_intf_if_type, &phba->sli4_hba.sli_intf);
	switch (if_type) {
	case LPFC_SLI_INTF_IF_TYPE_0:
		if (lpfc_readl(phba->sli4_hba.u.if_type0.UERRLOregaddr,
			&uerr_sta_lo) ||
			lpfc_readl(phba->sli4_hba.u.if_type0.UERRHIregaddr,
			&uerr_sta_hi)) {
			phba->work_hs |= UNPLUG_ERR;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		if ((~phba->sli4_hba.ue_mask_lo & uerr_sta_lo) ||
		    (~phba->sli4_hba.ue_mask_hi & uerr_sta_hi)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"1423 HBA Unrecoverable error: "
					"uerr_lo_reg=0x%x, uerr_hi_reg=0x%x, "
					"ue_mask_lo_reg=0x%x, "
					"ue_mask_hi_reg=0x%x\n",
					uerr_sta_lo, uerr_sta_hi,
					phba->sli4_hba.ue_mask_lo,
					phba->sli4_hba.ue_mask_hi);
			phba->work_status[0] = uerr_sta_lo;
			phba->work_status[1] = uerr_sta_hi;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_2:
		if (lpfc_readl(phba->sli4_hba.u.if_type2.STATUSregaddr,
			&portstat_reg.word0) ||
			lpfc_readl(phba->sli4_hba.PSMPHRregaddr,
			&portsmphr)){
			phba->work_hs |= UNPLUG_ERR;
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		if (bf_get(lpfc_sliport_status_err, &portstat_reg)) {
			phba->work_status[0] =
				readl(phba->sli4_hba.u.if_type2.ERR1regaddr);
			phba->work_status[1] =
				readl(phba->sli4_hba.u.if_type2.ERR2regaddr);
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"2885 Port Status Event: "
					"port status reg 0x%x, "
					"port smphr reg 0x%x, "
					"error 1=0x%x, error 2=0x%x\n",
					portstat_reg.word0,
					portsmphr,
					phba->work_status[0],
					phba->work_status[1]);
			phba->work_ha |= HA_ERATT;
			phba->hba_flag |= HBA_ERATT_HANDLED;
			return 1;
		}
		break;
	case LPFC_SLI_INTF_IF_TYPE_1:
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2886 HBA Error Attention on unsupported "
				"if type %d.", if_type);
		return 1;
	}

	return 0;
}

/**
 * lpfc_sli_check_eratt - check error attention events
 * @phba: Pointer to HBA context.
 *
 * This function is called from timer soft interrupt context to check HBA's
 * error attention register bit for error attention events.
 *
 * This function returns 1 when there is Error Attention in the Host Attention
 * Register and returns 0 otherwise.
 **/
int
lpfc_sli_check_eratt(struct lpfc_hba *phba)
{
	uint32_t ha_copy;

	/* If somebody is waiting to handle an eratt, don't process it
	 * here. The brdkill function will do this.
	 */
	if (phba->link_flag & LS_IGNORE_ERATT)
		return 0;

	/* Check if interrupt handler handles this ERATT */
	spin_lock_irq(&phba->hbalock);
	if (phba->hba_flag & HBA_ERATT_HANDLED) {
		/* Interrupt handler has handled ERATT */
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	/*
	 * If there is deferred error attention, do not check for error
	 * attention
	 */
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	/* If PCI channel is offline, don't process it */
	if (unlikely(pci_channel_offline(phba->pcidev))) {
		spin_unlock_irq(&phba->hbalock);
		return 0;
	}

	switch (phba->sli_rev) {
	case LPFC_SLI_REV2:
	case LPFC_SLI_REV3:
		/* Read chip Host Attention (HA) register */
		ha_copy = lpfc_sli_eratt_read(phba);
		break;
	case LPFC_SLI_REV4:
		/* Read device Uncoverable Error (UERR) registers */
		ha_copy = lpfc_sli4_eratt_read(phba);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0299 Invalid SLI revision (%d)\n",
				phba->sli_rev);
		ha_copy = 0;
		break;
	}
	spin_unlock_irq(&phba->hbalock);

	return ha_copy;
}

/**
 * lpfc_intr_state_check - Check device state for interrupt handling
 * @phba: Pointer to HBA context.
 *
 * This inline routine checks whether a device or its PCI slot is in a state
 * that the interrupt should be handled.
 *
 * This function returns 0 if the device or the PCI slot is in a state that
 * interrupt should be handled, otherwise -EIO.
 */
static inline int
lpfc_intr_state_check(struct lpfc_hba *phba)
{
	/* If the pci channel is offline, ignore all the interrupts */
	if (unlikely(pci_channel_offline(phba->pcidev)))
		return -EIO;

	/* Update device level interrupt statistics */
	phba->sli.slistat.sli_intr++;

	/* Ignore all interrupts during initialization. */
	if (unlikely(phba->link_state < LPFC_LINK_DOWN))
		return -EIO;

	return 0;
}

/**
 * lpfc_sli_sp_intr_handler - Slow-path interrupt handler to SLI-3 device
 * @irq: Interrupt number.
 * @dev_id: The device context pointer.
 *
 * This function is directly called from the PCI layer as an interrupt
 * service routine when device with SLI-3 interface spec is enabled with
 * MSI-X multi-message interrupt mode and there are slow-path events in
 * the HBA. However, when the device is enabled with either MSI or Pin-IRQ
 * interrupt mode, this function is called as part of the device-level
 * interrupt handler. When the PCI slot is in error recovery or the HBA
 * is undergoing initialization, the interrupt handler will not process
 * the interrupt. The link attention and ELS ring attention events are
 * handled by the worker thread. The interrupt handler signals the worker
 * thread and returns for these events. This function is called without
 * any lock held. It gets the hbalock to access and update SLI data
 * structures.
 *
 * This function returns IRQ_HANDLED when interrupt is handled else it
 * returns IRQ_NONE.
 **/
irqreturn_t
lpfc_sli_sp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	uint32_t ha_copy, hc_copy;
	uint32_t work_ha_copy;
	unsigned long status;
	unsigned long iflag;
	uint32_t control;

	MAILBOX_t *mbox, *pmbox;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *mp;
	LPFC_MBOXQ_t *pmb;
	int rc;

	/*
	 * Get the driver's phba structure from the dev_id and
	 * assume the HBA is not interrupting.
	 */
	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	/*
	 * Stuff needs to be attented to when this function is invoked as an
	 * individual interrupt handler in MSI-X multi-message interrupt mode
	 */
	if (phba->intr_type == MSIX) {
		/* Check device state for handling interrupt */
		if (lpfc_intr_state_check(phba))
			return IRQ_NONE;
		/* Need to read HA REG for slow-path events */
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			goto unplug_error;
		/* If somebody is waiting to handle an eratt don't process it
		 * here. The brdkill function will do this.
		 */
		if (phba->link_flag & LS_IGNORE_ERATT)
			ha_copy &= ~HA_ERATT;
		/* Check the need for handling ERATT in interrupt handler */
		if (ha_copy & HA_ERATT) {
			if (phba->hba_flag & HBA_ERATT_HANDLED)
				/* ERATT polling has handled ERATT */
				ha_copy &= ~HA_ERATT;
			else
				/* Indicate interrupt handler handles ERATT */
				phba->hba_flag |= HBA_ERATT_HANDLED;
		}

		/*
		 * If there is deferred error attention, do not check for any
		 * interrupt.
		 */
		if (unlikely(phba->hba_flag & DEFER_ERATT)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return IRQ_NONE;
		}

		/* Clear up only attention source related to slow-path */
		if (lpfc_readl(phba->HCregaddr, &hc_copy))
			goto unplug_error;

		writel(hc_copy & ~(HC_MBINT_ENA | HC_R2INT_ENA |
			HC_LAINT_ENA | HC_ERINT_ENA),
			phba->HCregaddr);
		writel((ha_copy & (HA_MBATT | HA_R2_CLR_MSK)),
			phba->HAregaddr);
		writel(hc_copy, phba->HCregaddr);
		readl(phba->HAregaddr); /* flush */
		spin_unlock_irqrestore(&phba->hbalock, iflag);
	} else
		ha_copy = phba->ha_copy;

	work_ha_copy = ha_copy & phba->work_ha_mask;

	if (work_ha_copy) {
		if (work_ha_copy & HA_LATT) {
			if (phba->sli.sli_flag & LPFC_PROCESS_LA) {
				/*
				 * Turn off Link Attention interrupts
				 * until CLEAR_LA done
				 */
				spin_lock_irqsave(&phba->hbalock, iflag);
				phba->sli.sli_flag &= ~LPFC_PROCESS_LA;
				if (lpfc_readl(phba->HCregaddr, &control))
					goto unplug_error;
				control &= ~HC_LAINT_ENA;
				writel(control, phba->HCregaddr);
				readl(phba->HCregaddr); /* flush */
				spin_unlock_irqrestore(&phba->hbalock, iflag);
			}
			else
				work_ha_copy &= ~HA_LATT;
		}

		if (work_ha_copy & ~(HA_ERATT | HA_MBATT | HA_LATT)) {
			/*
			 * Turn off Slow Rings interrupts, LPFC_ELS_RING is
			 * the only slow ring.
			 */
			status = (work_ha_copy &
				(HA_RXMASK  << (4*LPFC_ELS_RING)));
			status >>= (4*LPFC_ELS_RING);
			if (status & HA_RXMASK) {
				spin_lock_irqsave(&phba->hbalock, iflag);
				if (lpfc_readl(phba->HCregaddr, &control))
					goto unplug_error;

				lpfc_debugfs_slow_ring_trc(phba,
				"ISR slow ring:   ctl:x%x stat:x%x isrcnt:x%x",
				control, status,
				(uint32_t)phba->sli.slistat.sli_intr);

				if (control & (HC_R0INT_ENA << LPFC_ELS_RING)) {
					lpfc_debugfs_slow_ring_trc(phba,
						"ISR Disable ring:"
						"pwork:x%x hawork:x%x wait:x%x",
						phba->work_ha, work_ha_copy,
						(uint32_t)((unsigned long)
						&phba->work_waitq));

					control &=
					    ~(HC_R0INT_ENA << LPFC_ELS_RING);
					writel(control, phba->HCregaddr);
					readl(phba->HCregaddr); /* flush */
				}
				else {
					lpfc_debugfs_slow_ring_trc(phba,
						"ISR slow ring:   pwork:"
						"x%x hawork:x%x wait:x%x",
						phba->work_ha, work_ha_copy,
						(uint32_t)((unsigned long)
						&phba->work_waitq));
				}
				spin_unlock_irqrestore(&phba->hbalock, iflag);
			}
		}
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (work_ha_copy & HA_ERATT) {
			if (lpfc_sli_read_hs(phba))
				goto unplug_error;
			/*
			 * Check if there is a deferred error condition
			 * is active
			 */
			if ((HS_FFER1 & phba->work_hs) &&
				((HS_FFER2 | HS_FFER3 | HS_FFER4 | HS_FFER5 |
				  HS_FFER6 | HS_FFER7 | HS_FFER8) &
				  phba->work_hs)) {
				phba->hba_flag |= DEFER_ERATT;
				/* Clear all interrupt enable conditions */
				writel(0, phba->HCregaddr);
				readl(phba->HCregaddr);
			}
		}

		if ((work_ha_copy & HA_MBATT) && (phba->sli.mbox_active)) {
			pmb = phba->sli.mbox_active;
			pmbox = &pmb->u.mb;
			mbox = phba->mbox;
			vport = pmb->vport;

			/* First check out the status word */
			lpfc_sli_pcimem_bcopy(mbox, pmbox, sizeof(uint32_t));
			if (pmbox->mbxOwner != OWN_HOST) {
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				/*
				 * Stray Mailbox Interrupt, mbxCommand <cmd>
				 * mbxStatus <status>
				 */
				lpfc_printf_log(phba, KERN_ERR, LOG_MBOX |
						LOG_SLI,
						"(%d):0304 Stray Mailbox "
						"Interrupt mbxCommand x%x "
						"mbxStatus x%x\n",
						(vport ? vport->vpi : 0),
						pmbox->mbxCommand,
						pmbox->mbxStatus);
				/* clear mailbox attention bit */
				work_ha_copy &= ~HA_MBATT;
			} else {
				phba->sli.mbox_active = NULL;
				spin_unlock_irqrestore(&phba->hbalock, iflag);
				phba->last_completion_time = jiffies;
				del_timer(&phba->sli.mbox_tmo);
				if (pmb->mbox_cmpl) {
					lpfc_sli_pcimem_bcopy(mbox, pmbox,
							MAILBOX_CMD_SIZE);
					if (pmb->out_ext_byte_len &&
						pmb->context2)
						lpfc_sli_pcimem_bcopy(
						phba->mbox_ext,
						pmb->context2,
						pmb->out_ext_byte_len);
				}
				if (pmb->mbox_flag & LPFC_MBX_IMED_UNREG) {
					pmb->mbox_flag &= ~LPFC_MBX_IMED_UNREG;

					lpfc_debugfs_disc_trc(vport,
						LPFC_DISC_TRC_MBOX_VPORT,
						"MBOX dflt rpi: : "
						"status:x%x rpi:x%x",
						(uint32_t)pmbox->mbxStatus,
						pmbox->un.varWords[0], 0);

					if (!pmbox->mbxStatus) {
						mp = (struct lpfc_dmabuf *)
							(pmb->context1);
						ndlp = (struct lpfc_nodelist *)
							pmb->context2;

						/* Reg_LOGIN of dflt RPI was
						 * successful. new lets get
						 * rid of the RPI using the
						 * same mbox buffer.
						 */
						lpfc_unreg_login(phba,
							vport->vpi,
							pmbox->un.varWords[0],
							pmb);
						pmb->mbox_cmpl =
							lpfc_mbx_cmpl_dflt_rpi;
						pmb->context1 = mp;
						pmb->context2 = ndlp;
						pmb->vport = vport;
						rc = lpfc_sli_issue_mbox(phba,
								pmb,
								MBX_NOWAIT);
						if (rc != MBX_BUSY)
							lpfc_printf_log(phba,
							KERN_ERR,
							LOG_MBOX | LOG_SLI,
							"0350 rc should have"
							"been MBX_BUSY\n");
						if (rc != MBX_NOT_FINISHED)
							goto send_current_mbox;
					}
				}
				spin_lock_irqsave(
						&phba->pport->work_port_lock,
						iflag);
				phba->pport->work_port_events &=
					~WORKER_MBOX_TMO;
				spin_unlock_irqrestore(
						&phba->pport->work_port_lock,
						iflag);
				lpfc_mbox_cmpl_put(phba, pmb);
			}
		} else
			spin_unlock_irqrestore(&phba->hbalock, iflag);

		if ((work_ha_copy & HA_MBATT) &&
		    (phba->sli.mbox_active == NULL)) {
send_current_mbox:
			/* Process next mailbox command if there is one */
			do {
				rc = lpfc_sli_issue_mbox(phba, NULL,
							 MBX_NOWAIT);
			} while (rc == MBX_NOT_FINISHED);
			if (rc != MBX_SUCCESS)
				lpfc_printf_log(phba, KERN_ERR, LOG_MBOX |
						LOG_SLI, "0349 rc should be "
						"MBX_SUCCESS\n");
		}

		spin_lock_irqsave(&phba->hbalock, iflag);
		phba->work_ha |= work_ha_copy;
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		lpfc_worker_wake_up(phba);
	}
	return IRQ_HANDLED;
unplug_error:
	spin_unlock_irqrestore(&phba->hbalock, iflag);
	return IRQ_HANDLED;

} /* lpfc_sli_sp_intr_handler */

/**
 * lpfc_sli_fp_intr_handler - Fast-path interrupt handler to SLI-3 device.
 * @irq: Interrupt number.
 * @dev_id: The device context pointer.
 *
 * This function is directly called from the PCI layer as an interrupt
 * service routine when device with SLI-3 interface spec is enabled with
 * MSI-X multi-message interrupt mode and there is a fast-path FCP IOCB
 * ring event in the HBA. However, when the device is enabled with either
 * MSI or Pin-IRQ interrupt mode, this function is called as part of the
 * device-level interrupt handler. When the PCI slot is in error recovery
 * or the HBA is undergoing initialization, the interrupt handler will not
 * process the interrupt. The SCSI FCP fast-path ring event are handled in
 * the intrrupt context. This function is called without any lock held.
 * It gets the hbalock to access and update SLI data structures.
 *
 * This function returns IRQ_HANDLED when interrupt is handled else it
 * returns IRQ_NONE.
 **/
irqreturn_t
lpfc_sli_fp_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	uint32_t ha_copy;
	unsigned long status;
	unsigned long iflag;

	/* Get the driver's phba structure from the dev_id and
	 * assume the HBA is not interrupting.
	 */
	phba = (struct lpfc_hba *) dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	/*
	 * Stuff needs to be attented to when this function is invoked as an
	 * individual interrupt handler in MSI-X multi-message interrupt mode
	 */
	if (phba->intr_type == MSIX) {
		/* Check device state for handling interrupt */
		if (lpfc_intr_state_check(phba))
			return IRQ_NONE;
		/* Need to read HA REG for FCP ring and other ring events */
		if (lpfc_readl(phba->HAregaddr, &ha_copy))
			return IRQ_HANDLED;
		/* Clear up only attention source related to fast-path */
		spin_lock_irqsave(&phba->hbalock, iflag);
		/*
		 * If there is deferred error attention, do not check for
		 * any interrupt.
		 */
		if (unlikely(phba->hba_flag & DEFER_ERATT)) {
			spin_unlock_irqrestore(&phba->hbalock, iflag);
			return IRQ_NONE;
		}
		writel((ha_copy & (HA_R0_CLR_MSK | HA_R1_CLR_MSK)),
			phba->HAregaddr);
		readl(phba->HAregaddr); /* flush */
		spin_unlock_irqrestore(&phba->hbalock, iflag);
	} else
		ha_copy = phba->ha_copy;

	/*
	 * Process all events on FCP ring. Take the optimized path for FCP IO.
	 */
	ha_copy &= ~(phba->work_ha_mask);

	status = (ha_copy & (HA_RXMASK << (4*LPFC_FCP_RING)));
	status >>= (4*LPFC_FCP_RING);
	if (status & HA_RXMASK)
		lpfc_sli_handle_fast_ring_event(phba,
						&phba->sli.ring[LPFC_FCP_RING],
						status);

	if (phba->cfg_multi_ring_support == 2) {
		/*
		 * Process all events on extra ring. Take the optimized path
		 * for extra ring IO.
		 */
		status = (ha_copy & (HA_RXMASK << (4*LPFC_EXTRA_RING)));
		status >>= (4*LPFC_EXTRA_RING);
		if (status & HA_RXMASK) {
			lpfc_sli_handle_fast_ring_event(phba,
					&phba->sli.ring[LPFC_EXTRA_RING],
					status);
		}
	}
	return IRQ_HANDLED;
}  /* lpfc_sli_fp_intr_handler */

/**
 * lpfc_sli_intr_handler - Device-level interrupt handler to SLI-3 device
 * @irq: Interrupt number.
 * @dev_id: The device context pointer.
 *
 * This function is the HBA device-level interrupt handler to device with
 * SLI-3 interface spec, called from the PCI layer when either MSI or
 * Pin-IRQ interrupt mode is enabled and there is an event in the HBA which
 * requires driver attention. This function invokes the slow-path interrupt
 * attention handling function and fast-path interrupt attention handling
 * function in turn to process the relevant HBA attention events. This
 * function is called without any lock held. It gets the hbalock to access
 * and update SLI data structures.
 *
 * This function returns IRQ_HANDLED when interrupt is handled, else it
 * returns IRQ_NONE.
 **/
irqreturn_t
lpfc_sli_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	irqreturn_t sp_irq_rc, fp_irq_rc;
	unsigned long status1, status2;
	uint32_t hc_copy;

	/*
	 * Get the driver's phba structure from the dev_id and
	 * assume the HBA is not interrupting.
	 */
	phba = (struct lpfc_hba *) dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	/* Check device state for handling interrupt */
	if (lpfc_intr_state_check(phba))
		return IRQ_NONE;

	spin_lock(&phba->hbalock);
	if (lpfc_readl(phba->HAregaddr, &phba->ha_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_HANDLED;
	}

	if (unlikely(!phba->ha_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_NONE;
	} else if (phba->ha_copy & HA_ERATT) {
		if (phba->hba_flag & HBA_ERATT_HANDLED)
			/* ERATT polling has handled ERATT */
			phba->ha_copy &= ~HA_ERATT;
		else
			/* Indicate interrupt handler handles ERATT */
			phba->hba_flag |= HBA_ERATT_HANDLED;
	}

	/*
	 * If there is deferred error attention, do not check for any interrupt.
	 */
	if (unlikely(phba->hba_flag & DEFER_ERATT)) {
		spin_unlock(&phba->hbalock);
		return IRQ_NONE;
	}

	/* Clear attention sources except link and error attentions */
	if (lpfc_readl(phba->HCregaddr, &hc_copy)) {
		spin_unlock(&phba->hbalock);
		return IRQ_HANDLED;
	}
	writel(hc_copy & ~(HC_MBINT_ENA | HC_R0INT_ENA | HC_R1INT_ENA
		| HC_R2INT_ENA | HC_LAINT_ENA | HC_ERINT_ENA),
		phba->HCregaddr);
	writel((phba->ha_copy & ~(HA_LATT | HA_ERATT)), phba->HAregaddr);
	writel(hc_copy, phba->HCregaddr);
	readl(phba->HAregaddr); /* flush */
	spin_unlock(&phba->hbalock);

	/*
	 * Invokes slow-path host attention interrupt handling as appropriate.
	 */

	/* status of events with mailbox and link attention */
	status1 = phba->ha_copy & (HA_MBATT | HA_LATT | HA_ERATT);

	/* status of events with ELS ring */
	status2 = (phba->ha_copy & (HA_RXMASK  << (4*LPFC_ELS_RING)));
	status2 >>= (4*LPFC_ELS_RING);

	if (status1 || (status2 & HA_RXMASK))
		sp_irq_rc = lpfc_sli_sp_intr_handler(irq, dev_id);
	else
		sp_irq_rc = IRQ_NONE;

	/*
	 * Invoke fast-path host attention interrupt handling as appropriate.
	 */

	/* status of events with FCP ring */
	status1 = (phba->ha_copy & (HA_RXMASK << (4*LPFC_FCP_RING)));
	status1 >>= (4*LPFC_FCP_RING);

	/* status of events with extra ring */
	if (phba->cfg_multi_ring_support == 2) {
		status2 = (phba->ha_copy & (HA_RXMASK << (4*LPFC_EXTRA_RING)));
		status2 >>= (4*LPFC_EXTRA_RING);
	} else
		status2 = 0;

	if ((status1 & HA_RXMASK) || (status2 & HA_RXMASK))
		fp_irq_rc = lpfc_sli_fp_intr_handler(irq, dev_id);
	else
		fp_irq_rc = IRQ_NONE;

	/* Return device-level interrupt handling status */
	return (sp_irq_rc == IRQ_HANDLED) ? sp_irq_rc : fp_irq_rc;
}  /* lpfc_sli_intr_handler */

/**
 * lpfc_sli4_fcp_xri_abort_event_proc - Process fcp xri abort event
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked by the worker thread to process all the pending
 * SLI4 FCP abort XRI events.
 **/
void lpfc_sli4_fcp_xri_abort_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;

	/* First, declare the fcp xri abort event has been handled */
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~FCP_XRI_ABORT_EVENT;
	spin_unlock_irq(&phba->hbalock);
	/* Now, handle all the fcp xri abort events */
	while (!list_empty(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue)) {
		/* Get the first event from the head of the event queue */
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_fcp_xri_aborted_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);
		/* Notify aborted XRI for FCP work queue */
		lpfc_sli4_fcp_xri_aborted(phba, &cq_event->cqe.wcqe_axri);
		/* Free the event processed back to the free pool */
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

/**
 * lpfc_sli4_els_xri_abort_event_proc - Process els xri abort event
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked by the worker thread to process all the pending
 * SLI4 els abort xri events.
 **/
void lpfc_sli4_els_xri_abort_event_proc(struct lpfc_hba *phba)
{
	struct lpfc_cq_event *cq_event;

	/* First, declare the els xri abort event has been handled */
	spin_lock_irq(&phba->hbalock);
	phba->hba_flag &= ~ELS_XRI_ABORT_EVENT;
	spin_unlock_irq(&phba->hbalock);
	/* Now, handle all the els xri abort events */
	while (!list_empty(&phba->sli4_hba.sp_els_xri_aborted_work_queue)) {
		/* Get the first event from the head of the event queue */
		spin_lock_irq(&phba->hbalock);
		list_remove_head(&phba->sli4_hba.sp_els_xri_aborted_work_queue,
				 cq_event, struct lpfc_cq_event, list);
		spin_unlock_irq(&phba->hbalock);
		/* Notify aborted XRI for ELS work queue */
		lpfc_sli4_els_xri_aborted(phba, &cq_event->cqe.wcqe_axri);
		/* Free the event processed back to the free pool */
		lpfc_sli4_cq_event_release(phba, cq_event);
	}
}

/**
 * lpfc_sli4_iocb_param_transfer - Transfer pIocbOut and cmpl status to pIocbIn
 * @phba: pointer to lpfc hba data structure
 * @pIocbIn: pointer to the rspiocbq
 * @pIocbOut: pointer to the cmdiocbq
 * @wcqe: pointer to the complete wcqe
 *
 * This routine transfers the fields of a command iocbq to a response iocbq
 * by copying all the IOCB fields from command iocbq and transferring the
 * completion status information from the complete wcqe.
 **/
static void
lpfc_sli4_iocb_param_transfer(struct lpfc_hba *phba,
			      struct lpfc_iocbq *pIocbIn,
			      struct lpfc_iocbq *pIocbOut,
			      struct lpfc_wcqe_complete *wcqe)
{
	unsigned long iflags;
	uint32_t status;
	size_t offset = offsetof(struct lpfc_iocbq, iocb);

	memcpy((char *)pIocbIn + offset, (char *)pIocbOut + offset,
	       sizeof(struct lpfc_iocbq) - offset);
	/* Map WCQE parameters into irspiocb parameters */
	status = bf_get(lpfc_wcqe_c_status, wcqe);
	pIocbIn->iocb.ulpStatus = (status & LPFC_IOCB_STATUS_MASK);
	if (pIocbOut->iocb_flag & LPFC_IO_FCP)
		if (pIocbIn->iocb.ulpStatus == IOSTAT_FCP_RSP_ERROR)
			pIocbIn->iocb.un.fcpi.fcpi_parm =
					pIocbOut->iocb.un.fcpi.fcpi_parm -
					wcqe->total_data_placed;
		else
			pIocbIn->iocb.un.ulpWord[4] = wcqe->parameter;
	else {
		pIocbIn->iocb.un.ulpWord[4] = wcqe->parameter;
		pIocbIn->iocb.un.genreq64.bdl.bdeSize = wcqe->total_data_placed;
	}

	/* Convert BG errors for completion status */
	if (status == CQE_STATUS_DI_ERROR) {
		pIocbIn->iocb.ulpStatus = IOSTAT_LOCAL_REJECT;

		if (bf_get(lpfc_wcqe_c_bg_edir, wcqe))
			pIocbIn->iocb.un.ulpWord[4] = IOERR_RX_DMA_FAILED;
		else
			pIocbIn->iocb.un.ulpWord[4] = IOERR_TX_DMA_FAILED;

		pIocbIn->iocb.unsli3.sli3_bg.bgstat = 0;
		if (bf_get(lpfc_wcqe_c_bg_ge, wcqe)) /* Guard Check failed */
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_GUARD_ERR_MASK;
		if (bf_get(lpfc_wcqe_c_bg_ae, wcqe)) /* App Tag Check failed */
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_APPTAG_ERR_MASK;
		if (bf_get(lpfc_wcqe_c_bg_re, wcqe)) /* Ref Tag Check failed */
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_REFTAG_ERR_MASK;

		/* Check to see if there was any good data before the error */
		if (bf_get(lpfc_wcqe_c_bg_tdpv, wcqe)) {
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				BGS_HI_WATER_MARK_PRESENT_MASK;
			pIocbIn->iocb.unsli3.sli3_bg.bghm =
				wcqe->total_data_placed;
		}

		/*
		* Set ALL the error bits to indicate we don't know what
		* type of error it is.
		*/
		if (!pIocbIn->iocb.unsli3.sli3_bg.bgstat)
			pIocbIn->iocb.unsli3.sli3_bg.bgstat |=
				(BGS_REFTAG_ERR_MASK | BGS_APPTAG_ERR_MASK |
				BGS_GUARD_ERR_MASK);
	}

	/* Pick up HBA exchange busy condition */
	if (bf_get(lpfc_wcqe_c_xb, wcqe)) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		pIocbIn->iocb_flag |= LPFC_EXCHANGE_BUSY;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}
}

/**
 * lpfc_sli4_els_wcqe_to_rspiocbq - Get response iocbq from els wcqe
 * @phba: Pointer to HBA context object.
 * @wcqe: Pointer to work-queue completion queue entry.
 *
 * This routine handles an ELS work-queue completion event and construct
 * a pseudo response ELS IODBQ from the SLI4 ELS WCQE for the common
 * discovery engine to handle.
 *
 * Return: Pointer to the receive IOCBQ, NULL otherwise.
 **/
static struct lpfc_iocbq *
lpfc_sli4_els_wcqe_to_rspiocbq(struct lpfc_hba *phba,
			       struct lpfc_iocbq *irspiocbq)
{
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_wcqe_complete *wcqe;
	unsigned long iflags;

	wcqe = &irspiocbq->cq_event.cqe.wcqe_cmpl;
	spin_lock_irqsave(&pring->ring_lock, iflags);
	pring->stats.iocb_event++;
	/* Look up the ELS command IOCB and create pseudo response IOCB */
	cmdiocbq = lpfc_sli_iocbq_lookup_by_tag(phba, pring,
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	spin_unlock_irqrestore(&pring->ring_lock, iflags);

	if (unlikely(!cmdiocbq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0386 ELS complete with no corresponding "
				"cmdiocb: iotag (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		lpfc_sli_release_iocbq(phba, irspiocbq);
		return NULL;
	}

	/* Fake the irspiocbq and copy necessary response information */
	lpfc_sli4_iocb_param_transfer(phba, irspiocbq, cmdiocbq, wcqe);

	return irspiocbq;
}

/**
 * lpfc_sli4_sp_handle_async_event - Handle an asynchroous event
 * @phba: Pointer to HBA context object.
 * @cqe: Pointer to mailbox completion queue entry.
 *
 * This routine process a mailbox completion queue entry with asynchrous
 * event.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_async_event(struct lpfc_hba *phba, struct lpfc_mcqe *mcqe)
{
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	lpfc_printf_log(phba, KERN_INFO, LOG_SLI,
			"0392 Async Event: word0:x%x, word1:x%x, "
			"word2:x%x, word3:x%x\n", mcqe->word0,
			mcqe->mcqe_tag0, mcqe->mcqe_tag1, mcqe->trailer);

	/* Allocate a new internal CQ_EVENT entry */
	cq_event = lpfc_sli4_cq_event_alloc(phba);
	if (!cq_event) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0394 Failed to allocate CQ_EVENT entry\n");
		return false;
	}

	/* Move the CQE into an asynchronous event entry */
	memcpy(&cq_event->cqe, mcqe, sizeof(struct lpfc_mcqe));
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_add_tail(&cq_event->list, &phba->sli4_hba.sp_asynce_work_queue);
	/* Set the async event flag */
	phba->hba_flag |= ASYNC_EVENT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return true;
}

/**
 * lpfc_sli4_sp_handle_mbox_event - Handle a mailbox completion event
 * @phba: Pointer to HBA context object.
 * @cqe: Pointer to mailbox completion queue entry.
 *
 * This routine process a mailbox completion queue entry with mailbox
 * completion event.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_mbox_event(struct lpfc_hba *phba, struct lpfc_mcqe *mcqe)
{
	uint32_t mcqe_status;
	MAILBOX_t *mbox, *pmbox;
	struct lpfc_mqe *mqe;
	struct lpfc_vport *vport;
	struct lpfc_nodelist *ndlp;
	struct lpfc_dmabuf *mp;
	unsigned long iflags;
	LPFC_MBOXQ_t *pmb;
	bool workposted = false;
	int rc;

	/* If not a mailbox complete MCQE, out by checking mailbox consume */
	if (!bf_get(lpfc_trailer_completed, mcqe))
		goto out_no_mqe_complete;

	/* Get the reference to the active mbox command */
	spin_lock_irqsave(&phba->hbalock, iflags);
	pmb = phba->sli.mbox_active;
	if (unlikely(!pmb)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_MBOX,
				"1832 No pending MBOX command to handle\n");
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		goto out_no_mqe_complete;
	}
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	mqe = &pmb->u.mqe;
	pmbox = (MAILBOX_t *)&pmb->u.mqe;
	mbox = phba->mbox;
	vport = pmb->vport;

	/* Reset heartbeat timer */
	phba->last_completion_time = jiffies;
	del_timer(&phba->sli.mbox_tmo);

	/* Move mbox data to caller's mailbox region, do endian swapping */
	if (pmb->mbox_cmpl && mbox)
		lpfc_sli_pcimem_bcopy(mbox, mqe, sizeof(struct lpfc_mqe));

	/*
	 * For mcqe errors, conditionally move a modified error code to
	 * the mbox so that the error will not be missed.
	 */
	mcqe_status = bf_get(lpfc_mcqe_status, mcqe);
	if (mcqe_status != MB_CQE_STATUS_SUCCESS) {
		if (bf_get(lpfc_mqe_status, mqe) == MBX_SUCCESS)
			bf_set(lpfc_mqe_status, mqe,
			       (LPFC_MBX_ERROR_RANGE | mcqe_status));
	}
	if (pmb->mbox_flag & LPFC_MBX_IMED_UNREG) {
		pmb->mbox_flag &= ~LPFC_MBX_IMED_UNREG;
		lpfc_debugfs_disc_trc(vport, LPFC_DISC_TRC_MBOX_VPORT,
				      "MBOX dflt rpi: status:x%x rpi:x%x",
				      mcqe_status,
				      pmbox->un.varWords[0], 0);
		if (mcqe_status == MB_CQE_STATUS_SUCCESS) {
			mp = (struct lpfc_dmabuf *)(pmb->context1);
			ndlp = (struct lpfc_nodelist *)pmb->context2;
			/* Reg_LOGIN of dflt RPI was successful. Now lets get
			 * RID of the PPI using the same mbox buffer.
			 */
			lpfc_unreg_login(phba, vport->vpi,
					 pmbox->un.varWords[0], pmb);
			pmb->mbox_cmpl = lpfc_mbx_cmpl_dflt_rpi;
			pmb->context1 = mp;
			pmb->context2 = ndlp;
			pmb->vport = vport;
			rc = lpfc_sli_issue_mbox(phba, pmb, MBX_NOWAIT);
			if (rc != MBX_BUSY)
				lpfc_printf_log(phba, KERN_ERR, LOG_MBOX |
						LOG_SLI, "0385 rc should "
						"have been MBX_BUSY\n");
			if (rc != MBX_NOT_FINISHED)
				goto send_current_mbox;
		}
	}
	spin_lock_irqsave(&phba->pport->work_port_lock, iflags);
	phba->pport->work_port_events &= ~WORKER_MBOX_TMO;
	spin_unlock_irqrestore(&phba->pport->work_port_lock, iflags);

	/* There is mailbox completion work to do */
	spin_lock_irqsave(&phba->hbalock, iflags);
	__lpfc_mbox_cmpl_put(phba, pmb);
	phba->work_ha |= HA_MBATT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	workposted = true;

send_current_mbox:
	spin_lock_irqsave(&phba->hbalock, iflags);
	/* Release the mailbox command posting token */
	phba->sli.sli_flag &= ~LPFC_SLI_MBOX_ACTIVE;
	/* Setting active mailbox pointer need to be in sync to flag clear */
	phba->sli.mbox_active = NULL;
	spin_unlock_irqrestore(&phba->hbalock, iflags);
	/* Wake up worker thread to post the next pending mailbox command */
	lpfc_worker_wake_up(phba);
out_no_mqe_complete:
	if (bf_get(lpfc_trailer_consumed, mcqe))
		lpfc_sli4_mq_release(phba->sli4_hba.mbx_wq);
	return workposted;
}

/**
 * lpfc_sli4_sp_handle_mcqe - Process a mailbox completion queue entry
 * @phba: Pointer to HBA context object.
 * @cqe: Pointer to mailbox completion queue entry.
 *
 * This routine process a mailbox completion queue entry, it invokes the
 * proper mailbox complete handling or asynchrous event handling routine
 * according to the MCQE's async bit.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_mcqe(struct lpfc_hba *phba, struct lpfc_cqe *cqe)
{
	struct lpfc_mcqe mcqe;
	bool workposted;

	/* Copy the mailbox MCQE and convert endian order as needed */
	lpfc_sli_pcimem_bcopy(cqe, &mcqe, sizeof(struct lpfc_mcqe));

	/* Invoke the proper event handling routine */
	if (!bf_get(lpfc_trailer_async, &mcqe))
		workposted = lpfc_sli4_sp_handle_mbox_event(phba, &mcqe);
	else
		workposted = lpfc_sli4_sp_handle_async_event(phba, &mcqe);
	return workposted;
}

/**
 * lpfc_sli4_sp_handle_els_wcqe - Handle els work-queue completion event
 * @phba: Pointer to HBA context object.
 * @cq: Pointer to associated CQ
 * @wcqe: Pointer to work-queue completion queue entry.
 *
 * This routine handles an ELS work-queue completion event.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_els_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_complete *wcqe)
{
	struct lpfc_iocbq *irspiocbq;
	unsigned long iflags;
	struct lpfc_sli_ring *pring = cq->pring;
	int txq_cnt = 0;
	int txcmplq_cnt = 0;
	int fcp_txcmplq_cnt = 0;

	/* Get an irspiocbq for later ELS response processing use */
	irspiocbq = lpfc_sli_get_iocbq(phba);
	if (!irspiocbq) {
		if (!list_empty(&pring->txq))
			txq_cnt++;
		if (!list_empty(&pring->txcmplq))
			txcmplq_cnt++;
		if (!list_empty(&phba->sli.ring[LPFC_FCP_RING].txcmplq))
			fcp_txcmplq_cnt++;
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"0387 NO IOCBQ data: txq_cnt=%d iocb_cnt=%d "
			"fcp_txcmplq_cnt=%d, els_txcmplq_cnt=%d\n",
			txq_cnt, phba->iocb_cnt,
			fcp_txcmplq_cnt,
			txcmplq_cnt);
		return false;
	}

	/* Save off the slow-path queue event for work thread to process */
	memcpy(&irspiocbq->cq_event.cqe.wcqe_cmpl, wcqe, sizeof(*wcqe));
	spin_lock_irqsave(&phba->hbalock, iflags);
	list_add_tail(&irspiocbq->cq_event.list,
		      &phba->sli4_hba.sp_queue_event);
	phba->hba_flag |= HBA_SP_QUEUE_EVT;
	spin_unlock_irqrestore(&phba->hbalock, iflags);

	return true;
}

/**
 * lpfc_sli4_sp_handle_rel_wcqe - Handle slow-path WQ entry consumed event
 * @phba: Pointer to HBA context object.
 * @wcqe: Pointer to work-queue completion queue entry.
 *
 * This routine handles slow-path WQ entry comsumed event by invoking the
 * proper WQ release routine to the slow-path WQ.
 **/
static void
lpfc_sli4_sp_handle_rel_wcqe(struct lpfc_hba *phba,
			     struct lpfc_wcqe_release *wcqe)
{
	/* sanity check on queue memory */
	if (unlikely(!phba->sli4_hba.els_wq))
		return;
	/* Check for the slow-path ELS work queue */
	if (bf_get(lpfc_wcqe_r_wq_id, wcqe) == phba->sli4_hba.els_wq->queue_id)
		lpfc_sli4_wq_release(phba->sli4_hba.els_wq,
				     bf_get(lpfc_wcqe_r_wqe_index, wcqe));
	else
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2579 Slow-path wqe consume event carries "
				"miss-matched qid: wcqe-qid=x%x, sp-qid=x%x\n",
				bf_get(lpfc_wcqe_r_wqe_index, wcqe),
				phba->sli4_hba.els_wq->queue_id);
}

/**
 * lpfc_sli4_sp_handle_abort_xri_wcqe - Handle a xri abort event
 * @phba: Pointer to HBA context object.
 * @cq: Pointer to a WQ completion queue.
 * @wcqe: Pointer to work-queue completion queue entry.
 *
 * This routine handles an XRI abort event.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_abort_xri_wcqe(struct lpfc_hba *phba,
				   struct lpfc_queue *cq,
				   struct sli4_wcqe_xri_aborted *wcqe)
{
	bool workposted = false;
	struct lpfc_cq_event *cq_event;
	unsigned long iflags;

	/* Allocate a new internal CQ_EVENT entry */
	cq_event = lpfc_sli4_cq_event_alloc(phba);
	if (!cq_event) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0602 Failed to allocate CQ_EVENT entry\n");
		return false;
	}

	/* Move the CQE into the proper xri abort event list */
	memcpy(&cq_event->cqe, wcqe, sizeof(struct sli4_wcqe_xri_aborted));
	switch (cq->subtype) {
	case LPFC_FCP:
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_fcp_xri_aborted_work_queue);
		/* Set the fcp xri abort event flag */
		phba->hba_flag |= FCP_XRI_ABORT_EVENT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	case LPFC_ELS:
		spin_lock_irqsave(&phba->hbalock, iflags);
		list_add_tail(&cq_event->list,
			      &phba->sli4_hba.sp_els_xri_aborted_work_queue);
		/* Set the els xri abort event flag */
		phba->hba_flag |= ELS_XRI_ABORT_EVENT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0603 Invalid work queue CQE subtype (x%x)\n",
				cq->subtype);
		workposted = false;
		break;
	}
	return workposted;
}

/**
 * lpfc_sli4_sp_handle_rcqe - Process a receive-queue completion queue entry
 * @phba: Pointer to HBA context object.
 * @rcqe: Pointer to receive-queue completion queue entry.
 *
 * This routine process a receive-queue completion queue entry.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_rcqe(struct lpfc_hba *phba, struct lpfc_rcqe *rcqe)
{
	bool workposted = false;
	struct lpfc_queue *hrq = phba->sli4_hba.hdr_rq;
	struct lpfc_queue *drq = phba->sli4_hba.dat_rq;
	struct hbq_dmabuf *dma_buf;
	uint32_t status, rq_id;
	unsigned long iflags;

	/* sanity check on queue memory */
	if (unlikely(!hrq) || unlikely(!drq))
		return workposted;

	if (bf_get(lpfc_cqe_code, rcqe) == CQE_CODE_RECEIVE_V1)
		rq_id = bf_get(lpfc_rcqe_rq_id_v1, rcqe);
	else
		rq_id = bf_get(lpfc_rcqe_rq_id, rcqe);
	if (rq_id != hrq->queue_id)
		goto out;

	status = bf_get(lpfc_rcqe_status, rcqe);
	switch (status) {
	case FC_STATUS_RQ_BUF_LEN_EXCEEDED:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2537 Receive Frame Truncated!!\n");
		hrq->RQ_buf_trunc++;
	case FC_STATUS_RQ_SUCCESS:
		lpfc_sli4_rq_release(hrq, drq);
		spin_lock_irqsave(&phba->hbalock, iflags);
		dma_buf = lpfc_sli_hbqbuf_get(&phba->hbqs[0].hbq_buffer_list);
		if (!dma_buf) {
			hrq->RQ_no_buf_found++;
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			goto out;
		}
		hrq->RQ_rcv_buf++;
		memcpy(&dma_buf->cq_event.cqe.rcqe_cmpl, rcqe, sizeof(*rcqe));
		/* save off the frame for the word thread to process */
		list_add_tail(&dma_buf->cq_event.list,
			      &phba->sli4_hba.sp_queue_event);
		/* Frame received */
		phba->hba_flag |= HBA_SP_QUEUE_EVT;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	case FC_STATUS_INSUFF_BUF_NEED_BUF:
	case FC_STATUS_INSUFF_BUF_FRM_DISC:
		hrq->RQ_no_posted_buf++;
		/* Post more buffers if possible */
		spin_lock_irqsave(&phba->hbalock, iflags);
		phba->hba_flag |= HBA_POST_RECEIVE_BUFFER;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
		workposted = true;
		break;
	}
out:
	return workposted;
}

/**
 * lpfc_sli4_sp_handle_cqe - Process a slow path completion queue entry
 * @phba: Pointer to HBA context object.
 * @cq: Pointer to the completion queue.
 * @wcqe: Pointer to a completion queue entry.
 *
 * This routine process a slow-path work-queue or receive queue completion queue
 * entry.
 *
 * Return: true if work posted to worker thread, otherwise false.
 **/
static bool
lpfc_sli4_sp_handle_cqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_cqe cqevt;
	bool workposted = false;

	/* Copy the work queue CQE and convert endian order if needed */
	lpfc_sli_pcimem_bcopy(cqe, &cqevt, sizeof(struct lpfc_cqe));

	/* Check and process for different type of WCQE and dispatch */
	switch (bf_get(lpfc_cqe_code, &cqevt)) {
	case CQE_CODE_COMPL_WQE:
		/* Process the WQ/RQ complete event */
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_els_wcqe(phba, cq,
				(struct lpfc_wcqe_complete *)&cqevt);
		break;
	case CQE_CODE_RELEASE_WQE:
		/* Process the WQ release event */
		lpfc_sli4_sp_handle_rel_wcqe(phba,
				(struct lpfc_wcqe_release *)&cqevt);
		break;
	case CQE_CODE_XRI_ABORTED:
		/* Process the WQ XRI abort event */
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_abort_xri_wcqe(phba, cq,
				(struct sli4_wcqe_xri_aborted *)&cqevt);
		break;
	case CQE_CODE_RECEIVE:
	case CQE_CODE_RECEIVE_V1:
		/* Process the RQ event */
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_rcqe(phba,
				(struct lpfc_rcqe *)&cqevt);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0388 Not a valid WCQE code: x%x\n",
				bf_get(lpfc_cqe_code, &cqevt));
		break;
	}
	return workposted;
}

/**
 * lpfc_sli4_sp_handle_eqe - Process a slow-path event queue entry
 * @phba: Pointer to HBA context object.
 * @eqe: Pointer to fast-path event queue entry.
 *
 * This routine process a event queue entry from the slow-path event queue.
 * It will check the MajorCode and MinorCode to determine this is for a
 * completion event on a completion queue, if not, an error shall be logged
 * and just return. Otherwise, it will get to the corresponding completion
 * queue and process all the entries on that completion queue, rearm the
 * completion queue, and then return.
 *
 **/
static void
lpfc_sli4_sp_handle_eqe(struct lpfc_hba *phba, struct lpfc_eqe *eqe,
	struct lpfc_queue *speq)
{
	struct lpfc_queue *cq = NULL, *childq;
	struct lpfc_cqe *cqe;
	bool workposted = false;
	int ecount = 0;
	uint16_t cqid;

	/* Get the reference to the corresponding CQ */
	cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);

	list_for_each_entry(childq, &speq->child_list, list) {
		if (childq->queue_id == cqid) {
			cq = childq;
			break;
		}
	}
	if (unlikely(!cq)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE)
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0365 Slow-path CQ identifier "
					"(%d) does not exist\n", cqid);
		return;
	}

	/* Process all the entries to the CQ */
	switch (cq->type) {
	case LPFC_MCQ:
		while ((cqe = lpfc_sli4_cq_get(cq))) {
			workposted |= lpfc_sli4_sp_handle_mcqe(phba, cqe);
			if (!(++ecount % cq->entry_repost))
				lpfc_sli4_cq_release(cq, LPFC_QUEUE_NOARM);
			cq->CQ_mbox++;
		}
		break;
	case LPFC_WCQ:
		while ((cqe = lpfc_sli4_cq_get(cq))) {
			if (cq->subtype == LPFC_FCP)
				workposted |= lpfc_sli4_fp_handle_wcqe(phba, cq,
								       cqe);
			else
				workposted |= lpfc_sli4_sp_handle_cqe(phba, cq,
								      cqe);
			if (!(++ecount % cq->entry_repost))
				lpfc_sli4_cq_release(cq, LPFC_QUEUE_NOARM);
		}

		/* Track the max number of CQEs processed in 1 EQ */
		if (ecount > cq->CQ_max_cqe)
			cq->CQ_max_cqe = ecount;
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0370 Invalid completion queue type (%d)\n",
				cq->type);
		return;
	}

	/* Catch the no cq entry condition, log an error */
	if (unlikely(ecount == 0))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0371 No entry from the CQ: identifier "
				"(x%x), type (%d)\n", cq->queue_id, cq->type);

	/* In any case, flash and re-arm the RCQ */
	lpfc_sli4_cq_release(cq, LPFC_QUEUE_REARM);

	/* wake up worker thread if there are works to be done */
	if (workposted)
		lpfc_worker_wake_up(phba);
}

/**
 * lpfc_sli4_fp_handle_fcp_wcqe - Process fast-path work queue completion entry
 * @phba: Pointer to HBA context object.
 * @cq: Pointer to associated CQ
 * @wcqe: Pointer to work-queue completion queue entry.
 *
 * This routine process a fast-path work queue completion entry from fast-path
 * event queue for FCP command response completion.
 **/
static void
lpfc_sli4_fp_handle_fcp_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_complete *wcqe)
{
	struct lpfc_sli_ring *pring = cq->pring;
	struct lpfc_iocbq *cmdiocbq;
	struct lpfc_iocbq irspiocbq;
	unsigned long iflags;

	/* Check for response status */
	if (unlikely(bf_get(lpfc_wcqe_c_status, wcqe))) {
		/* If resource errors reported from HBA, reduce queue
		 * depth of the SCSI device.
		 */
		if (((bf_get(lpfc_wcqe_c_status, wcqe) ==
		     IOSTAT_LOCAL_REJECT)) &&
		    ((wcqe->parameter & IOERR_PARAM_MASK) ==
		     IOERR_NO_RESOURCES))
			phba->lpfc_rampdown_queue_depth(phba);

		/* Log the error status */
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0373 FCP complete error: status=x%x, "
				"hw_status=x%x, total_data_specified=%d, "
				"parameter=x%x, word3=x%x\n",
				bf_get(lpfc_wcqe_c_status, wcqe),
				bf_get(lpfc_wcqe_c_hw_status, wcqe),
				wcqe->total_data_placed, wcqe->parameter,
				wcqe->word3);
	}

	/* Look up the FCP command IOCB and create pseudo response IOCB */
	spin_lock_irqsave(&pring->ring_lock, iflags);
	pring->stats.iocb_event++;
	cmdiocbq = lpfc_sli_iocbq_lookup_by_tag(phba, pring,
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
	spin_unlock_irqrestore(&pring->ring_lock, iflags);
	if (unlikely(!cmdiocbq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0374 FCP complete with no corresponding "
				"cmdiocb: iotag (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		return;
	}
	if (unlikely(!cmdiocbq->iocb_cmpl)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"0375 FCP cmdiocb not callback function "
				"iotag: (%d)\n",
				bf_get(lpfc_wcqe_c_request_tag, wcqe));
		return;
	}

	/* Fake the irspiocb and copy necessary response information */
	lpfc_sli4_iocb_param_transfer(phba, &irspiocbq, cmdiocbq, wcqe);

	if (cmdiocbq->iocb_flag & LPFC_DRIVER_ABORTED) {
		spin_lock_irqsave(&phba->hbalock, iflags);
		cmdiocbq->iocb_flag &= ~LPFC_DRIVER_ABORTED;
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	/* Pass the cmd_iocb and the rsp state to the upper layer */
	(cmdiocbq->iocb_cmpl)(phba, cmdiocbq, &irspiocbq);
}

/**
 * lpfc_sli4_fp_handle_rel_wcqe - Handle fast-path WQ entry consumed event
 * @phba: Pointer to HBA context object.
 * @cq: Pointer to completion queue.
 * @wcqe: Pointer to work-queue completion queue entry.
 *
 * This routine handles an fast-path WQ entry comsumed event by invoking the
 * proper WQ release routine to the slow-path WQ.
 **/
static void
lpfc_sli4_fp_handle_rel_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			     struct lpfc_wcqe_release *wcqe)
{
	struct lpfc_queue *childwq;
	bool wqid_matched = false;
	uint16_t fcp_wqid;

	/* Check for fast-path FCP work queue release */
	fcp_wqid = bf_get(lpfc_wcqe_r_wq_id, wcqe);
	list_for_each_entry(childwq, &cq->child_list, list) {
		if (childwq->queue_id == fcp_wqid) {
			lpfc_sli4_wq_release(childwq,
					bf_get(lpfc_wcqe_r_wqe_index, wcqe));
			wqid_matched = true;
			break;
		}
	}
	/* Report warning log message if no match found */
	if (wqid_matched != true)
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2580 Fast-path wqe consume event carries "
				"miss-matched qid: wcqe-qid=x%x\n", fcp_wqid);
}

/**
 * lpfc_sli4_fp_handle_wcqe - Process fast-path work queue completion entry
 * @cq: Pointer to the completion queue.
 * @eqe: Pointer to fast-path completion queue entry.
 *
 * This routine process a fast-path work queue completion entry from fast-path
 * event queue for FCP command response completion.
 **/
static int
lpfc_sli4_fp_handle_wcqe(struct lpfc_hba *phba, struct lpfc_queue *cq,
			 struct lpfc_cqe *cqe)
{
	struct lpfc_wcqe_release wcqe;
	bool workposted = false;

	/* Copy the work queue CQE and convert endian order if needed */
	lpfc_sli_pcimem_bcopy(cqe, &wcqe, sizeof(struct lpfc_cqe));

	/* Check and process for different type of WCQE and dispatch */
	switch (bf_get(lpfc_wcqe_c_code, &wcqe)) {
	case CQE_CODE_COMPL_WQE:
		cq->CQ_wq++;
		/* Process the WQ complete event */
		phba->last_completion_time = jiffies;
		lpfc_sli4_fp_handle_fcp_wcqe(phba, cq,
				(struct lpfc_wcqe_complete *)&wcqe);
		break;
	case CQE_CODE_RELEASE_WQE:
		cq->CQ_release_wqe++;
		/* Process the WQ release event */
		lpfc_sli4_fp_handle_rel_wcqe(phba, cq,
				(struct lpfc_wcqe_release *)&wcqe);
		break;
	case CQE_CODE_XRI_ABORTED:
		cq->CQ_xri_aborted++;
		/* Process the WQ XRI abort event */
		phba->last_completion_time = jiffies;
		workposted = lpfc_sli4_sp_handle_abort_xri_wcqe(phba, cq,
				(struct sli4_wcqe_xri_aborted *)&wcqe);
		break;
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0144 Not a valid WCQE code: x%x\n",
				bf_get(lpfc_wcqe_c_code, &wcqe));
		break;
	}
	return workposted;
}

/**
 * lpfc_sli4_hba_handle_eqe - Process a fast-path event queue entry
 * @phba: Pointer to HBA context object.
 * @eqe: Pointer to fast-path event queue entry.
 *
 * This routine process a event queue entry from the fast-path event queue.
 * It will check the MajorCode and MinorCode to determine this is for a
 * completion event on a completion queue, if not, an error shall be logged
 * and just return. Otherwise, it will get to the corresponding completion
 * queue and process all the entries on the completion queue, rearm the
 * completion queue, and then return.
 **/
static void
lpfc_sli4_hba_handle_eqe(struct lpfc_hba *phba, struct lpfc_eqe *eqe,
			uint32_t qidx)
{
	struct lpfc_queue *cq;
	struct lpfc_cqe *cqe;
	bool workposted = false;
	uint16_t cqid;
	int ecount = 0;

	if (unlikely(bf_get_le32(lpfc_eqe_major_code, eqe) != 0)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0366 Not a valid completion "
				"event: majorcode=x%x, minorcode=x%x\n",
				bf_get_le32(lpfc_eqe_major_code, eqe),
				bf_get_le32(lpfc_eqe_minor_code, eqe));
		return;
	}

	/* Get the reference to the corresponding CQ */
	cqid = bf_get_le32(lpfc_eqe_resource_id, eqe);

	/* Check if this is a Slow path event */
	if (unlikely(cqid != phba->sli4_hba.fcp_cq_map[qidx])) {
		lpfc_sli4_sp_handle_eqe(phba, eqe,
			phba->sli4_hba.hba_eq[qidx]);
		return;
	}

	if (unlikely(!phba->sli4_hba.fcp_cq)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"3146 Fast-path completion queues "
				"does not exist\n");
		return;
	}
	cq = phba->sli4_hba.fcp_cq[qidx];
	if (unlikely(!cq)) {
		if (phba->sli.sli_flag & LPFC_SLI_ACTIVE)
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"0367 Fast-path completion queue "
					"(%d) does not exist\n", qidx);
		return;
	}

	if (unlikely(cqid != cq->queue_id)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0368 Miss-matched fast-path completion "
				"queue identifier: eqcqid=%d, fcpcqid=%d\n",
				cqid, cq->queue_id);
		return;
	}

	/* Process all the entries to the CQ */
	while ((cqe = lpfc_sli4_cq_get(cq))) {
		workposted |= lpfc_sli4_fp_handle_wcqe(phba, cq, cqe);
		if (!(++ecount % cq->entry_repost))
			lpfc_sli4_cq_release(cq, LPFC_QUEUE_NOARM);
	}

	/* Track the max number of CQEs processed in 1 EQ */
	if (ecount > cq->CQ_max_cqe)
		cq->CQ_max_cqe = ecount;

	/* Catch the no cq entry condition */
	if (unlikely(ecount == 0))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0369 No entry from fast-path completion "
				"queue fcpcqid=%d\n", cq->queue_id);

	/* In any case, flash and re-arm the CQ */
	lpfc_sli4_cq_release(cq, LPFC_QUEUE_REARM);

	/* wake up worker thread if there are works to be done */
	if (workposted)
		lpfc_worker_wake_up(phba);
}

static void
lpfc_sli4_eq_flush(struct lpfc_hba *phba, struct lpfc_queue *eq)
{
	struct lpfc_eqe *eqe;

	/* walk all the EQ entries and drop on the floor */
	while ((eqe = lpfc_sli4_eq_get(eq)))
		;

	/* Clear and re-arm the EQ */
	lpfc_sli4_eq_release(eq, LPFC_QUEUE_REARM);
}

/**
 * lpfc_sli4_hba_intr_handler - HBA interrupt handler to SLI-4 device
 * @irq: Interrupt number.
 * @dev_id: The device context pointer.
 *
 * This function is directly called from the PCI layer as an interrupt
 * service routine when device with SLI-4 interface spec is enabled with
 * MSI-X multi-message interrupt mode and there is a fast-path FCP IOCB
 * ring event in the HBA. However, when the device is enabled with either
 * MSI or Pin-IRQ interrupt mode, this function is called as part of the
 * device-level interrupt handler. When the PCI slot is in error recovery
 * or the HBA is undergoing initialization, the interrupt handler will not
 * process the interrupt. The SCSI FCP fast-path ring event are handled in
 * the intrrupt context. This function is called without any lock held.
 * It gets the hbalock to access and update SLI data structures. Note that,
 * the FCP EQ to FCP CQ are one-to-one map such that the FCP EQ index is
 * equal to that of FCP CQ index.
 *
 * The link attention and ELS ring attention events are handled
 * by the worker thread. The interrupt handler signals the worker thread
 * and returns for these events. This function is called without any lock
 * held. It gets the hbalock to access and update SLI data structures.
 *
 * This function returns IRQ_HANDLED when interrupt is handled else it
 * returns IRQ_NONE.
 **/
irqreturn_t
lpfc_sli4_hba_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba *phba;
	struct lpfc_fcp_eq_hdl *fcp_eq_hdl;
	struct lpfc_queue *fpeq;
	struct lpfc_eqe *eqe;
	unsigned long iflag;
	int ecount = 0;
	int fcp_eqidx;

	/* Get the driver's phba structure from the dev_id */
	fcp_eq_hdl = (struct lpfc_fcp_eq_hdl *)dev_id;
	phba = fcp_eq_hdl->phba;
	fcp_eqidx = fcp_eq_hdl->idx;

	if (unlikely(!phba))
		return IRQ_NONE;
	if (unlikely(!phba->sli4_hba.hba_eq))
		return IRQ_NONE;

	/* Get to the EQ struct associated with this vector */
	fpeq = phba->sli4_hba.hba_eq[fcp_eqidx];
	if (unlikely(!fpeq))
		return IRQ_NONE;

	if (lpfc_fcp_look_ahead) {
		if (atomic_dec_and_test(&fcp_eq_hdl->fcp_eq_in_use))
			lpfc_sli4_eq_clr_intr(fpeq);
		else {
			atomic_inc(&fcp_eq_hdl->fcp_eq_in_use);
			return IRQ_NONE;
		}
	}

	/* Check device state for handling interrupt */
	if (unlikely(lpfc_intr_state_check(phba))) {
		fpeq->EQ_badstate++;
		/* Check again for link_state with lock held */
		spin_lock_irqsave(&phba->hbalock, iflag);
		if (phba->link_state < LPFC_LINK_DOWN)
			/* Flush, clear interrupt, and rearm the EQ */
			lpfc_sli4_eq_flush(phba, fpeq);
		spin_unlock_irqrestore(&phba->hbalock, iflag);
		if (lpfc_fcp_look_ahead)
			atomic_inc(&fcp_eq_hdl->fcp_eq_in_use);
		return IRQ_NONE;
	}

	/*
	 * Process all the event on FCP fast-path EQ
	 */
	while ((eqe = lpfc_sli4_eq_get(fpeq))) {
		lpfc_sli4_hba_handle_eqe(phba, eqe, fcp_eqidx);
		if (!(++ecount % fpeq->entry_repost))
			lpfc_sli4_eq_release(fpeq, LPFC_QUEUE_NOARM);
		fpeq->EQ_processed++;
	}

	/* Track the max number of EQEs processed in 1 intr */
	if (ecount > fpeq->EQ_max_eqe)
		fpeq->EQ_max_eqe = ecount;

	/* Always clear and re-arm the fast-path EQ */
	lpfc_sli4_eq_release(fpeq, LPFC_QUEUE_REARM);

	if (unlikely(ecount == 0)) {
		fpeq->EQ_no_entry++;

		if (lpfc_fcp_look_ahead) {
			atomic_inc(&fcp_eq_hdl->fcp_eq_in_use);
			return IRQ_NONE;
		}

		if (phba->intr_type == MSIX)
			/* MSI-X treated interrupt served as no EQ share INT */
			lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
					"0358 MSI-X interrupt with no EQE\n");
		else
			/* Non MSI-X treated on interrupt as EQ share INT */
			return IRQ_NONE;
	}

	if (lpfc_fcp_look_ahead)
		atomic_inc(&fcp_eq_hdl->fcp_eq_in_use);
	return IRQ_HANDLED;
} /* lpfc_sli4_fp_intr_handler */

/**
 * lpfc_sli4_intr_handler - Device-level interrupt handler for SLI-4 device
 * @irq: Interrupt number.
 * @dev_id: The device context pointer.
 *
 * This function is the device-level interrupt handler to device with SLI-4
 * interface spec, called from the PCI layer when either MSI or Pin-IRQ
 * interrupt mode is enabled and there is an event in the HBA which requires
 * driver attention. This function invokes the slow-path interrupt attention
 * handling function and fast-path interrupt attention handling function in
 * turn to process the relevant HBA attention events. This function is called
 * without any lock held. It gets the hbalock to access and update SLI data
 * structures.
 *
 * This function returns IRQ_HANDLED when interrupt is handled, else it
 * returns IRQ_NONE.
 **/
irqreturn_t
lpfc_sli4_intr_handler(int irq, void *dev_id)
{
	struct lpfc_hba  *phba;
	irqreturn_t hba_irq_rc;
	bool hba_handled = false;
	int fcp_eqidx;

	/* Get the driver's phba structure from the dev_id */
	phba = (struct lpfc_hba *)dev_id;

	if (unlikely(!phba))
		return IRQ_NONE;

	/*
	 * Invoke fast-path host attention interrupt handling as appropriate.
	 */
	for (fcp_eqidx = 0; fcp_eqidx < phba->cfg_fcp_io_channel; fcp_eqidx++) {
		hba_irq_rc = lpfc_sli4_hba_intr_handler(irq,
					&phba->sli4_hba.fcp_eq_hdl[fcp_eqidx]);
		if (hba_irq_rc == IRQ_HANDLED)
			hba_handled |= true;
	}

	return (hba_handled == true) ? IRQ_HANDLED : IRQ_NONE;
} /* lpfc_sli4_intr_handler */

/**
 * lpfc_sli4_queue_free - free a queue structure and associated memory
 * @queue: The queue structure to free.
 *
 * This function frees a queue structure and the DMAable memory used for
 * the host resident queue. This function must be called after destroying the
 * queue on the HBA.
 **/
void
lpfc_sli4_queue_free(struct lpfc_queue *queue)
{
	struct lpfc_dmabuf *dmabuf;

	if (!queue)
		return;

	while (!list_empty(&queue->page_list)) {
		list_remove_head(&queue->page_list, dmabuf, struct lpfc_dmabuf,
				 list);
		dma_free_coherent(&queue->phba->pcidev->dev, SLI4_PAGE_SIZE,
				  dmabuf->virt, dmabuf->phys);
		kfree(dmabuf);
	}
	kfree(queue);
	return;
}

/**
 * lpfc_sli4_queue_alloc - Allocate and initialize a queue structure
 * @phba: The HBA that this queue is being created on.
 * @entry_size: The size of each queue entry for this queue.
 * @entry count: The number of entries that this queue will handle.
 *
 * This function allocates a queue structure and the DMAable memory used for
 * the host resident queue. This function must be called before creating the
 * queue on the HBA.
 **/
struct lpfc_queue *
lpfc_sli4_queue_alloc(struct lpfc_hba *phba, uint32_t entry_size,
		      uint32_t entry_count)
{
	struct lpfc_queue *queue;
	struct lpfc_dmabuf *dmabuf;
	int x, total_qe_count;
	void *dma_pointer;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	queue = kzalloc(sizeof(struct lpfc_queue) +
			(sizeof(union sli4_qe) * entry_count), GFP_KERNEL);
	if (!queue)
		return NULL;
	queue->page_count = (ALIGN(entry_size * entry_count,
			hw_page_size))/hw_page_size;
	INIT_LIST_HEAD(&queue->list);
	INIT_LIST_HEAD(&queue->page_list);
	INIT_LIST_HEAD(&queue->child_list);
	for (x = 0, total_qe_count = 0; x < queue->page_count; x++) {
		dmabuf = kzalloc(sizeof(struct lpfc_dmabuf), GFP_KERNEL);
		if (!dmabuf)
			goto out_fail;
		dmabuf->virt = dma_alloc_coherent(&phba->pcidev->dev,
						  hw_page_size, &dmabuf->phys,
						  GFP_KERNEL);
		if (!dmabuf->virt) {
			kfree(dmabuf);
			goto out_fail;
		}
		memset(dmabuf->virt, 0, hw_page_size);
		dmabuf->buffer_tag = x;
		list_add_tail(&dmabuf->list, &queue->page_list);
		/* initialize queue's entry array */
		dma_pointer = dmabuf->virt;
		for (; total_qe_count < entry_count &&
		     dma_pointer < (hw_page_size + dmabuf->virt);
		     total_qe_count++, dma_pointer += entry_size) {
			queue->qe[total_qe_count].address = dma_pointer;
		}
	}
	queue->entry_size = entry_size;
	queue->entry_count = entry_count;

	/*
	 * entry_repost is calculated based on the number of entries in the
	 * queue. This works out except for RQs. If buffers are NOT initially
	 * posted for every RQE, entry_repost should be adjusted accordingly.
	 */
	queue->entry_repost = (entry_count >> 3);
	if (queue->entry_repost < LPFC_QUEUE_MIN_REPOST)
		queue->entry_repost = LPFC_QUEUE_MIN_REPOST;
	queue->phba = phba;

	return queue;
out_fail:
	lpfc_sli4_queue_free(queue);
	return NULL;
}

/**
 * lpfc_dual_chute_pci_bar_map - Map pci base address register to host memory
 * @phba: HBA structure that indicates port to create a queue on.
 * @pci_barset: PCI BAR set flag.
 *
 * This function shall perform iomap of the specified PCI BAR address to host
 * memory address if not already done so and return it. The returned host
 * memory address can be NULL.
 */
static void __iomem *
lpfc_dual_chute_pci_bar_map(struct lpfc_hba *phba, uint16_t pci_barset)
{
	struct pci_dev *pdev;
	unsigned long bar_map, bar_map_len;

	if (!phba->pcidev)
		return NULL;
	else
		pdev = phba->pcidev;

	switch (pci_barset) {
	case WQ_PCI_BAR_0_AND_1:
		if (!phba->pci_bar0_memmap_p) {
			bar_map = pci_resource_start(pdev, PCI_64BIT_BAR0);
			bar_map_len = pci_resource_len(pdev, PCI_64BIT_BAR0);
			phba->pci_bar0_memmap_p = ioremap(bar_map, bar_map_len);
		}
		return phba->pci_bar0_memmap_p;
	case WQ_PCI_BAR_2_AND_3:
		if (!phba->pci_bar2_memmap_p) {
			bar_map = pci_resource_start(pdev, PCI_64BIT_BAR2);
			bar_map_len = pci_resource_len(pdev, PCI_64BIT_BAR2);
			phba->pci_bar2_memmap_p = ioremap(bar_map, bar_map_len);
		}
		return phba->pci_bar2_memmap_p;
	case WQ_PCI_BAR_4_AND_5:
		if (!phba->pci_bar4_memmap_p) {
			bar_map = pci_resource_start(pdev, PCI_64BIT_BAR4);
			bar_map_len = pci_resource_len(pdev, PCI_64BIT_BAR4);
			phba->pci_bar4_memmap_p = ioremap(bar_map, bar_map_len);
		}
		return phba->pci_bar4_memmap_p;
	default:
		break;
	}
	return NULL;
}

/**
 * lpfc_modify_fcp_eq_delay - Modify Delay Multiplier on FCP EQs
 * @phba: HBA structure that indicates port to create a queue on.
 * @startq: The starting FCP EQ to modify
 *
 * This function sends an MODIFY_EQ_DELAY mailbox command to the HBA.
 *
 * The @phba struct is used to send mailbox command to HBA. The @startq
 * is used to get the starting FCP EQ to change.
 * This function is asynchronous and will wait for the mailbox
 * command to finish before continuing.
 *
 * On success this function will return a zero. If unable to allocate enough
 * memory this function will return -ENOMEM. If the queue create mailbox command
 * fails this function will return -ENXIO.
 **/
uint32_t
lpfc_modify_fcp_eq_delay(struct lpfc_hba *phba, uint16_t startq)
{
	struct lpfc_mbx_modify_eq_delay *eq_delay;
	LPFC_MBOXQ_t *mbox;
	struct lpfc_queue *eq;
	int cnt, rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	uint32_t result;
	int fcp_eqidx;
	union lpfc_sli4_cfg_shdr *shdr;
	uint16_t dmult;

	if (startq >= phba->cfg_fcp_io_channel)
		return 0;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_modify_eq_delay) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MODIFY_EQ_DELAY,
			 length, LPFC_SLI4_MBX_EMBED);
	eq_delay = &mbox->u.mqe.un.eq_delay;

	/* Calculate delay multiper from maximum interrupt per second */
	result = phba->cfg_fcp_imax / phba->cfg_fcp_io_channel;
	if (result > LPFC_DMULT_CONST)
		dmult = 0;
	else
		dmult = LPFC_DMULT_CONST/result - 1;

	cnt = 0;
	for (fcp_eqidx = startq; fcp_eqidx < phba->cfg_fcp_io_channel;
	    fcp_eqidx++) {
		eq = phba->sli4_hba.hba_eq[fcp_eqidx];
		if (!eq)
			continue;
		eq_delay->u.request.eq[cnt].eq_id = eq->queue_id;
		eq_delay->u.request.eq[cnt].phase = 0;
		eq_delay->u.request.eq[cnt].delay_multi = dmult;
		cnt++;
		if (cnt >= LPFC_MAX_EQ_DELAY)
			break;
	}
	eq_delay->u.request.num_eq = cnt;

	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->context1 = NULL;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &eq_delay->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2512 MODIFY_EQ_DELAY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_eq_create - Create an Event Queue on the HBA
 * @phba: HBA structure that indicates port to create a queue on.
 * @eq: The queue structure to use to create the event queue.
 * @imax: The maximum interrupt per second limit.
 *
 * This function creates an event queue, as detailed in @eq, on a port,
 * described by @phba by sending an EQ_CREATE mailbox command to the HBA.
 *
 * The @phba struct is used to send mailbox command to HBA. The @eq struct
 * is used to get the entry count and entry size that are necessary to
 * determine the number of pages to allocate and use for this queue. This
 * function will send the EQ_CREATE mailbox command to the HBA to setup the
 * event queue. This function is asynchronous and will wait for the mailbox
 * command to finish before continuing.
 *
 * On success this function will return a zero. If unable to allocate enough
 * memory this function will return -ENOMEM. If the queue create mailbox command
 * fails this function will return -ENXIO.
 **/
uint32_t
lpfc_eq_create(struct lpfc_hba *phba, struct lpfc_queue *eq, uint32_t imax)
{
	struct lpfc_mbx_eq_create *eq_create;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	struct lpfc_dmabuf *dmabuf;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint16_t dmult;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	/* sanity check on queue memory */
	if (!eq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_eq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_EQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	eq_create = &mbox->u.mqe.un.eq_create;
	bf_set(lpfc_mbx_eq_create_num_pages, &eq_create->u.request,
	       eq->page_count);
	bf_set(lpfc_eq_context_size, &eq_create->u.request.context,
	       LPFC_EQE_SIZE);
	bf_set(lpfc_eq_context_valid, &eq_create->u.request.context, 1);
	/* Calculate delay multiper from maximum interrupt per second */
	if (imax > LPFC_DMULT_CONST)
		dmult = 0;
	else
		dmult = LPFC_DMULT_CONST/imax - 1;
	bf_set(lpfc_eq_context_delay_multi, &eq_create->u.request.context,
	       dmult);
	switch (eq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0360 Unsupported EQ count. (%d)\n",
				eq->entry_count);
		if (eq->entry_count < 256)
			return -EINVAL;
		/* otherwise default to smallest count (drop through) */
	case 256:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_256);
		break;
	case 512:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_512);
		break;
	case 1024:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_1024);
		break;
	case 2048:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_2048);
		break;
	case 4096:
		bf_set(lpfc_eq_context_count, &eq_create->u.request.context,
		       LPFC_EQ_CNT_4096);
		break;
	}
	list_for_each_entry(dmabuf, &eq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		eq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		eq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mbox->context1 = NULL;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &eq_create->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2500 EQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	eq->type = LPFC_EQ;
	eq->subtype = LPFC_NONE;
	eq->queue_id = bf_get(lpfc_mbx_eq_create_q_id, &eq_create->u.response);
	if (eq->queue_id == 0xFFFF)
		status = -ENXIO;
	eq->host_index = 0;
	eq->hba_index = 0;

	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_cq_create - Create a Completion Queue on the HBA
 * @phba: HBA structure that indicates port to create a queue on.
 * @cq: The queue structure to use to create the completion queue.
 * @eq: The event queue to bind this completion queue to.
 *
 * This function creates a completion queue, as detailed in @wq, on a port,
 * described by @phba by sending a CQ_CREATE mailbox command to the HBA.
 *
 * The @phba struct is used to send mailbox command to HBA. The @cq struct
 * is used to get the entry count and entry size that are necessary to
 * determine the number of pages to allocate and use for this queue. The @eq
 * is used to indicate which event queue to bind this completion queue to. This
 * function will send the CQ_CREATE mailbox command to the HBA to setup the
 * completion queue. This function is asynchronous and will wait for the mailbox
 * command to finish before continuing.
 *
 * On success this function will return a zero. If unable to allocate enough
 * memory this function will return -ENOMEM. If the queue create mailbox command
 * fails this function will return -ENXIO.
 **/
uint32_t
lpfc_cq_create(struct lpfc_hba *phba, struct lpfc_queue *cq,
	       struct lpfc_queue *eq, uint32_t type, uint32_t subtype)
{
	struct lpfc_mbx_cq_create *cq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	/* sanity check on queue memory */
	if (!cq || !eq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_cq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_CQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	cq_create = &mbox->u.mqe.un.cq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &cq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_cq_create_num_pages, &cq_create->u.request,
		    cq->page_count);
	bf_set(lpfc_cq_context_event, &cq_create->u.request.context, 1);
	bf_set(lpfc_cq_context_valid, &cq_create->u.request.context, 1);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.cqv);
	if (phba->sli4_hba.pc_sli4_params.cqv == LPFC_Q_CREATE_VERSION_2) {
		/* FW only supports 1. Should be PAGE_SIZE/SLI4_PAGE_SIZE */
		bf_set(lpfc_mbx_cq_create_page_size, &cq_create->u.request, 1);
		bf_set(lpfc_cq_eq_id_2, &cq_create->u.request.context,
		       eq->queue_id);
	} else {
		bf_set(lpfc_cq_eq_id, &cq_create->u.request.context,
		       eq->queue_id);
	}
	switch (cq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0361 Unsupported CQ count. (%d)\n",
				cq->entry_count);
		if (cq->entry_count < 256) {
			status = -EINVAL;
			goto out;
		}
		/* otherwise default to smallest count (drop through) */
	case 256:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_256);
		break;
	case 512:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_512);
		break;
	case 1024:
		bf_set(lpfc_cq_context_count, &cq_create->u.request.context,
		       LPFC_CQ_CNT_1024);
		break;
	}
	list_for_each_entry(dmabuf, &cq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		cq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		cq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);

	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2501 CQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	cq->queue_id = bf_get(lpfc_mbx_cq_create_q_id, &cq_create->u.response);
	if (cq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	/* link the cq onto the parent eq child list */
	list_add_tail(&cq->list, &eq->child_list);
	/* Set up completion queue's type and subtype */
	cq->type = type;
	cq->subtype = subtype;
	cq->queue_id = bf_get(lpfc_mbx_cq_create_q_id, &cq_create->u.response);
	cq->assoc_qid = eq->queue_id;
	cq->host_index = 0;
	cq->hba_index = 0;

out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_mq_create_fb_init - Send MCC_CREATE without async events registration
 * @phba: HBA structure that indicates port to create a queue on.
 * @mq: The queue structure to use to create the mailbox queue.
 * @mbox: An allocated pointer to type LPFC_MBOXQ_t
 * @cq: The completion queue to associate with this cq.
 *
 * This function provides failback (fb) functionality when the
 * mq_create_ext fails on older FW generations.  It's purpose is identical
 * to mq_create_ext otherwise.
 *
 * This routine cannot fail as all attributes were previously accessed and
 * initialized in mq_create_ext.
 **/
static void
lpfc_mq_create_fb_init(struct lpfc_hba *phba, struct lpfc_queue *mq,
		       LPFC_MBOXQ_t *mbox, struct lpfc_queue *cq)
{
	struct lpfc_mbx_mq_create *mq_create;
	struct lpfc_dmabuf *dmabuf;
	int length;

	length = (sizeof(struct lpfc_mbx_mq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	mq_create = &mbox->u.mqe.un.mq_create;
	bf_set(lpfc_mbx_mq_create_num_pages, &mq_create->u.request,
	       mq->page_count);
	bf_set(lpfc_mq_context_cq_id, &mq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mq_context_valid, &mq_create->u.request.context, 1);
	switch (mq->entry_count) {
	case 16:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_16);
		break;
	case 32:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_32);
		break;
	case 64:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_64);
		break;
	case 128:
		bf_set(lpfc_mq_context_ring_size, &mq_create->u.request.context,
		       LPFC_MQ_RING_SIZE_128);
		break;
	}
	list_for_each_entry(dmabuf, &mq->page_list, list) {
		mq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
			putPaddrLow(dmabuf->phys);
		mq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
			putPaddrHigh(dmabuf->phys);
	}
}

/**
 * lpfc_mq_create - Create a mailbox Queue on the HBA
 * @phba: HBA structure that indicates port to create a queue on.
 * @mq: The queue structure to use to create the mailbox queue.
 * @cq: The completion queue to associate with this cq.
 * @subtype: The queue's subtype.
 *
 * This function creates a mailbox queue, as detailed in @mq, on a port,
 * described by @phba by sending a MQ_CREATE mailbox command to the HBA.
 *
 * The @phba struct is used to send mailbox command to HBA. The @cq struct
 * is used to get the entry count and entry size that are necessary to
 * determine the number of pages to allocate and use for this queue. This
 * function will send the MQ_CREATE mailbox command to the HBA to setup the
 * mailbox queue. This function is asynchronous and will wait for the mailbox
 * command to finish before continuing.
 *
 * On success this function will return a zero. If unable to allocate enough
 * memory this function will return -ENOMEM. If the queue create mailbox command
 * fails this function will return -ENXIO.
 **/
int32_t
lpfc_mq_create(struct lpfc_hba *phba, struct lpfc_queue *mq,
	       struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_mq_create *mq_create;
	struct lpfc_mbx_mq_create_ext *mq_create_ext;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;

	/* sanity check on queue memory */
	if (!mq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_mq_create_ext) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_CREATE_EXT,
			 length, LPFC_SLI4_MBX_EMBED);

	mq_create_ext = &mbox->u.mqe.un.mq_create_ext;
	shdr = (union lpfc_sli4_cfg_shdr *) &mq_create_ext->header.cfg_shdr;
	bf_set(lpfc_mbx_mq_create_ext_num_pages,
	       &mq_create_ext->u.request, mq->page_count);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_link,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_fip,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_group5,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_fc,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mbx_mq_create_ext_async_evt_sli,
	       &mq_create_ext->u.request, 1);
	bf_set(lpfc_mq_context_valid, &mq_create_ext->u.request.context, 1);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.mqv);
	if (phba->sli4_hba.pc_sli4_params.mqv == LPFC_Q_CREATE_VERSION_1)
		bf_set(lpfc_mbx_mq_create_ext_cq_id, &mq_create_ext->u.request,
		       cq->queue_id);
	else
		bf_set(lpfc_mq_context_cq_id, &mq_create_ext->u.request.context,
		       cq->queue_id);
	switch (mq->entry_count) {
	default:
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0362 Unsupported MQ count. (%d)\n",
				mq->entry_count);
		if (mq->entry_count < 16) {
			status = -EINVAL;
			goto out;
		}
		/* otherwise default to smallest count (drop through) */
	case 16:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_16);
		break;
	case 32:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_32);
		break;
	case 64:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_64);
		break;
	case 128:
		bf_set(lpfc_mq_context_ring_size,
		       &mq_create_ext->u.request.context,
		       LPFC_MQ_RING_SIZE_128);
		break;
	}
	list_for_each_entry(dmabuf, &mq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		mq_create_ext->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		mq_create_ext->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	mq->queue_id = bf_get(lpfc_mbx_mq_create_q_id,
			      &mq_create_ext->u.response);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"2795 MQ_CREATE_EXT failed with "
				"status x%x. Failback to MQ_CREATE.\n",
				rc);
		lpfc_mq_create_fb_init(phba, mq, mbox, cq);
		mq_create = &mbox->u.mqe.un.mq_create;
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
		shdr = (union lpfc_sli4_cfg_shdr *) &mq_create->header.cfg_shdr;
		mq->queue_id = bf_get(lpfc_mbx_mq_create_q_id,
				      &mq_create->u.response);
	}

	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2502 MQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	if (mq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	mq->type = LPFC_MQ;
	mq->assoc_qid = cq->queue_id;
	mq->subtype = subtype;
	mq->host_index = 0;
	mq->hba_index = 0;

	/* link the mq onto the parent cq child list */
	list_add_tail(&mq->list, &cq->child_list);
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_wq_create - Create a Work Queue on the HBA
 * @phba: HBA structure that indicates port to create a queue on.
 * @wq: The queue structure to use to create the work queue.
 * @cq: The completion queue to bind this work queue to.
 * @subtype: The subtype of the work queue indicating its functionality.
 *
 * This function creates a work queue, as detailed in @wq, on a port, described
 * by @phba by sending a WQ_CREATE mailbox command to the HBA.
 *
 * The @phba struct is used to send mailbox command to HBA. The @wq struct
 * is used to get the entry count and entry size that are necessary to
 * determine the number of pages to allocate and use for this queue. The @cq
 * is used to indicate which completion queue to bind this work queue to. This
 * function will send the WQ_CREATE mailbox command to the HBA to setup the
 * work queue. This function is asynchronous and will wait for the mailbox
 * command to finish before continuing.
 *
 * On success this function will return a zero. If unable to allocate enough
 * memory this function will return -ENOMEM. If the queue create mailbox command
 * fails this function will return -ENXIO.
 **/
uint32_t
lpfc_wq_create(struct lpfc_hba *phba, struct lpfc_queue *wq,
	       struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_wq_create *wq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;
	struct dma_address *page;
	void __iomem *bar_memmap_p;
	uint32_t db_offset;
	uint16_t pci_barset;

	/* sanity check on queue memory */
	if (!wq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_wq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_WQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	wq_create = &mbox->u.mqe.un.wq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &wq_create->header.cfg_shdr;
	bf_set(lpfc_mbx_wq_create_num_pages, &wq_create->u.request,
		    wq->page_count);
	bf_set(lpfc_mbx_wq_create_cq_id, &wq_create->u.request,
		    cq->queue_id);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.wqv);

	if (phba->sli4_hba.pc_sli4_params.wqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_mbx_wq_create_wqe_count, &wq_create->u.request_1,
		       wq->entry_count);
		switch (wq->entry_size) {
		default:
		case 64:
			bf_set(lpfc_mbx_wq_create_wqe_size,
			       &wq_create->u.request_1,
			       LPFC_WQ_WQE_SIZE_64);
			break;
		case 128:
			bf_set(lpfc_mbx_wq_create_wqe_size,
			       &wq_create->u.request_1,
			       LPFC_WQ_WQE_SIZE_128);
			break;
		}
		bf_set(lpfc_mbx_wq_create_page_size, &wq_create->u.request_1,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
		page = wq_create->u.request_1.page;
	} else {
		page = wq_create->u.request.page;
	}
	list_for_each_entry(dmabuf, &wq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		page[dmabuf->buffer_tag].addr_lo = putPaddrLow(dmabuf->phys);
		page[dmabuf->buffer_tag].addr_hi = putPaddrHigh(dmabuf->phys);
	}

	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE)
		bf_set(lpfc_mbx_wq_create_dua, &wq_create->u.request, 1);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2503 WQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	wq->queue_id = bf_get(lpfc_mbx_wq_create_q_id, &wq_create->u.response);
	if (wq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE) {
		wq->db_format = bf_get(lpfc_mbx_wq_create_db_format,
				       &wq_create->u.response);
		if ((wq->db_format != LPFC_DB_LIST_FORMAT) &&
		    (wq->db_format != LPFC_DB_RING_FORMAT)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3265 WQ[%d] doorbell format not "
					"supported: x%x\n", wq->queue_id,
					wq->db_format);
			status = -EINVAL;
			goto out;
		}
		pci_barset = bf_get(lpfc_mbx_wq_create_bar_set,
				    &wq_create->u.response);
		bar_memmap_p = lpfc_dual_chute_pci_bar_map(phba, pci_barset);
		if (!bar_memmap_p) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3263 WQ[%d] failed to memmap pci "
					"barset:x%x\n", wq->queue_id,
					pci_barset);
			status = -ENOMEM;
			goto out;
		}
		db_offset = wq_create->u.response.doorbell_offset;
		if ((db_offset != LPFC_ULP0_WQ_DOORBELL) &&
		    (db_offset != LPFC_ULP1_WQ_DOORBELL)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3252 WQ[%d] doorbell offset not "
					"supported: x%x\n", wq->queue_id,
					db_offset);
			status = -EINVAL;
			goto out;
		}
		wq->db_regaddr = bar_memmap_p + db_offset;
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3264 WQ[%d]: barset:x%x, offset:x%x, "
				"format:x%x\n", wq->queue_id, pci_barset,
				db_offset, wq->db_format);
	} else {
		wq->db_format = LPFC_DB_LIST_FORMAT;
		wq->db_regaddr = phba->sli4_hba.WQDBregaddr;
	}
	wq->type = LPFC_WQ;
	wq->assoc_qid = cq->queue_id;
	wq->subtype = subtype;
	wq->host_index = 0;
	wq->hba_index = 0;
	wq->entry_repost = LPFC_RELEASE_NOTIFICATION_INTERVAL;

	/* link the wq onto the parent cq child list */
	list_add_tail(&wq->list, &cq->child_list);
out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_rq_adjust_repost - Adjust entry_repost for an RQ
 * @phba: HBA structure that indicates port to create a queue on.
 * @rq:   The queue structure to use for the receive queue.
 * @qno:  The associated HBQ number
 *
 *
 * For SLI4 we need to adjust the RQ repost value based on
 * the number of buffers that are initially posted to the RQ.
 */
void
lpfc_rq_adjust_repost(struct lpfc_hba *phba, struct lpfc_queue *rq, int qno)
{
	uint32_t cnt;

	/* sanity check on queue memory */
	if (!rq)
		return;
	cnt = lpfc_hbq_defs[qno]->entry_count;

	/* Recalc repost for RQs based on buffers initially posted */
	cnt = (cnt >> 3);
	if (cnt < LPFC_QUEUE_MIN_REPOST)
		cnt = LPFC_QUEUE_MIN_REPOST;

	rq->entry_repost = cnt;
}

/**
 * lpfc_rq_create - Create a Receive Queue on the HBA
 * @phba: HBA structure that indicates port to create a queue on.
 * @hrq: The queue structure to use to create the header receive queue.
 * @drq: The queue structure to use to create the data receive queue.
 * @cq: The completion queue to bind this work queue to.
 *
 * This function creates a receive buffer queue pair , as detailed in @hrq and
 * @drq, on a port, described by @phba by sending a RQ_CREATE mailbox command
 * to the HBA.
 *
 * The @phba struct is used to send mailbox command to HBA. The @drq and @hrq
 * struct is used to get the entry count that is necessary to determine the
 * number of pages to use for this queue. The @cq is used to indicate which
 * completion queue to bind received buffers that are posted to these queues to.
 * This function will send the RQ_CREATE mailbox command to the HBA to setup the
 * receive queue pair. This function is asynchronous and will wait for the
 * mailbox command to finish before continuing.
 *
 * On success this function will return a zero. If unable to allocate enough
 * memory this function will return -ENOMEM. If the queue create mailbox command
 * fails this function will return -ENXIO.
 **/
uint32_t
lpfc_rq_create(struct lpfc_hba *phba, struct lpfc_queue *hrq,
	       struct lpfc_queue *drq, struct lpfc_queue *cq, uint32_t subtype)
{
	struct lpfc_mbx_rq_create *rq_create;
	struct lpfc_dmabuf *dmabuf;
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t hw_page_size = phba->sli4_hba.pc_sli4_params.if_page_sz;
	void __iomem *bar_memmap_p;
	uint32_t db_offset;
	uint16_t pci_barset;

	/* sanity check on queue memory */
	if (!hrq || !drq || !cq)
		return -ENODEV;
	if (!phba->sli4_hba.pc_sli4_params.supported)
		hw_page_size = SLI4_PAGE_SIZE;

	if (hrq->entry_count != drq->entry_count)
		return -EINVAL;
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_rq_create) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	rq_create = &mbox->u.mqe.un.rq_create;
	shdr = (union lpfc_sli4_cfg_shdr *) &rq_create->header.cfg_shdr;
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.rqv);
	if (phba->sli4_hba.pc_sli4_params.rqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_rq_context_rqe_count_1,
		       &rq_create->u.request.context,
		       hrq->entry_count);
		rq_create->u.request.context.buffer_size = LPFC_HDR_BUF_SIZE;
		bf_set(lpfc_rq_context_rqe_size,
		       &rq_create->u.request.context,
		       LPFC_RQE_SIZE_8);
		bf_set(lpfc_rq_context_page_size,
		       &rq_create->u.request.context,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
	} else {
		switch (hrq->entry_count) {
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2535 Unsupported RQ count. (%d)\n",
					hrq->entry_count);
			if (hrq->entry_count < 512) {
				status = -EINVAL;
				goto out;
			}
			/* otherwise default to smallest count (drop through) */
		case 512:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_512);
			break;
		case 1024:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_1024);
			break;
		case 2048:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_2048);
			break;
		case 4096:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_4096);
			break;
		}
		bf_set(lpfc_rq_context_buf_size, &rq_create->u.request.context,
		       LPFC_HDR_BUF_SIZE);
	}
	bf_set(lpfc_rq_context_cq_id, &rq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mbx_rq_create_num_pages, &rq_create->u.request,
	       hrq->page_count);
	list_for_each_entry(dmabuf, &hrq->page_list, list) {
		memset(dmabuf->virt, 0, hw_page_size);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE)
		bf_set(lpfc_mbx_rq_create_dua, &rq_create->u.request, 1);

	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2504 RQ_CREATE mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
		goto out;
	}
	hrq->queue_id = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (hrq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}

	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE) {
		hrq->db_format = bf_get(lpfc_mbx_rq_create_db_format,
					&rq_create->u.response);
		if ((hrq->db_format != LPFC_DB_LIST_FORMAT) &&
		    (hrq->db_format != LPFC_DB_RING_FORMAT)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3262 RQ [%d] doorbell format not "
					"supported: x%x\n", hrq->queue_id,
					hrq->db_format);
			status = -EINVAL;
			goto out;
		}

		pci_barset = bf_get(lpfc_mbx_rq_create_bar_set,
				    &rq_create->u.response);
		bar_memmap_p = lpfc_dual_chute_pci_bar_map(phba, pci_barset);
		if (!bar_memmap_p) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3269 RQ[%d] failed to memmap pci "
					"barset:x%x\n", hrq->queue_id,
					pci_barset);
			status = -ENOMEM;
			goto out;
		}

		db_offset = rq_create->u.response.doorbell_offset;
		if ((db_offset != LPFC_ULP0_RQ_DOORBELL) &&
		    (db_offset != LPFC_ULP1_RQ_DOORBELL)) {
			lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
					"3270 RQ[%d] doorbell offset not "
					"supported: x%x\n", hrq->queue_id,
					db_offset);
			status = -EINVAL;
			goto out;
		}
		hrq->db_regaddr = bar_memmap_p + db_offset;
		lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
				"3266 RQ[qid:%d]: barset:x%x, offset:x%x, "
				"format:x%x\n", hrq->queue_id, pci_barset,
				db_offset, hrq->db_format);
	} else {
		hrq->db_format = LPFC_DB_RING_FORMAT;
		hrq->db_regaddr = phba->sli4_hba.RQDBregaddr;
	}
	hrq->type = LPFC_HRQ;
	hrq->assoc_qid = cq->queue_id;
	hrq->subtype = subtype;
	hrq->host_index = 0;
	hrq->hba_index = 0;

	/* now create the data queue */
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_CREATE,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbox_hdr_version, &shdr->request,
	       phba->sli4_hba.pc_sli4_params.rqv);
	if (phba->sli4_hba.pc_sli4_params.rqv == LPFC_Q_CREATE_VERSION_1) {
		bf_set(lpfc_rq_context_rqe_count_1,
		       &rq_create->u.request.context, hrq->entry_count);
		rq_create->u.request.context.buffer_size = LPFC_DATA_BUF_SIZE;
		bf_set(lpfc_rq_context_rqe_size, &rq_create->u.request.context,
		       LPFC_RQE_SIZE_8);
		bf_set(lpfc_rq_context_page_size, &rq_create->u.request.context,
		       (PAGE_SIZE/SLI4_PAGE_SIZE));
	} else {
		switch (drq->entry_count) {
		default:
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2536 Unsupported RQ count. (%d)\n",
					drq->entry_count);
			if (drq->entry_count < 512) {
				status = -EINVAL;
				goto out;
			}
			/* otherwise default to smallest count (drop through) */
		case 512:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_512);
			break;
		case 1024:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_1024);
			break;
		case 2048:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_2048);
			break;
		case 4096:
			bf_set(lpfc_rq_context_rqe_count,
			       &rq_create->u.request.context,
			       LPFC_RQ_RING_SIZE_4096);
			break;
		}
		bf_set(lpfc_rq_context_buf_size, &rq_create->u.request.context,
		       LPFC_DATA_BUF_SIZE);
	}
	bf_set(lpfc_rq_context_cq_id, &rq_create->u.request.context,
	       cq->queue_id);
	bf_set(lpfc_mbx_rq_create_num_pages, &rq_create->u.request,
	       drq->page_count);
	list_for_each_entry(dmabuf, &drq->page_list, list) {
		rq_create->u.request.page[dmabuf->buffer_tag].addr_lo =
					putPaddrLow(dmabuf->phys);
		rq_create->u.request.page[dmabuf->buffer_tag].addr_hi =
					putPaddrHigh(dmabuf->phys);
	}
	if (phba->sli4_hba.fw_func_mode & LPFC_DUA_MODE)
		bf_set(lpfc_mbx_rq_create_dua, &rq_create->u.request, 1);
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *) &rq_create->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		status = -ENXIO;
		goto out;
	}
	drq->queue_id = bf_get(lpfc_mbx_rq_create_q_id, &rq_create->u.response);
	if (drq->queue_id == 0xFFFF) {
		status = -ENXIO;
		goto out;
	}
	drq->type = LPFC_DRQ;
	drq->assoc_qid = cq->queue_id;
	drq->subtype = subtype;
	drq->host_index = 0;
	drq->hba_index = 0;

	/* link the header and data RQs onto the parent cq child list */
	list_add_tail(&hrq->list, &cq->child_list);
	list_add_tail(&drq->list, &cq->child_list);

out:
	mempool_free(mbox, phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_eq_destroy - Destroy an event Queue on the HBA
 * @eq: The queue structure associated with the queue to destroy.
 *
 * This function destroys a queue, as detailed in @eq by sending an mailbox
 * command, specific to the type of queue, to the HBA.
 *
 * The @eq struct is used to get the queue ID of the queue to destroy.
 *
 * On success this function will return a zero. If the queue destroy mailbox
 * command fails this function will return -ENXIO.
 **/
uint32_t
lpfc_eq_destroy(struct lpfc_hba *phba, struct lpfc_queue *eq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	/* sanity check on queue memory */
	if (!eq)
		return -ENODEV;
	mbox = mempool_alloc(eq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_eq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_EQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_eq_destroy_q_id, &mbox->u.mqe.un.eq_destroy.u.request,
	       eq->queue_id);
	mbox->vport = eq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;

	rc = lpfc_sli_issue_mbox(eq->phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.eq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2505 EQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}

	/* Remove eq from any list */
	list_del_init(&eq->list);
	mempool_free(mbox, eq->phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_cq_destroy - Destroy a Completion Queue on the HBA
 * @cq: The queue structure associated with the queue to destroy.
 *
 * This function destroys a queue, as detailed in @cq by sending an mailbox
 * command, specific to the type of queue, to the HBA.
 *
 * The @cq struct is used to get the queue ID of the queue to destroy.
 *
 * On success this function will return a zero. If the queue destroy mailbox
 * command fails this function will return -ENXIO.
 **/
uint32_t
lpfc_cq_destroy(struct lpfc_hba *phba, struct lpfc_queue *cq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	/* sanity check on queue memory */
	if (!cq)
		return -ENODEV;
	mbox = mempool_alloc(cq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_cq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_CQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_cq_destroy_q_id, &mbox->u.mqe.un.cq_destroy.u.request,
	       cq->queue_id);
	mbox->vport = cq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(cq->phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.wq_create.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2506 CQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	/* Remove cq from any list */
	list_del_init(&cq->list);
	mempool_free(mbox, cq->phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_mq_destroy - Destroy a Mailbox Queue on the HBA
 * @qm: The queue structure associated with the queue to destroy.
 *
 * This function destroys a queue, as detailed in @mq by sending an mailbox
 * command, specific to the type of queue, to the HBA.
 *
 * The @mq struct is used to get the queue ID of the queue to destroy.
 *
 * On success this function will return a zero. If the queue destroy mailbox
 * command fails this function will return -ENXIO.
 **/
uint32_t
lpfc_mq_destroy(struct lpfc_hba *phba, struct lpfc_queue *mq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	/* sanity check on queue memory */
	if (!mq)
		return -ENODEV;
	mbox = mempool_alloc(mq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_mq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			 LPFC_MBOX_OPCODE_MQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_mq_destroy_q_id, &mbox->u.mqe.un.mq_destroy.u.request,
	       mq->queue_id);
	mbox->vport = mq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(mq->phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.mq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2507 MQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	/* Remove mq from any list */
	list_del_init(&mq->list);
	mempool_free(mbox, mq->phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_wq_destroy - Destroy a Work Queue on the HBA
 * @wq: The queue structure associated with the queue to destroy.
 *
 * This function destroys a queue, as detailed in @wq by sending an mailbox
 * command, specific to the type of queue, to the HBA.
 *
 * The @wq struct is used to get the queue ID of the queue to destroy.
 *
 * On success this function will return a zero. If the queue destroy mailbox
 * command fails this function will return -ENXIO.
 **/
uint32_t
lpfc_wq_destroy(struct lpfc_hba *phba, struct lpfc_queue *wq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	/* sanity check on queue memory */
	if (!wq)
		return -ENODEV;
	mbox = mempool_alloc(wq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_wq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_WQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_wq_destroy_q_id, &mbox->u.mqe.un.wq_destroy.u.request,
	       wq->queue_id);
	mbox->vport = wq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(wq->phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.wq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2508 WQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	/* Remove wq from any list */
	list_del_init(&wq->list);
	mempool_free(mbox, wq->phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_rq_destroy - Destroy a Receive Queue on the HBA
 * @rq: The queue structure associated with the queue to destroy.
 *
 * This function destroys a queue, as detailed in @rq by sending an mailbox
 * command, specific to the type of queue, to the HBA.
 *
 * The @rq struct is used to get the queue ID of the queue to destroy.
 *
 * On success this function will return a zero. If the queue destroy mailbox
 * command fails this function will return -ENXIO.
 **/
uint32_t
lpfc_rq_destroy(struct lpfc_hba *phba, struct lpfc_queue *hrq,
		struct lpfc_queue *drq)
{
	LPFC_MBOXQ_t *mbox;
	int rc, length, status = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	/* sanity check on queue memory */
	if (!hrq || !drq)
		return -ENODEV;
	mbox = mempool_alloc(hrq->phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;
	length = (sizeof(struct lpfc_mbx_rq_destroy) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_RQ_DESTROY,
			 length, LPFC_SLI4_MBX_EMBED);
	bf_set(lpfc_mbx_rq_destroy_q_id, &mbox->u.mqe.un.rq_destroy.u.request,
	       hrq->queue_id);
	mbox->vport = hrq->phba->pport;
	mbox->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	rc = lpfc_sli_issue_mbox(hrq->phba, mbox, MBX_POLL);
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.rq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2509 RQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		if (rc != MBX_TIMEOUT)
			mempool_free(mbox, hrq->phba->mbox_mem_pool);
		return -ENXIO;
	}
	bf_set(lpfc_mbx_rq_destroy_q_id, &mbox->u.mqe.un.rq_destroy.u.request,
	       drq->queue_id);
	rc = lpfc_sli_issue_mbox(drq->phba, mbox, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *)
		&mbox->u.mqe.un.rq_destroy.header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2510 RQ_DESTROY mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		status = -ENXIO;
	}
	list_del_init(&hrq->list);
	list_del_init(&drq->list);
	mempool_free(mbox, hrq->phba->mbox_mem_pool);
	return status;
}

/**
 * lpfc_sli4_post_sgl - Post scatter gather list for an XRI to HBA
 * @phba: The virtual port for which this call being executed.
 * @pdma_phys_addr0: Physical address of the 1st SGL page.
 * @pdma_phys_addr1: Physical address of the 2nd SGL page.
 * @xritag: the xritag that ties this io to the SGL pages.
 *
 * This routine will post the sgl pages for the IO that has the xritag
 * that is in the iocbq structure. The xritag is assigned during iocbq
 * creation and persists for as long as the driver is loaded.
 * if the caller has fewer than 256 scatter gather segments to map then
 * pdma_phys_addr1 should be 0.
 * If the caller needs to map more than 256 scatter gather segment then
 * pdma_phys_addr1 should be a valid physical address.
 * physical address for SGLs must be 64 byte aligned.
 * If you are going to map 2 SGL's then the first one must have 256 entries
 * the second sgl can have between 1 and 256 entries.
 *
 * Return codes:
 * 	0 - Success
 * 	-ENXIO, -ENOMEM - Failure
 **/
int
lpfc_sli4_post_sgl(struct lpfc_hba *phba,
		dma_addr_t pdma_phys_addr0,
		dma_addr_t pdma_phys_addr1,
		uint16_t xritag)
{
	struct lpfc_mbx_post_sgl_pages *post_sgl_pages;
	LPFC_MBOXQ_t *mbox;
	int rc;
	uint32_t shdr_status, shdr_add_status;
	uint32_t mbox_tmo;
	union lpfc_sli4_cfg_shdr *shdr;

	if (xritag == NO_XRI) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"0364 Invalid param:\n");
		return -EINVAL;
	}

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES,
			sizeof(struct lpfc_mbx_post_sgl_pages) -
			sizeof(struct lpfc_sli4_cfg_mhdr), LPFC_SLI4_MBX_EMBED);

	post_sgl_pages = (struct lpfc_mbx_post_sgl_pages *)
				&mbox->u.mqe.un.post_sgl_pages;
	bf_set(lpfc_post_sgl_pages_xri, post_sgl_pages, xritag);
	bf_set(lpfc_post_sgl_pages_xricnt, post_sgl_pages, 1);

	post_sgl_pages->sgl_pg_pairs[0].sgl_pg0_addr_lo	=
				cpu_to_le32(putPaddrLow(pdma_phys_addr0));
	post_sgl_pages->sgl_pg_pairs[0].sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_addr0));

	post_sgl_pages->sgl_pg_pairs[0].sgl_pg1_addr_lo	=
				cpu_to_le32(putPaddrLow(pdma_phys_addr1));
	post_sgl_pages->sgl_pg_pairs[0].sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(pdma_phys_addr1));
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *) &post_sgl_pages->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		mempool_free(mbox, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2511 POST_SGL mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return 0;
}

/**
 * lpfc_sli4_alloc_xri - Get an available rpi in the device's range
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to post rpi header templates to the
 * HBA consistent with the SLI-4 interface spec.  This routine
 * posts a SLI4_PAGE_SIZE memory region to the port to hold up to
 * SLI4_PAGE_SIZE modulo 64 rpi context headers.
 *
 * Returns
 *	A nonzero rpi defined as rpi_base <= rpi < max_rpi if successful
 *	LPFC_RPI_ALLOC_ERROR if no rpis are available.
 **/
uint16_t
lpfc_sli4_alloc_xri(struct lpfc_hba *phba)
{
	unsigned long xri;

	/*
	 * Fetch the next logical xri.  Because this index is logical,
	 * the driver starts at 0 each time.
	 */
	spin_lock_irq(&phba->hbalock);
	xri = find_next_zero_bit(phba->sli4_hba.xri_bmask,
				 phba->sli4_hba.max_cfg_param.max_xri, 0);
	if (xri >= phba->sli4_hba.max_cfg_param.max_xri) {
		spin_unlock_irq(&phba->hbalock);
		return NO_XRI;
	} else {
		set_bit(xri, phba->sli4_hba.xri_bmask);
		phba->sli4_hba.max_cfg_param.xri_used++;
	}
	spin_unlock_irq(&phba->hbalock);
	return xri;
}

/**
 * lpfc_sli4_free_xri - Release an xri for reuse.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to release an xri to the pool of
 * available rpis maintained by the driver.
 **/
void
__lpfc_sli4_free_xri(struct lpfc_hba *phba, int xri)
{
	if (test_and_clear_bit(xri, phba->sli4_hba.xri_bmask)) {
		phba->sli4_hba.max_cfg_param.xri_used--;
	}
}

/**
 * lpfc_sli4_free_xri - Release an xri for reuse.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to release an xri to the pool of
 * available rpis maintained by the driver.
 **/
void
lpfc_sli4_free_xri(struct lpfc_hba *phba, int xri)
{
	spin_lock_irq(&phba->hbalock);
	__lpfc_sli4_free_xri(phba, xri);
	spin_unlock_irq(&phba->hbalock);
}

/**
 * lpfc_sli4_next_xritag - Get an xritag for the io
 * @phba: Pointer to HBA context object.
 *
 * This function gets an xritag for the iocb. If there is no unused xritag
 * it will return 0xffff.
 * The function returns the allocated xritag if successful, else returns zero.
 * Zero is not a valid xritag.
 * The caller is not required to hold any lock.
 **/
uint16_t
lpfc_sli4_next_xritag(struct lpfc_hba *phba)
{
	uint16_t xri_index;

	xri_index = lpfc_sli4_alloc_xri(phba);
	if (xri_index == NO_XRI)
		lpfc_printf_log(phba, KERN_WARNING, LOG_SLI,
				"2004 Failed to allocate XRI.last XRITAG is %d"
				" Max XRI is %d, Used XRI is %d\n",
				xri_index,
				phba->sli4_hba.max_cfg_param.max_xri,
				phba->sli4_hba.max_cfg_param.xri_used);
	return xri_index;
}

/**
 * lpfc_sli4_post_els_sgl_list - post a block of ELS sgls to the port.
 * @phba: pointer to lpfc hba data structure.
 * @post_sgl_list: pointer to els sgl entry list.
 * @count: number of els sgl entries on the list.
 *
 * This routine is invoked to post a block of driver's sgl pages to the
 * HBA using non-embedded mailbox command. No Lock is held. This routine
 * is only called when the driver is loading and after all IO has been
 * stopped.
 **/
static int
lpfc_sli4_post_els_sgl_list(struct lpfc_hba *phba,
			    struct list_head *post_sgl_list,
			    int post_cnt)
{
	struct lpfc_sglq *sglq_entry = NULL, *sglq_next = NULL;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xritag_start = 0;
	int rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	reqlen = phba->sli4_hba.els_xri_cnt * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"2559 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	/* Allocate DMA memory and set up the non-embedded mailbox command */
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES, reqlen,
			 LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0285 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory "
				"size (%d)\n", alloclen, reqlen);
		lpfc_sli4_mbox_cmd_free(phba, mbox);
		return -ENOMEM;
	}
	/* Set up the SGL pages in the non-embedded DMA pages */
	viraddr = mbox->sge_array->addr[0];
	sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
	sgl_pg_pairs = &sgl->sgl_pg_pairs;

	pg_pairs = 0;
	list_for_each_entry_safe(sglq_entry, sglq_next, post_sgl_list, list) {
		/* Set up the sge entry */
		sgl_pg_pairs->sgl_pg0_addr_lo =
				cpu_to_le32(putPaddrLow(sglq_entry->phys));
		sgl_pg_pairs->sgl_pg0_addr_hi =
				cpu_to_le32(putPaddrHigh(sglq_entry->phys));
		sgl_pg_pairs->sgl_pg1_addr_lo =
				cpu_to_le32(putPaddrLow(0));
		sgl_pg_pairs->sgl_pg1_addr_hi =
				cpu_to_le32(putPaddrHigh(0));

		/* Keep the first xritag on the list */
		if (pg_pairs == 0)
			xritag_start = sglq_entry->sli4_xritag;
		sgl_pg_pairs++;
		pg_pairs++;
	}

	/* Complete initialization and perform endian conversion. */
	bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
	bf_set(lpfc_post_sgl_pages_xricnt, sgl, phba->sli4_hba.els_xri_cnt);
	sgl->word0 = cpu_to_le32(sgl->word0);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2513 POST_SGL_BLOCK mailbox command failed "
				"status x%x add_status x%x mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

/**
 * lpfc_sli4_post_scsi_sgl_block - post a block of scsi sgl list to firmware
 * @phba: pointer to lpfc hba data structure.
 * @sblist: pointer to scsi buffer list.
 * @count: number of scsi buffers on the list.
 *
 * This routine is invoked to post a block of @count scsi sgl pages from a
 * SCSI buffer list @sblist to the HBA using non-embedded mailbox command.
 * No Lock is held.
 *
 **/
int
lpfc_sli4_post_scsi_sgl_block(struct lpfc_hba *phba,
			      struct list_head *sblist,
			      int count)
{
	struct lpfc_scsi_buf *psb;
	struct lpfc_mbx_post_uembed_sgl_page1 *sgl;
	struct sgl_page_pairs *sgl_pg_pairs;
	void *viraddr;
	LPFC_MBOXQ_t *mbox;
	uint32_t reqlen, alloclen, pg_pairs;
	uint32_t mbox_tmo;
	uint16_t xritag_start = 0;
	int rc = 0;
	uint32_t shdr_status, shdr_add_status;
	dma_addr_t pdma_phys_bpl1;
	union lpfc_sli4_cfg_shdr *shdr;

	/* Calculate the requested length of the dma memory */
	reqlen = count * sizeof(struct sgl_page_pairs) +
		 sizeof(union lpfc_sli4_cfg_shdr) + sizeof(uint32_t);
	if (reqlen > SLI4_PAGE_SIZE) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_INIT,
				"0217 Block sgl registration required DMA "
				"size (%d) great than a page\n", reqlen);
		return -ENOMEM;
	}
	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"0283 Failed to allocate mbox cmd memory\n");
		return -ENOMEM;
	}

	/* Allocate DMA memory and set up the non-embedded mailbox command */
	alloclen = lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
				LPFC_MBOX_OPCODE_FCOE_POST_SGL_PAGES, reqlen,
				LPFC_SLI4_MBX_NEMBED);

	if (alloclen < reqlen) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2561 Allocated DMA memory size (%d) is "
				"less than the requested DMA memory "
				"size (%d)\n", alloclen, reqlen);
		lpfc_sli4_mbox_cmd_free(phba, mbox);
		return -ENOMEM;
	}

	/* Get the first SGE entry from the non-embedded DMA memory */
	viraddr = mbox->sge_array->addr[0];

	/* Set up the SGL pages in the non-embedded DMA pages */
	sgl = (struct lpfc_mbx_post_uembed_sgl_page1 *)viraddr;
	sgl_pg_pairs = &sgl->sgl_pg_pairs;

	pg_pairs = 0;
	list_for_each_entry(psb, sblist, list) {
		/* Set up the sge entry */
		sgl_pg_pairs->sgl_pg0_addr_lo =
			cpu_to_le32(putPaddrLow(psb->dma_phys_bpl));
		sgl_pg_pairs->sgl_pg0_addr_hi =
			cpu_to_le32(putPaddrHigh(psb->dma_phys_bpl));
		if (phba->cfg_sg_dma_buf_size > SGL_PAGE_SIZE)
			pdma_phys_bpl1 = psb->dma_phys_bpl + SGL_PAGE_SIZE;
		else
			pdma_phys_bpl1 = 0;
		sgl_pg_pairs->sgl_pg1_addr_lo =
			cpu_to_le32(putPaddrLow(pdma_phys_bpl1));
		sgl_pg_pairs->sgl_pg1_addr_hi =
			cpu_to_le32(putPaddrHigh(pdma_phys_bpl1));
		/* Keep the first xritag on the list */
		if (pg_pairs == 0)
			xritag_start = psb->cur_iocbq.sli4_xritag;
		sgl_pg_pairs++;
		pg_pairs++;
	}
	bf_set(lpfc_post_sgl_pages_xri, sgl, xritag_start);
	bf_set(lpfc_post_sgl_pages_xricnt, sgl, pg_pairs);
	/* Perform endian conversion if necessary */
	sgl->word0 = cpu_to_le32(sgl->word0);

	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	shdr = (union lpfc_sli4_cfg_shdr *) &sgl->cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		lpfc_sli4_mbox_cmd_free(phba, mbox);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2564 POST_SGL_BLOCK mailbox command failed "
				"status x%x add_status x%x mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

/**
 * lpfc_fc_frame_check - Check that this frame is a valid frame to handle
 * @phba: pointer to lpfc_hba struct that the frame was received on
 * @fc_hdr: A pointer to the FC Header data (In Big Endian Format)
 *
 * This function checks the fields in the @fc_hdr to see if the FC frame is a
 * valid type of frame that the LPFC driver will handle. This function will
 * return a zero if the frame is a valid frame or a non zero value when the
 * frame does not pass the check.
 **/
static int
lpfc_fc_frame_check(struct lpfc_hba *phba, struct fc_frame_header *fc_hdr)
{
	/*  make rctl_names static to save stack space */
	static char *rctl_names[] = FC_RCTL_NAMES_INIT;
	char *type_names[] = FC_TYPE_NAMES_INIT;
	struct fc_vft_header *fc_vft_hdr;
	uint32_t *header = (uint32_t *) fc_hdr;

	switch (fc_hdr->fh_r_ctl) {
	case FC_RCTL_DD_UNCAT:		/* uncategorized information */
	case FC_RCTL_DD_SOL_DATA:	/* solicited data */
	case FC_RCTL_DD_UNSOL_CTL:	/* unsolicited control */
	case FC_RCTL_DD_SOL_CTL:	/* solicited control or reply */
	case FC_RCTL_DD_UNSOL_DATA:	/* unsolicited data */
	case FC_RCTL_DD_DATA_DESC:	/* data descriptor */
	case FC_RCTL_DD_UNSOL_CMD:	/* unsolicited command */
	case FC_RCTL_DD_CMD_STATUS:	/* command status */
	case FC_RCTL_ELS_REQ:	/* extended link services request */
	case FC_RCTL_ELS_REP:	/* extended link services reply */
	case FC_RCTL_ELS4_REQ:	/* FC-4 ELS request */
	case FC_RCTL_ELS4_REP:	/* FC-4 ELS reply */
	case FC_RCTL_BA_NOP:  	/* basic link service NOP */
	case FC_RCTL_BA_ABTS: 	/* basic link service abort */
	case FC_RCTL_BA_RMC: 	/* remove connection */
	case FC_RCTL_BA_ACC:	/* basic accept */
	case FC_RCTL_BA_RJT:	/* basic reject */
	case FC_RCTL_BA_PRMT:
	case FC_RCTL_ACK_1:	/* acknowledge_1 */
	case FC_RCTL_ACK_0:	/* acknowledge_0 */
	case FC_RCTL_P_RJT:	/* port reject */
	case FC_RCTL_F_RJT:	/* fabric reject */
	case FC_RCTL_P_BSY:	/* port busy */
	case FC_RCTL_F_BSY:	/* fabric busy to data frame */
	case FC_RCTL_F_BSYL:	/* fabric busy to link control frame */
	case FC_RCTL_LCR:	/* link credit reset */
	case FC_RCTL_END:	/* end */
		break;
	case FC_RCTL_VFTH:	/* Virtual Fabric tagging Header */
		fc_vft_hdr = (struct fc_vft_header *)fc_hdr;
		fc_hdr = &((struct fc_frame_header *)fc_vft_hdr)[1];
		return lpfc_fc_frame_check(phba, fc_hdr);
	default:
		goto drop;
	}
	switch (fc_hdr->fh_type) {
	case FC_TYPE_BLS:
	case FC_TYPE_ELS:
	case FC_TYPE_FCP:
	case FC_TYPE_CT:
		break;
	case FC_TYPE_IP:
	case FC_TYPE_ILS:
	default:
		goto drop;
	}

	lpfc_printf_log(phba, KERN_INFO, LOG_ELS,
			"2538 Received frame rctl:%s (x%x), type:%s (x%x), "
			"frame Data:%08x %08x %08x %08x %08x %08x %08x\n",
			rctl_names[fc_hdr->fh_r_ctl], fc_hdr->fh_r_ctl,
			type_names[fc_hdr->fh_type], fc_hdr->fh_type,
			be32_to_cpu(header[0]), be32_to_cpu(header[1]),
			be32_to_cpu(header[2]), be32_to_cpu(header[3]),
			be32_to_cpu(header[4]), be32_to_cpu(header[5]),
			be32_to_cpu(header[6]));
	return 0;
drop:
	lpfc_printf_log(phba, KERN_WARNING, LOG_ELS,
			"2539 Dropped frame rctl:%s type:%s\n",
			rctl_names[fc_hdr->fh_r_ctl],
			type_names[fc_hdr->fh_type]);
	return 1;
}

/**
 * lpfc_fc_hdr_get_vfi - Get the VFI from an FC frame
 * @fc_hdr: A pointer to the FC Header data (In Big Endian Format)
 *
 * This function processes the FC header to retrieve the VFI from the VF
 * header, if one exists. This function will return the VFI if one exists
 * or 0 if no VSAN Header exists.
 **/
static uint32_t
lpfc_fc_hdr_get_vfi(struct fc_frame_header *fc_hdr)
{
	struct fc_vft_header *fc_vft_hdr = (struct fc_vft_header *)fc_hdr;

	if (fc_hdr->fh_r_ctl != FC_RCTL_VFTH)
		return 0;
	return bf_get(fc_vft_hdr_vf_id, fc_vft_hdr);
}

/**
 * lpfc_fc_frame_to_vport - Finds the vport that a frame is destined to
 * @phba: Pointer to the HBA structure to search for the vport on
 * @fc_hdr: A pointer to the FC Header data (In Big Endian Format)
 * @fcfi: The FC Fabric ID that the frame came from
 *
 * This function searches the @phba for a vport that matches the content of the
 * @fc_hdr passed in and the @fcfi. This function uses the @fc_hdr to fetch the
 * VFI, if the Virtual Fabric Tagging Header exists, and the DID. This function
 * returns the matching vport pointer or NULL if unable to match frame to a
 * vport.
 **/
static struct lpfc_vport *
lpfc_fc_frame_to_vport(struct lpfc_hba *phba, struct fc_frame_header *fc_hdr,
		       uint16_t fcfi)
{
	struct lpfc_vport **vports;
	struct lpfc_vport *vport = NULL;
	int i;
	uint32_t did = (fc_hdr->fh_d_id[0] << 16 |
			fc_hdr->fh_d_id[1] << 8 |
			fc_hdr->fh_d_id[2]);

	if (did == Fabric_DID)
		return phba->pport;
	if ((phba->pport->fc_flag & FC_PT2PT) &&
		!(phba->link_state == LPFC_HBA_READY))
		return phba->pport;

	vports = lpfc_create_vport_work_array(phba);
	if (vports != NULL)
		for (i = 0; i <= phba->max_vpi && vports[i] != NULL; i++) {
			if (phba->fcf.fcfi == fcfi &&
			    vports[i]->vfi == lpfc_fc_hdr_get_vfi(fc_hdr) &&
			    vports[i]->fc_myDID == did) {
				vport = vports[i];
				break;
			}
		}
	lpfc_destroy_vport_work_array(phba, vports);
	return vport;
}

/**
 * lpfc_update_rcv_time_stamp - Update vport's rcv seq time stamp
 * @vport: The vport to work on.
 *
 * This function updates the receive sequence time stamp for this vport. The
 * receive sequence time stamp indicates the time that the last frame of the
 * the sequence that has been idle for the longest amount of time was received.
 * the driver uses this time stamp to indicate if any received sequences have
 * timed out.
 **/
void
lpfc_update_rcv_time_stamp(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf;
	struct hbq_dmabuf *dmabuf = NULL;

	/* get the oldest sequence on the rcv list */
	h_buf = list_get_first(&vport->rcv_buffer_list,
			       struct lpfc_dmabuf, list);
	if (!h_buf)
		return;
	dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
	vport->rcv_buffer_time_stamp = dmabuf->time_stamp;
}

/**
 * lpfc_cleanup_rcv_buffers - Cleans up all outstanding receive sequences.
 * @vport: The vport that the received sequences were sent to.
 *
 * This function cleans up all outstanding received sequences. This is called
 * by the driver when a link event or user action invalidates all the received
 * sequences.
 **/
void
lpfc_cleanup_rcv_buffers(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf, *hnext;
	struct lpfc_dmabuf *d_buf, *dnext;
	struct hbq_dmabuf *dmabuf = NULL;

	/* start with the oldest sequence on the rcv list */
	list_for_each_entry_safe(h_buf, hnext, &vport->rcv_buffer_list, list) {
		dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		list_del_init(&dmabuf->hbuf.list);
		list_for_each_entry_safe(d_buf, dnext,
					 &dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		lpfc_in_buf_free(vport->phba, &dmabuf->dbuf);
	}
}

/**
 * lpfc_rcv_seq_check_edtov - Cleans up timed out receive sequences.
 * @vport: The vport that the received sequences were sent to.
 *
 * This function determines whether any received sequences have timed out by
 * first checking the vport's rcv_buffer_time_stamp. If this time_stamp
 * indicates that there is at least one timed out sequence this routine will
 * go through the received sequences one at a time from most inactive to most
 * active to determine which ones need to be cleaned up. Once it has determined
 * that a sequence needs to be cleaned up it will simply free up the resources
 * without sending an abort.
 **/
void
lpfc_rcv_seq_check_edtov(struct lpfc_vport *vport)
{
	struct lpfc_dmabuf *h_buf, *hnext;
	struct lpfc_dmabuf *d_buf, *dnext;
	struct hbq_dmabuf *dmabuf = NULL;
	unsigned long timeout;
	int abort_count = 0;

	timeout = (msecs_to_jiffies(vport->phba->fc_edtov) +
		   vport->rcv_buffer_time_stamp);
	if (list_empty(&vport->rcv_buffer_list) ||
	    time_before(jiffies, timeout))
		return;
	/* start with the oldest sequence on the rcv list */
	list_for_each_entry_safe(h_buf, hnext, &vport->rcv_buffer_list, list) {
		dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		timeout = (msecs_to_jiffies(vport->phba->fc_edtov) +
			   dmabuf->time_stamp);
		if (time_before(jiffies, timeout))
			break;
		abort_count++;
		list_del_init(&dmabuf->hbuf.list);
		list_for_each_entry_safe(d_buf, dnext,
					 &dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		lpfc_in_buf_free(vport->phba, &dmabuf->dbuf);
	}
	if (abort_count)
		lpfc_update_rcv_time_stamp(vport);
}

/**
 * lpfc_fc_frame_add - Adds a frame to the vport's list of received sequences
 * @dmabuf: pointer to a dmabuf that describes the hdr and data of the FC frame
 *
 * This function searches through the existing incomplete sequences that have
 * been sent to this @vport. If the frame matches one of the incomplete
 * sequences then the dbuf in the @dmabuf is added to the list of frames that
 * make up that sequence. If no sequence is found that matches this frame then
 * the function will add the hbuf in the @dmabuf to the @vport's rcv_buffer_list
 * This function returns a pointer to the first dmabuf in the sequence list that
 * the frame was linked to.
 **/
static struct hbq_dmabuf *
lpfc_fc_frame_add(struct lpfc_vport *vport, struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *new_hdr;
	struct fc_frame_header *temp_hdr;
	struct lpfc_dmabuf *d_buf;
	struct lpfc_dmabuf *h_buf;
	struct hbq_dmabuf *seq_dmabuf = NULL;
	struct hbq_dmabuf *temp_dmabuf = NULL;

	INIT_LIST_HEAD(&dmabuf->dbuf.list);
	dmabuf->time_stamp = jiffies;
	new_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	/* Use the hdr_buf to find the sequence that this frame belongs to */
	list_for_each_entry(h_buf, &vport->rcv_buffer_list, list) {
		temp_hdr = (struct fc_frame_header *)h_buf->virt;
		if ((temp_hdr->fh_seq_id != new_hdr->fh_seq_id) ||
		    (temp_hdr->fh_ox_id != new_hdr->fh_ox_id) ||
		    (memcmp(&temp_hdr->fh_s_id, &new_hdr->fh_s_id, 3)))
			continue;
		/* found a pending sequence that matches this frame */
		seq_dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		break;
	}
	if (!seq_dmabuf) {
		/*
		 * This indicates first frame received for this sequence.
		 * Queue the buffer on the vport's rcv_buffer_list.
		 */
		list_add_tail(&dmabuf->hbuf.list, &vport->rcv_buffer_list);
		lpfc_update_rcv_time_stamp(vport);
		return dmabuf;
	}
	temp_hdr = seq_dmabuf->hbuf.virt;
	if (be16_to_cpu(new_hdr->fh_seq_cnt) <
		be16_to_cpu(temp_hdr->fh_seq_cnt)) {
		list_del_init(&seq_dmabuf->hbuf.list);
		list_add_tail(&dmabuf->hbuf.list, &vport->rcv_buffer_list);
		list_add_tail(&dmabuf->dbuf.list, &seq_dmabuf->dbuf.list);
		lpfc_update_rcv_time_stamp(vport);
		return dmabuf;
	}
	/* move this sequence to the tail to indicate a young sequence */
	list_move_tail(&seq_dmabuf->hbuf.list, &vport->rcv_buffer_list);
	seq_dmabuf->time_stamp = jiffies;
	lpfc_update_rcv_time_stamp(vport);
	if (list_empty(&seq_dmabuf->dbuf.list)) {
		temp_hdr = dmabuf->hbuf.virt;
		list_add_tail(&dmabuf->dbuf.list, &seq_dmabuf->dbuf.list);
		return seq_dmabuf;
	}
	/* find the correct place in the sequence to insert this frame */
	list_for_each_entry_reverse(d_buf, &seq_dmabuf->dbuf.list, list) {
		temp_dmabuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		temp_hdr = (struct fc_frame_header *)temp_dmabuf->hbuf.virt;
		/*
		 * If the frame's sequence count is greater than the frame on
		 * the list then insert the frame right after this frame
		 */
		if (be16_to_cpu(new_hdr->fh_seq_cnt) >
			be16_to_cpu(temp_hdr->fh_seq_cnt)) {
			list_add(&dmabuf->dbuf.list, &temp_dmabuf->dbuf.list);
			return seq_dmabuf;
		}
	}
	return NULL;
}

/**
 * lpfc_sli4_abort_partial_seq - Abort partially assembled unsol sequence
 * @vport: pointer to a vitural port
 * @dmabuf: pointer to a dmabuf that describes the FC sequence
 *
 * This function tries to abort from the partially assembed sequence, described
 * by the information from basic abbort @dmabuf. It checks to see whether such
 * partially assembled sequence held by the driver. If so, it shall free up all
 * the frames from the partially assembled sequence.
 *
 * Return
 * true  -- if there is matching partially assembled sequence present and all
 *          the frames freed with the sequence;
 * false -- if there is no matching partially assembled sequence present so
 *          nothing got aborted in the lower layer driver
 **/
static bool
lpfc_sli4_abort_partial_seq(struct lpfc_vport *vport,
			    struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *new_hdr;
	struct fc_frame_header *temp_hdr;
	struct lpfc_dmabuf *d_buf, *n_buf, *h_buf;
	struct hbq_dmabuf *seq_dmabuf = NULL;

	/* Use the hdr_buf to find the sequence that matches this frame */
	INIT_LIST_HEAD(&dmabuf->dbuf.list);
	INIT_LIST_HEAD(&dmabuf->hbuf.list);
	new_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	list_for_each_entry(h_buf, &vport->rcv_buffer_list, list) {
		temp_hdr = (struct fc_frame_header *)h_buf->virt;
		if ((temp_hdr->fh_seq_id != new_hdr->fh_seq_id) ||
		    (temp_hdr->fh_ox_id != new_hdr->fh_ox_id) ||
		    (memcmp(&temp_hdr->fh_s_id, &new_hdr->fh_s_id, 3)))
			continue;
		/* found a pending sequence that matches this frame */
		seq_dmabuf = container_of(h_buf, struct hbq_dmabuf, hbuf);
		break;
	}

	/* Free up all the frames from the partially assembled sequence */
	if (seq_dmabuf) {
		list_for_each_entry_safe(d_buf, n_buf,
					 &seq_dmabuf->dbuf.list, list) {
			list_del_init(&d_buf->list);
			lpfc_in_buf_free(vport->phba, d_buf);
		}
		return true;
	}
	return false;
}

/**
 * lpfc_sli4_abort_ulp_seq - Abort assembled unsol sequence from ulp
 * @vport: pointer to a vitural port
 * @dmabuf: pointer to a dmabuf that describes the FC sequence
 *
 * This function tries to abort from the assembed sequence from upper level
 * protocol, described by the information from basic abbort @dmabuf. It
 * checks to see whether such pending context exists at upper level protocol.
 * If so, it shall clean up the pending context.
 *
 * Return
 * true  -- if there is matching pending context of the sequence cleaned
 *          at ulp;
 * false -- if there is no matching pending context of the sequence present
 *          at ulp.
 **/
static bool
lpfc_sli4_abort_ulp_seq(struct lpfc_vport *vport, struct hbq_dmabuf *dmabuf)
{
	struct lpfc_hba *phba = vport->phba;
	int handled;

	/* Accepting abort at ulp with SLI4 only */
	if (phba->sli_rev < LPFC_SLI_REV4)
		return false;

	/* Register all caring upper level protocols to attend abort */
	handled = lpfc_ct_handle_unsol_abort(phba, dmabuf);
	if (handled)
		return true;

	return false;
}

/**
 * lpfc_sli4_seq_abort_rsp_cmpl - BLS ABORT RSP seq abort iocb complete handler
 * @phba: Pointer to HBA context object.
 * @cmd_iocbq: pointer to the command iocbq structure.
 * @rsp_iocbq: pointer to the response iocbq structure.
 *
 * This function handles the sequence abort response iocb command complete
 * event. It properly releases the memory allocated to the sequence abort
 * accept iocb.
 **/
static void
lpfc_sli4_seq_abort_rsp_cmpl(struct lpfc_hba *phba,
			     struct lpfc_iocbq *cmd_iocbq,
			     struct lpfc_iocbq *rsp_iocbq)
{
	struct lpfc_nodelist *ndlp;

	if (cmd_iocbq) {
		ndlp = (struct lpfc_nodelist *)cmd_iocbq->context1;
		lpfc_nlp_put(ndlp);
		lpfc_nlp_not_used(ndlp);
		lpfc_sli_release_iocbq(phba, cmd_iocbq);
	}

	/* Failure means BLS ABORT RSP did not get delivered to remote node*/
	if (rsp_iocbq && rsp_iocbq->iocb.ulpStatus)
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
			"3154 BLS ABORT RSP failed, data:  x%x/x%x\n",
			rsp_iocbq->iocb.ulpStatus,
			rsp_iocbq->iocb.un.ulpWord[4]);
}

/**
 * lpfc_sli4_xri_inrange - check xri is in range of xris owned by driver.
 * @phba: Pointer to HBA context object.
 * @xri: xri id in transaction.
 *
 * This function validates the xri maps to the known range of XRIs allocated an
 * used by the driver.
 **/
uint16_t
lpfc_sli4_xri_inrange(struct lpfc_hba *phba,
		      uint16_t xri)
{
	int i;

	for (i = 0; i < phba->sli4_hba.max_cfg_param.max_xri; i++) {
		if (xri == phba->sli4_hba.xri_ids[i])
			return i;
	}
	return NO_XRI;
}

/**
 * lpfc_sli4_seq_abort_rsp - bls rsp to sequence abort
 * @phba: Pointer to HBA context object.
 * @fc_hdr: pointer to a FC frame header.
 *
 * This function sends a basic response to a previous unsol sequence abort
 * event after aborting the sequence handling.
 **/
static void
lpfc_sli4_seq_abort_rsp(struct lpfc_vport *vport,
			struct fc_frame_header *fc_hdr, bool aborted)
{
	struct lpfc_hba *phba = vport->phba;
	struct lpfc_iocbq *ctiocb = NULL;
	struct lpfc_nodelist *ndlp;
	uint16_t oxid, rxid, xri, lxri;
	uint32_t sid, fctl;
	IOCB_t *icmd;
	int rc;

	if (!lpfc_is_link_up(phba))
		return;

	sid = sli4_sid_from_fc_hdr(fc_hdr);
	oxid = be16_to_cpu(fc_hdr->fh_ox_id);
	rxid = be16_to_cpu(fc_hdr->fh_rx_id);

	ndlp = lpfc_findnode_did(vport, sid);
	if (!ndlp) {
		ndlp = mempool_alloc(phba->nlp_mem_pool, GFP_KERNEL);
		if (!ndlp) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS,
					 "1268 Failed to allocate ndlp for "
					 "oxid:x%x SID:x%x\n", oxid, sid);
			return;
		}
		lpfc_nlp_init(vport, ndlp, sid);
		/* Put ndlp onto pport node list */
		lpfc_enqueue_node(vport, ndlp);
	} else if (!NLP_CHK_NODE_ACT(ndlp)) {
		/* re-setup ndlp without removing from node list */
		ndlp = lpfc_enable_node(vport, ndlp, NLP_STE_UNUSED_NODE);
		if (!ndlp) {
			lpfc_printf_vlog(vport, KERN_WARNING, LOG_ELS,
					 "3275 Failed to active ndlp found "
					 "for oxid:x%x SID:x%x\n", oxid, sid);
			return;
		}
	}

	/* Allocate buffer for rsp iocb */
	ctiocb = lpfc_sli_get_iocbq(phba);
	if (!ctiocb)
		return;

	/* Extract the F_CTL field from FC_HDR */
	fctl = sli4_fctl_from_fc_hdr(fc_hdr);

	icmd = &ctiocb->iocb;
	icmd->un.xseq64.bdl.bdeSize = 0;
	icmd->un.xseq64.bdl.ulpIoTag32 = 0;
	icmd->un.xseq64.w5.hcsw.Dfctl = 0;
	icmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_BA_ACC;
	icmd->un.xseq64.w5.hcsw.Type = FC_TYPE_BLS;

	/* Fill in the rest of iocb fields */
	icmd->ulpCommand = CMD_XMIT_BLS_RSP64_CX;
	icmd->ulpBdeCount = 0;
	icmd->ulpLe = 1;
	icmd->ulpClass = CLASS3;
	icmd->ulpContext = phba->sli4_hba.rpi_ids[ndlp->nlp_rpi];
	ctiocb->context1 = lpfc_nlp_get(ndlp);

	ctiocb->iocb_cmpl = NULL;
	ctiocb->vport = phba->pport;
	ctiocb->iocb_cmpl = lpfc_sli4_seq_abort_rsp_cmpl;
	ctiocb->sli4_lxritag = NO_XRI;
	ctiocb->sli4_xritag = NO_XRI;

	if (fctl & FC_FC_EX_CTX)
		/* Exchange responder sent the abort so we
		 * own the oxid.
		 */
		xri = oxid;
	else
		xri = rxid;
	lxri = lpfc_sli4_xri_inrange(phba, xri);
	if (lxri != NO_XRI)
		lpfc_set_rrq_active(phba, ndlp, lxri,
			(xri == oxid) ? rxid : oxid, 0);
	/* For BA_ABTS from exchange responder, if the logical xri with
	 * the oxid maps to the FCP XRI range, the port no longer has
	 * that exchange context, send a BLS_RJT. Override the IOCB for
	 * a BA_RJT.
	 */
	if ((fctl & FC_FC_EX_CTX) &&
	    (lxri > lpfc_sli4_get_els_iocb_cnt(phba))) {
		icmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_BA_RJT;
		bf_set(lpfc_vndr_code, &icmd->un.bls_rsp, 0);
		bf_set(lpfc_rsn_expln, &icmd->un.bls_rsp, FC_BA_RJT_INV_XID);
		bf_set(lpfc_rsn_code, &icmd->un.bls_rsp, FC_BA_RJT_UNABLE);
	}

	/* If BA_ABTS failed to abort a partially assembled receive sequence,
	 * the driver no longer has that exchange, send a BLS_RJT. Override
	 * the IOCB for a BA_RJT.
	 */
	if (aborted == false) {
		icmd->un.xseq64.w5.hcsw.Rctl = FC_RCTL_BA_RJT;
		bf_set(lpfc_vndr_code, &icmd->un.bls_rsp, 0);
		bf_set(lpfc_rsn_expln, &icmd->un.bls_rsp, FC_BA_RJT_INV_XID);
		bf_set(lpfc_rsn_code, &icmd->un.bls_rsp, FC_BA_RJT_UNABLE);
	}

	if (fctl & FC_FC_EX_CTX) {
		/* ABTS sent by responder to CT exchange, construction
		 * of BA_ACC will use OX_ID from ABTS for the XRI_TAG
		 * field and RX_ID from ABTS for RX_ID field.
		 */
		bf_set(lpfc_abts_orig, &icmd->un.bls_rsp, LPFC_ABTS_UNSOL_RSP);
	} else {
		/* ABTS sent by initiator to CT exchange, construction
		 * of BA_ACC will need to allocate a new XRI as for the
		 * XRI_TAG field.
		 */
		bf_set(lpfc_abts_orig, &icmd->un.bls_rsp, LPFC_ABTS_UNSOL_INT);
	}
	bf_set(lpfc_abts_rxid, &icmd->un.bls_rsp, rxid);
	bf_set(lpfc_abts_oxid, &icmd->un.bls_rsp, oxid);

	/* Xmit CT abts response on exchange <xid> */
	lpfc_printf_vlog(vport, KERN_INFO, LOG_ELS,
			 "1200 Send BLS cmd x%x on oxid x%x Data: x%x\n",
			 icmd->un.xseq64.w5.hcsw.Rctl, oxid, phba->link_state);

	rc = lpfc_sli_issue_iocb(phba, LPFC_ELS_RING, ctiocb, 0);
	if (rc == IOCB_ERROR) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_ELS,
				 "2925 Failed to issue CT ABTS RSP x%x on "
				 "xri x%x, Data x%x\n",
				 icmd->un.xseq64.w5.hcsw.Rctl, oxid,
				 phba->link_state);
		lpfc_nlp_put(ndlp);
		ctiocb->context1 = NULL;
		lpfc_sli_release_iocbq(phba, ctiocb);
	}
}

/**
 * lpfc_sli4_handle_unsol_abort - Handle sli-4 unsolicited abort event
 * @vport: Pointer to the vport on which this sequence was received
 * @dmabuf: pointer to a dmabuf that describes the FC sequence
 *
 * This function handles an SLI-4 unsolicited abort event. If the unsolicited
 * receive sequence is only partially assembed by the driver, it shall abort
 * the partially assembled frames for the sequence. Otherwise, if the
 * unsolicited receive sequence has been completely assembled and passed to
 * the Upper Layer Protocol (UPL), it then mark the per oxid status for the
 * unsolicited sequence has been aborted. After that, it will issue a basic
 * accept to accept the abort.
 **/
void
lpfc_sli4_handle_unsol_abort(struct lpfc_vport *vport,
			     struct hbq_dmabuf *dmabuf)
{
	struct lpfc_hba *phba = vport->phba;
	struct fc_frame_header fc_hdr;
	uint32_t fctl;
	bool aborted;

	/* Make a copy of fc_hdr before the dmabuf being released */
	memcpy(&fc_hdr, dmabuf->hbuf.virt, sizeof(struct fc_frame_header));
	fctl = sli4_fctl_from_fc_hdr(&fc_hdr);

	if (fctl & FC_FC_EX_CTX) {
		/* ABTS by responder to exchange, no cleanup needed */
		aborted = true;
	} else {
		/* ABTS by initiator to exchange, need to do cleanup */
		aborted = lpfc_sli4_abort_partial_seq(vport, dmabuf);
		if (aborted == false)
			aborted = lpfc_sli4_abort_ulp_seq(vport, dmabuf);
	}
	lpfc_in_buf_free(phba, &dmabuf->dbuf);

	/* Respond with BA_ACC or BA_RJT accordingly */
	lpfc_sli4_seq_abort_rsp(vport, &fc_hdr, aborted);
}

/**
 * lpfc_seq_complete - Indicates if a sequence is complete
 * @dmabuf: pointer to a dmabuf that describes the FC sequence
 *
 * This function checks the sequence, starting with the frame described by
 * @dmabuf, to see if all the frames associated with this sequence are present.
 * the frames associated with this sequence are linked to the @dmabuf using the
 * dbuf list. This function looks for two major things. 1) That the first frame
 * has a sequence count of zero. 2) There is a frame with last frame of sequence
 * set. 3) That there are no holes in the sequence count. The function will
 * return 1 when the sequence is complete, otherwise it will return 0.
 **/
static int
lpfc_seq_complete(struct hbq_dmabuf *dmabuf)
{
	struct fc_frame_header *hdr;
	struct lpfc_dmabuf *d_buf;
	struct hbq_dmabuf *seq_dmabuf;
	uint32_t fctl;
	int seq_count = 0;

	hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	/* make sure first fame of sequence has a sequence count of zero */
	if (hdr->fh_seq_cnt != seq_count)
		return 0;
	fctl = (hdr->fh_f_ctl[0] << 16 |
		hdr->fh_f_ctl[1] << 8 |
		hdr->fh_f_ctl[2]);
	/* If last frame of sequence we can return success. */
	if (fctl & FC_FC_END_SEQ)
		return 1;
	list_for_each_entry(d_buf, &dmabuf->dbuf.list, list) {
		seq_dmabuf = container_of(d_buf, struct hbq_dmabuf, dbuf);
		hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
		/* If there is a hole in the sequence count then fail. */
		if (++seq_count != be16_to_cpu(hdr->fh_seq_cnt))
			return 0;
		fctl = (hdr->fh_f_ctl[0] << 16 |
			hdr->fh_f_ctl[1] << 8 |
			hdr->fh_f_ctl[2]);
		/* If last frame of sequence we can return success. */
		if (fctl & FC_FC_END_SEQ)
			return 1;
	}
	return 0;
}

/**
 * lpfc_prep_seq - Prep sequence for ULP processing
 * @vport: Pointer to the vport on which this sequence was received
 * @dmabuf: pointer to a dmabuf that describes the FC sequence
 *
 * This function takes a sequence, described by a list of frames, and creates
 * a list of iocbq structures to describe the sequence. This iocbq list will be
 * used to issue to the generic unsolicited sequence handler. This routine
 * returns a pointer to the first iocbq in the list. If the function is unable
 * to allocate an iocbq then it throw out the received frames that were not
 * able to be described and return a pointer to the first iocbq. If unable to
 * allocate any iocbqs (including the first) this function will return NULL.
 **/
static struct lpfc_iocbq *
lpfc_prep_seq(struct lpfc_vport *vport, struct hbq_dmabuf *seq_dmabuf)
{
	struct hbq_dmabuf *hbq_buf;
	struct lpfc_dmabuf *d_buf, *n_buf;
	struct lpfc_iocbq *first_iocbq, *iocbq;
	struct fc_frame_header *fc_hdr;
	uint32_t sid;
	uint32_t len, tot_len;
	struct ulp_bde64 *pbde;

	fc_hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
	/* remove from receive buffer list */
	list_del_init(&seq_dmabuf->hbuf.list);
	lpfc_update_rcv_time_stamp(vport);
	/* get the Remote Port's SID */
	sid = sli4_sid_from_fc_hdr(fc_hdr);
	tot_len = 0;
	/* Get an iocbq struct to fill in. */
	first_iocbq = lpfc_sli_get_iocbq(vport->phba);
	if (first_iocbq) {
		/* Initialize the first IOCB. */
		first_iocbq->iocb.unsli3.rcvsli3.acc_len = 0;
		first_iocbq->iocb.ulpStatus = IOSTAT_SUCCESS;

		/* Check FC Header to see what TYPE of frame we are rcv'ing */
		if (sli4_type_from_fc_hdr(fc_hdr) == FC_TYPE_ELS) {
			first_iocbq->iocb.ulpCommand = CMD_IOCB_RCV_ELS64_CX;
			first_iocbq->iocb.un.rcvels.parmRo =
				sli4_did_from_fc_hdr(fc_hdr);
			first_iocbq->iocb.ulpPU = PARM_NPIV_DID;
		} else
			first_iocbq->iocb.ulpCommand = CMD_IOCB_RCV_SEQ64_CX;
		first_iocbq->iocb.ulpContext = NO_XRI;
		first_iocbq->iocb.unsli3.rcvsli3.ox_id =
			be16_to_cpu(fc_hdr->fh_ox_id);
		/* iocbq is prepped for internal consumption.  Physical vpi. */
		first_iocbq->iocb.unsli3.rcvsli3.vpi =
			vport->phba->vpi_ids[vport->vpi];
		/* put the first buffer into the first IOCBq */
		first_iocbq->context2 = &seq_dmabuf->dbuf;
		first_iocbq->context3 = NULL;
		first_iocbq->iocb.ulpBdeCount = 1;
		first_iocbq->iocb.un.cont64[0].tus.f.bdeSize =
							LPFC_DATA_BUF_SIZE;
		first_iocbq->iocb.un.rcvels.remoteID = sid;
		tot_len = bf_get(lpfc_rcqe_length,
				       &seq_dmabuf->cq_event.cqe.rcqe_cmpl);
		first_iocbq->iocb.unsli3.rcvsli3.acc_len = tot_len;
	}
	iocbq = first_iocbq;
	/*
	 * Each IOCBq can have two Buffers assigned, so go through the list
	 * of buffers for this sequence and save two buffers in each IOCBq
	 */
	list_for_each_entry_safe(d_buf, n_buf, &seq_dmabuf->dbuf.list, list) {
		if (!iocbq) {
			lpfc_in_buf_free(vport->phba, d_buf);
			continue;
		}
		if (!iocbq->context3) {
			iocbq->context3 = d_buf;
			iocbq->iocb.ulpBdeCount++;
			pbde = (struct ulp_bde64 *)
					&iocbq->iocb.unsli3.sli3Words[4];
			pbde->tus.f.bdeSize = LPFC_DATA_BUF_SIZE;

			/* We need to get the size out of the right CQE */
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			len = bf_get(lpfc_rcqe_length,
				       &hbq_buf->cq_event.cqe.rcqe_cmpl);
			iocbq->iocb.unsli3.rcvsli3.acc_len += len;
			tot_len += len;
		} else {
			iocbq = lpfc_sli_get_iocbq(vport->phba);
			if (!iocbq) {
				if (first_iocbq) {
					first_iocbq->iocb.ulpStatus =
							IOSTAT_FCP_RSP_ERROR;
					first_iocbq->iocb.un.ulpWord[4] =
							IOERR_NO_RESOURCES;
				}
				lpfc_in_buf_free(vport->phba, d_buf);
				continue;
			}
			iocbq->context2 = d_buf;
			iocbq->context3 = NULL;
			iocbq->iocb.ulpBdeCount = 1;
			iocbq->iocb.un.cont64[0].tus.f.bdeSize =
							LPFC_DATA_BUF_SIZE;

			/* We need to get the size out of the right CQE */
			hbq_buf = container_of(d_buf, struct hbq_dmabuf, dbuf);
			len = bf_get(lpfc_rcqe_length,
				       &hbq_buf->cq_event.cqe.rcqe_cmpl);
			tot_len += len;
			iocbq->iocb.unsli3.rcvsli3.acc_len = tot_len;

			iocbq->iocb.un.rcvels.remoteID = sid;
			list_add_tail(&iocbq->list, &first_iocbq->list);
		}
	}
	return first_iocbq;
}

static void
lpfc_sli4_send_seq_to_ulp(struct lpfc_vport *vport,
			  struct hbq_dmabuf *seq_dmabuf)
{
	struct fc_frame_header *fc_hdr;
	struct lpfc_iocbq *iocbq, *curr_iocb, *next_iocb;
	struct lpfc_hba *phba = vport->phba;

	fc_hdr = (struct fc_frame_header *)seq_dmabuf->hbuf.virt;
	iocbq = lpfc_prep_seq(vport, seq_dmabuf);
	if (!iocbq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2707 Ring %d handler: Failed to allocate "
				"iocb Rctl x%x Type x%x received\n",
				LPFC_ELS_RING,
				fc_hdr->fh_r_ctl, fc_hdr->fh_type);
		return;
	}
	if (!lpfc_complete_unsol_iocb(phba,
				      &phba->sli.ring[LPFC_ELS_RING],
				      iocbq, fc_hdr->fh_r_ctl,
				      fc_hdr->fh_type))
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2540 Ring %d handler: unexpected Rctl "
				"x%x Type x%x received\n",
				LPFC_ELS_RING,
				fc_hdr->fh_r_ctl, fc_hdr->fh_type);

	/* Free iocb created in lpfc_prep_seq */
	list_for_each_entry_safe(curr_iocb, next_iocb,
		&iocbq->list, list) {
		list_del_init(&curr_iocb->list);
		lpfc_sli_release_iocbq(phba, curr_iocb);
	}
	lpfc_sli_release_iocbq(phba, iocbq);
}

/**
 * lpfc_sli4_handle_received_buffer - Handle received buffers from firmware
 * @phba: Pointer to HBA context object.
 *
 * This function is called with no lock held. This function processes all
 * the received buffers and gives it to upper layers when a received buffer
 * indicates that it is the final frame in the sequence. The interrupt
 * service routine processes received buffers at interrupt contexts and adds
 * received dma buffers to the rb_pend_list queue and signals the worker thread.
 * Worker thread calls lpfc_sli4_handle_received_buffer, which will call the
 * appropriate receive function when the final frame in a sequence is received.
 **/
void
lpfc_sli4_handle_received_buffer(struct lpfc_hba *phba,
				 struct hbq_dmabuf *dmabuf)
{
	struct hbq_dmabuf *seq_dmabuf;
	struct fc_frame_header *fc_hdr;
	struct lpfc_vport *vport;
	uint32_t fcfi;
	uint32_t did;

	/* Process each received buffer */
	fc_hdr = (struct fc_frame_header *)dmabuf->hbuf.virt;
	/* check to see if this a valid type of frame */
	if (lpfc_fc_frame_check(phba, fc_hdr)) {
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}
	if ((bf_get(lpfc_cqe_code,
		    &dmabuf->cq_event.cqe.rcqe_cmpl) == CQE_CODE_RECEIVE_V1))
		fcfi = bf_get(lpfc_rcqe_fcf_id_v1,
			      &dmabuf->cq_event.cqe.rcqe_cmpl);
	else
		fcfi = bf_get(lpfc_rcqe_fcf_id,
			      &dmabuf->cq_event.cqe.rcqe_cmpl);

	vport = lpfc_fc_frame_to_vport(phba, fc_hdr, fcfi);
	if (!vport) {
		/* throw out the frame */
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}

	/* d_id this frame is directed to */
	did = sli4_did_from_fc_hdr(fc_hdr);

	/* vport is registered unless we rcv a FLOGI directed to Fabric_DID */
	if (!(vport->vpi_state & LPFC_VPI_REGISTERED) &&
		(did != Fabric_DID)) {
		/*
		 * Throw out the frame if we are not pt2pt.
		 * The pt2pt protocol allows for discovery frames
		 * to be received without a registered VPI.
		 */
		if (!(vport->fc_flag & FC_PT2PT) ||
			(phba->link_state == LPFC_HBA_READY)) {
			lpfc_in_buf_free(phba, &dmabuf->dbuf);
			return;
		}
	}

	/* Handle the basic abort sequence (BA_ABTS) event */
	if (fc_hdr->fh_r_ctl == FC_RCTL_BA_ABTS) {
		lpfc_sli4_handle_unsol_abort(vport, dmabuf);
		return;
	}

	/* Link this frame */
	seq_dmabuf = lpfc_fc_frame_add(vport, dmabuf);
	if (!seq_dmabuf) {
		/* unable to add frame to vport - throw it out */
		lpfc_in_buf_free(phba, &dmabuf->dbuf);
		return;
	}
	/* If not last frame in sequence continue processing frames. */
	if (!lpfc_seq_complete(seq_dmabuf))
		return;

	/* Send the complete sequence to the upper layer protocol */
	lpfc_sli4_send_seq_to_ulp(vport, seq_dmabuf);
}

/**
 * lpfc_sli4_post_all_rpi_hdrs - Post the rpi header memory region to the port
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to post rpi header templates to the
 * HBA consistent with the SLI-4 interface spec.  This routine
 * posts a SLI4_PAGE_SIZE memory region to the port to hold up to
 * SLI4_PAGE_SIZE modulo 64 rpi context headers.
 *
 * This routine does not require any locks.  It's usage is expected
 * to be driver load or reset recovery when the driver is
 * sequential.
 *
 * Return codes
 * 	0 - successful
 *      -EIO - The mailbox failed to complete successfully.
 * 	When this error occurs, the driver is not guaranteed
 *	to have any rpi regions posted to the device and
 *	must either attempt to repost the regions or take a
 *	fatal error.
 **/
int
lpfc_sli4_post_all_rpi_hdrs(struct lpfc_hba *phba)
{
	struct lpfc_rpi_hdr *rpi_page;
	uint32_t rc = 0;
	uint16_t lrpi = 0;

	/* SLI4 ports that support extents do not require RPI headers. */
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		goto exit;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	list_for_each_entry(rpi_page, &phba->sli4_hba.lpfc_rpi_hdr_list, list) {
		/*
		 * Assign the rpi headers a physical rpi only if the driver
		 * has not initialized those resources.  A port reset only
		 * needs the headers posted.
		 */
		if (bf_get(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags) !=
		    LPFC_RPI_RSRC_RDY)
			rpi_page->start_rpi = phba->sli4_hba.rpi_ids[lrpi];

		rc = lpfc_sli4_post_rpi_hdr(phba, rpi_page);
		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2008 Error %d posting all rpi "
					"headers\n", rc);
			rc = -EIO;
			break;
		}
	}

 exit:
	bf_set(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags,
	       LPFC_RPI_RSRC_RDY);
	return rc;
}

/**
 * lpfc_sli4_post_rpi_hdr - Post an rpi header memory region to the port
 * @phba: pointer to lpfc hba data structure.
 * @rpi_page:  pointer to the rpi memory region.
 *
 * This routine is invoked to post a single rpi header to the
 * HBA consistent with the SLI-4 interface spec.  This memory region
 * maps up to 64 rpi context regions.
 *
 * Return codes
 * 	0 - successful
 * 	-ENOMEM - No available memory
 *      -EIO - The mailbox failed to complete successfully.
 **/
int
lpfc_sli4_post_rpi_hdr(struct lpfc_hba *phba, struct lpfc_rpi_hdr *rpi_page)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_mbx_post_hdr_tmpl *hdr_tmpl;
	uint32_t rc = 0;
	uint32_t shdr_status, shdr_add_status;
	union lpfc_sli4_cfg_shdr *shdr;

	/* SLI4 ports that support extents do not require RPI headers. */
	if (!phba->sli4_hba.rpi_hdrs_in_use)
		return rc;
	if (phba->sli4_hba.extents_in_use)
		return -EIO;

	/* The port is notified of the header region via a mailbox command. */
	mboxq = (LPFC_MBOXQ_t *) mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2001 Unable to allocate memory for issuing "
				"SLI_CONFIG_SPECIAL mailbox command\n");
		return -ENOMEM;
	}

	/* Post all rpi memory regions to the port. */
	hdr_tmpl = &mboxq->u.mqe.un.hdr_tmpl;
	lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_POST_HDR_TEMPLATE,
			 sizeof(struct lpfc_mbx_post_hdr_tmpl) -
			 sizeof(struct lpfc_sli4_cfg_mhdr),
			 LPFC_SLI4_MBX_EMBED);


	/* Post the physical rpi to the port for this rpi header. */
	bf_set(lpfc_mbx_post_hdr_tmpl_rpi_offset, hdr_tmpl,
	       rpi_page->start_rpi);
	bf_set(lpfc_mbx_post_hdr_tmpl_page_cnt,
	       hdr_tmpl, rpi_page->page_count);

	hdr_tmpl->rpi_paddr_lo = putPaddrLow(rpi_page->dmabuf->phys);
	hdr_tmpl->rpi_paddr_hi = putPaddrHigh(rpi_page->dmabuf->phys);
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	shdr = (union lpfc_sli4_cfg_shdr *) &hdr_tmpl->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2514 POST_RPI_HDR mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	}
	return rc;
}

/**
 * lpfc_sli4_alloc_rpi - Get an available rpi in the device's range
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to post rpi header templates to the
 * HBA consistent with the SLI-4 interface spec.  This routine
 * posts a SLI4_PAGE_SIZE memory region to the port to hold up to
 * SLI4_PAGE_SIZE modulo 64 rpi context headers.
 *
 * Returns
 * 	A nonzero rpi defined as rpi_base <= rpi < max_rpi if successful
 * 	LPFC_RPI_ALLOC_ERROR if no rpis are available.
 **/
int
lpfc_sli4_alloc_rpi(struct lpfc_hba *phba)
{
	unsigned long rpi;
	uint16_t max_rpi, rpi_limit;
	uint16_t rpi_remaining, lrpi = 0;
	struct lpfc_rpi_hdr *rpi_hdr;

	max_rpi = phba->sli4_hba.max_cfg_param.max_rpi;
	rpi_limit = phba->sli4_hba.next_rpi;

	/*
	 * Fetch the next logical rpi.  Because this index is logical,
	 * the  driver starts at 0 each time.
	 */
	spin_lock_irq(&phba->hbalock);
	rpi = find_next_zero_bit(phba->sli4_hba.rpi_bmask, rpi_limit, 0);
	if (rpi >= rpi_limit)
		rpi = LPFC_RPI_ALLOC_ERROR;
	else {
		set_bit(rpi, phba->sli4_hba.rpi_bmask);
		phba->sli4_hba.max_cfg_param.rpi_used++;
		phba->sli4_hba.rpi_count++;
	}

	/*
	 * Don't try to allocate more rpi header regions if the device limit
	 * has been exhausted.
	 */
	if ((rpi == LPFC_RPI_ALLOC_ERROR) &&
	    (phba->sli4_hba.rpi_count >= max_rpi)) {
		spin_unlock_irq(&phba->hbalock);
		return rpi;
	}

	/*
	 * RPI header postings are not required for SLI4 ports capable of
	 * extents.
	 */
	if (!phba->sli4_hba.rpi_hdrs_in_use) {
		spin_unlock_irq(&phba->hbalock);
		return rpi;
	}

	/*
	 * If the driver is running low on rpi resources, allocate another
	 * page now.  Note that the next_rpi value is used because
	 * it represents how many are actually in use whereas max_rpi notes
	 * how many are supported max by the device.
	 */
	rpi_remaining = phba->sli4_hba.next_rpi - phba->sli4_hba.rpi_count;
	spin_unlock_irq(&phba->hbalock);
	if (rpi_remaining < LPFC_RPI_LOW_WATER_MARK) {
		rpi_hdr = lpfc_sli4_create_rpi_hdr(phba);
		if (!rpi_hdr) {
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2002 Error Could not grow rpi "
					"count\n");
		} else {
			lrpi = rpi_hdr->start_rpi;
			rpi_hdr->start_rpi = phba->sli4_hba.rpi_ids[lrpi];
			lpfc_sli4_post_rpi_hdr(phba, rpi_hdr);
		}
	}

	return rpi;
}

/**
 * lpfc_sli4_free_rpi - Release an rpi for reuse.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to release an rpi to the pool of
 * available rpis maintained by the driver.
 **/
void
__lpfc_sli4_free_rpi(struct lpfc_hba *phba, int rpi)
{
	if (test_and_clear_bit(rpi, phba->sli4_hba.rpi_bmask)) {
		phba->sli4_hba.rpi_count--;
		phba->sli4_hba.max_cfg_param.rpi_used--;
	}
}

/**
 * lpfc_sli4_free_rpi - Release an rpi for reuse.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to release an rpi to the pool of
 * available rpis maintained by the driver.
 **/
void
lpfc_sli4_free_rpi(struct lpfc_hba *phba, int rpi)
{
	spin_lock_irq(&phba->hbalock);
	__lpfc_sli4_free_rpi(phba, rpi);
	spin_unlock_irq(&phba->hbalock);
}

/**
 * lpfc_sli4_remove_rpis - Remove the rpi bitmask region
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to remove the memory region that
 * provided rpi via a bitmask.
 **/
void
lpfc_sli4_remove_rpis(struct lpfc_hba *phba)
{
	kfree(phba->sli4_hba.rpi_bmask);
	kfree(phba->sli4_hba.rpi_ids);
	bf_set(lpfc_rpi_rsrc_rdy, &phba->sli4_hba.sli4_flags, 0);
}

/**
 * lpfc_sli4_resume_rpi - Remove the rpi bitmask region
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to remove the memory region that
 * provided rpi via a bitmask.
 **/
int
lpfc_sli4_resume_rpi(struct lpfc_nodelist *ndlp,
	void (*cmpl)(struct lpfc_hba *, LPFC_MBOXQ_t *), void *arg)
{
	LPFC_MBOXQ_t *mboxq;
	struct lpfc_hba *phba = ndlp->phba;
	int rc;

	/* The port is notified of the header region via a mailbox command. */
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;

	/* Post all rpi memory regions to the port. */
	lpfc_resume_rpi(mboxq, ndlp);
	if (cmpl) {
		mboxq->mbox_cmpl = cmpl;
		mboxq->context1 = arg;
		mboxq->context2 = ndlp;
	} else
		mboxq->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
	mboxq->vport = ndlp->vport;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2010 Resume RPI Mailbox failed "
				"status %d, mbxStatus x%x\n", rc,
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		mempool_free(mboxq, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

/**
 * lpfc_sli4_init_vpi - Initialize a vpi with the port
 * @vport: Pointer to the vport for which the vpi is being initialized
 *
 * This routine is invoked to activate a vpi with the port.
 *
 * Returns:
 *    0 success
 *    -Evalue otherwise
 **/
int
lpfc_sli4_init_vpi(struct lpfc_vport *vport)
{
	LPFC_MBOXQ_t *mboxq;
	int rc = 0;
	int retval = MBX_SUCCESS;
	uint32_t mbox_tmo;
	struct lpfc_hba *phba = vport->phba;
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq)
		return -ENOMEM;
	lpfc_init_vpi(phba, mboxq, vport->vpi);
	mbox_tmo = lpfc_mbox_tmo_val(phba, mboxq);
	rc = lpfc_sli_issue_mbox_wait(phba, mboxq, mbox_tmo);
	if (rc != MBX_SUCCESS) {
		lpfc_printf_vlog(vport, KERN_ERR, LOG_SLI,
				"2022 INIT VPI Mailbox failed "
				"status %d, mbxStatus x%x\n", rc,
				bf_get(lpfc_mqe_status, &mboxq->u.mqe));
		retval = -EIO;
	}
	if (rc != MBX_TIMEOUT)
		mempool_free(mboxq, vport->phba->mbox_mem_pool);

	return retval;
}

/**
 * lpfc_mbx_cmpl_add_fcf_record - add fcf mbox completion handler.
 * @phba: pointer to lpfc hba data structure.
 * @mboxq: Pointer to mailbox object.
 *
 * This routine is invoked to manually add a single FCF record. The caller
 * must pass a completely initialized FCF_Record.  This routine takes
 * care of the nonembedded mailbox operations.
 **/
static void
lpfc_mbx_cmpl_add_fcf_record(struct lpfc_hba *phba, LPFC_MBOXQ_t *mboxq)
{
	void *virt_addr;
	union lpfc_sli4_cfg_shdr *shdr;
	uint32_t shdr_status, shdr_add_status;

	virt_addr = mboxq->sge_array->addr[0];
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *) virt_addr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);

	if ((shdr_status || shdr_add_status) &&
		(shdr_status != STATUS_FCF_IN_USE))
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2558 ADD_FCF_RECORD mailbox failed with "
			"status x%x add_status x%x\n",
			shdr_status, shdr_add_status);

	lpfc_sli4_mbox_cmd_free(phba, mboxq);
}

/**
 * lpfc_sli4_add_fcf_record - Manually add an FCF Record.
 * @phba: pointer to lpfc hba data structure.
 * @fcf_record:  pointer to the initialized fcf record to add.
 *
 * This routine is invoked to manually add a single FCF record. The caller
 * must pass a completely initialized FCF_Record.  This routine takes
 * care of the nonembedded mailbox operations.
 **/
int
lpfc_sli4_add_fcf_record(struct lpfc_hba *phba, struct fcf_record *fcf_record)
{
	int rc = 0;
	LPFC_MBOXQ_t *mboxq;
	uint8_t *bytep;
	void *virt_addr;
	dma_addr_t phys_addr;
	struct lpfc_mbx_sge sge;
	uint32_t alloc_len, req_len;
	uint32_t fcfindex;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2009 Failed to allocate mbox for ADD_FCF cmd\n");
		return -ENOMEM;
	}

	req_len = sizeof(struct fcf_record) + sizeof(union lpfc_sli4_cfg_shdr) +
		  sizeof(uint32_t);

	/* Allocate DMA memory and set up the non-embedded mailbox command */
	alloc_len = lpfc_sli4_config(phba, mboxq, LPFC_MBOX_SUBSYSTEM_FCOE,
				     LPFC_MBOX_OPCODE_FCOE_ADD_FCF,
				     req_len, LPFC_SLI4_MBX_NEMBED);
	if (alloc_len < req_len) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2523 Allocated DMA memory size (x%x) is "
			"less than the requested DMA memory "
			"size (x%x)\n", alloc_len, req_len);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		return -ENOMEM;
	}

	/*
	 * Get the first SGE entry from the non-embedded DMA memory.  This
	 * routine only uses a single SGE.
	 */
	lpfc_sli4_mbx_sge_get(mboxq, 0, &sge);
	phys_addr = getPaddr(sge.pa_hi, sge.pa_lo);
	virt_addr = mboxq->sge_array->addr[0];
	/*
	 * Configure the FCF record for FCFI 0.  This is the driver's
	 * hardcoded default and gets used in nonFIP mode.
	 */
	fcfindex = bf_get(lpfc_fcf_record_fcf_index, fcf_record);
	bytep = virt_addr + sizeof(union lpfc_sli4_cfg_shdr);
	lpfc_sli_pcimem_bcopy(&fcfindex, bytep, sizeof(uint32_t));

	/*
	 * Copy the fcf_index and the FCF Record Data. The data starts after
	 * the FCoE header plus word10. The data copy needs to be endian
	 * correct.
	 */
	bytep += sizeof(uint32_t);
	lpfc_sli_pcimem_bcopy(fcf_record, bytep, sizeof(struct fcf_record));
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_add_fcf_record;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2515 ADD_FCF_RECORD mailbox failed with "
			"status 0x%x\n", rc);
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
		rc = -EIO;
	} else
		rc = 0;

	return rc;
}

/**
 * lpfc_sli4_build_dflt_fcf_record - Build the driver's default FCF Record.
 * @phba: pointer to lpfc hba data structure.
 * @fcf_record:  pointer to the fcf record to write the default data.
 * @fcf_index: FCF table entry index.
 *
 * This routine is invoked to build the driver's default FCF record.  The
 * values used are hardcoded.  This routine handles memory initialization.
 *
 **/
void
lpfc_sli4_build_dflt_fcf_record(struct lpfc_hba *phba,
				struct fcf_record *fcf_record,
				uint16_t fcf_index)
{
	memset(fcf_record, 0, sizeof(struct fcf_record));
	fcf_record->max_rcv_size = LPFC_FCOE_MAX_RCV_SIZE;
	fcf_record->fka_adv_period = LPFC_FCOE_FKA_ADV_PER;
	fcf_record->fip_priority = LPFC_FCOE_FIP_PRIORITY;
	bf_set(lpfc_fcf_record_mac_0, fcf_record, phba->fc_map[0]);
	bf_set(lpfc_fcf_record_mac_1, fcf_record, phba->fc_map[1]);
	bf_set(lpfc_fcf_record_mac_2, fcf_record, phba->fc_map[2]);
	bf_set(lpfc_fcf_record_mac_3, fcf_record, LPFC_FCOE_FCF_MAC3);
	bf_set(lpfc_fcf_record_mac_4, fcf_record, LPFC_FCOE_FCF_MAC4);
	bf_set(lpfc_fcf_record_mac_5, fcf_record, LPFC_FCOE_FCF_MAC5);
	bf_set(lpfc_fcf_record_fc_map_0, fcf_record, phba->fc_map[0]);
	bf_set(lpfc_fcf_record_fc_map_1, fcf_record, phba->fc_map[1]);
	bf_set(lpfc_fcf_record_fc_map_2, fcf_record, phba->fc_map[2]);
	bf_set(lpfc_fcf_record_fcf_valid, fcf_record, 1);
	bf_set(lpfc_fcf_record_fcf_avail, fcf_record, 1);
	bf_set(lpfc_fcf_record_fcf_index, fcf_record, fcf_index);
	bf_set(lpfc_fcf_record_mac_addr_prov, fcf_record,
		LPFC_FCF_FPMA | LPFC_FCF_SPMA);
	/* Set the VLAN bit map */
	if (phba->valid_vlan) {
		fcf_record->vlan_bitmap[phba->vlan_id / 8]
			= 1 << (phba->vlan_id % 8);
	}
}

/**
 * lpfc_sli4_fcf_scan_read_fcf_rec - Read hba fcf record for fcf scan.
 * @phba: pointer to lpfc hba data structure.
 * @fcf_index: FCF table entry offset.
 *
 * This routine is invoked to scan the entire FCF table by reading FCF
 * record and processing it one at a time starting from the @fcf_index
 * for initial FCF discovery or fast FCF failover rediscovery.
 *
 * Return 0 if the mailbox command is submitted successfully, none 0
 * otherwise.
 **/
int
lpfc_sli4_fcf_scan_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	phba->fcoe_eventtag_at_fcf_scan = phba->fcoe_eventtag;
	phba->fcoe_cvl_eventtag_attn = phba->fcoe_cvl_eventtag;
	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2000 Failed to allocate mbox for "
				"READ_FCF cmd\n");
		error = -ENOMEM;
		goto fail_fcf_scan;
	}
	/* Construct the read FCF record mailbox command */
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_scan;
	}
	/* Issue the mailbox command asynchronously */
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_fcf_scan_read_fcf_rec;

	spin_lock_irq(&phba->hbalock);
	phba->hba_flag |= FCF_TS_INPROG;
	spin_unlock_irq(&phba->hbalock);

	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else {
		/* Reset eligible FCF count for new scan */
		if (fcf_index == LPFC_FCOE_FCF_GET_FIRST)
			phba->fcf.eligible_fcf_cnt = 0;
		error = 0;
	}
fail_fcf_scan:
	if (error) {
		if (mboxq)
			lpfc_sli4_mbox_cmd_free(phba, mboxq);
		/* FCF scan failed, clear FCF_TS_INPROG flag */
		spin_lock_irq(&phba->hbalock);
		phba->hba_flag &= ~FCF_TS_INPROG;
		spin_unlock_irq(&phba->hbalock);
	}
	return error;
}

/**
 * lpfc_sli4_fcf_rr_read_fcf_rec - Read hba fcf record for roundrobin fcf.
 * @phba: pointer to lpfc hba data structure.
 * @fcf_index: FCF table entry offset.
 *
 * This routine is invoked to read an FCF record indicated by @fcf_index
 * and to use it for FLOGI roundrobin FCF failover.
 *
 * Return 0 if the mailbox command is submitted successfully, none 0
 * otherwise.
 **/
int
lpfc_sli4_fcf_rr_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_INIT,
				"2763 Failed to allocate mbox for "
				"READ_FCF cmd\n");
		error = -ENOMEM;
		goto fail_fcf_read;
	}
	/* Construct the read FCF record mailbox command */
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_read;
	}
	/* Issue the mailbox command asynchronously */
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_fcf_rr_read_fcf_rec;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else
		error = 0;

fail_fcf_read:
	if (error && mboxq)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	return error;
}

/**
 * lpfc_sli4_read_fcf_rec - Read hba fcf record for update eligible fcf bmask.
 * @phba: pointer to lpfc hba data structure.
 * @fcf_index: FCF table entry offset.
 *
 * This routine is invoked to read an FCF record indicated by @fcf_index to
 * determine whether it's eligible for FLOGI roundrobin failover list.
 *
 * Return 0 if the mailbox command is submitted successfully, none 0
 * otherwise.
 **/
int
lpfc_sli4_read_fcf_rec(struct lpfc_hba *phba, uint16_t fcf_index)
{
	int rc = 0, error;
	LPFC_MBOXQ_t *mboxq;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP | LOG_INIT,
				"2758 Failed to allocate mbox for "
				"READ_FCF cmd\n");
				error = -ENOMEM;
				goto fail_fcf_read;
	}
	/* Construct the read FCF record mailbox command */
	rc = lpfc_sli4_mbx_read_fcf_rec(phba, mboxq, fcf_index);
	if (rc) {
		error = -EINVAL;
		goto fail_fcf_read;
	}
	/* Issue the mailbox command asynchronously */
	mboxq->vport = phba->pport;
	mboxq->mbox_cmpl = lpfc_mbx_cmpl_read_fcf_rec;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_NOWAIT);
	if (rc == MBX_NOT_FINISHED)
		error = -EIO;
	else
		error = 0;

fail_fcf_read:
	if (error && mboxq)
		lpfc_sli4_mbox_cmd_free(phba, mboxq);
	return error;
}

/**
 * lpfc_check_next_fcf_pri
 * phba pointer to the lpfc_hba struct for this port.
 * This routine is called from the lpfc_sli4_fcf_rr_next_index_get
 * routine when the rr_bmask is empty. The FCF indecies are put into the
 * rr_bmask based on their priority level. Starting from the highest priority
 * to the lowest. The most likely FCF candidate will be in the highest
 * priority group. When this routine is called it searches the fcf_pri list for
 * next lowest priority group and repopulates the rr_bmask with only those
 * fcf_indexes.
 * returns:
 * 1=success 0=failure
 **/
int
lpfc_check_next_fcf_pri_level(struct lpfc_hba *phba)
{
	uint16_t next_fcf_pri;
	uint16_t last_index;
	struct lpfc_fcf_pri *fcf_pri;
	int rc;
	int ret = 0;

	last_index = find_first_bit(phba->fcf.fcf_rr_bmask,
			LPFC_SLI4_FCF_TBL_INDX_MAX);
	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"3060 Last IDX %d\n", last_index);

	/* Verify the priority list has 2 or more entries */
	spin_lock_irq(&phba->hbalock);
	if (list_empty(&phba->fcf.fcf_pri_list) ||
	    list_is_singular(&phba->fcf.fcf_pri_list)) {
		spin_unlock_irq(&phba->hbalock);
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
			"3061 Last IDX %d\n", last_index);
		return 0; /* Empty rr list */
	}
	spin_unlock_irq(&phba->hbalock);

	next_fcf_pri = 0;
	/*
	 * Clear the rr_bmask and set all of the bits that are at this
	 * priority.
	 */
	memset(phba->fcf.fcf_rr_bmask, 0,
			sizeof(*phba->fcf.fcf_rr_bmask));
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
		if (fcf_pri->fcf_rec.flag & LPFC_FCF_FLOGI_FAILED)
			continue;
		/*
		 * the 1st priority that has not FLOGI failed
		 * will be the highest.
		 */
		if (!next_fcf_pri)
			next_fcf_pri = fcf_pri->fcf_rec.priority;
		spin_unlock_irq(&phba->hbalock);
		if (fcf_pri->fcf_rec.priority == next_fcf_pri) {
			rc = lpfc_sli4_fcf_rr_index_set(phba,
						fcf_pri->fcf_rec.fcf_index);
			if (rc)
				return 0;
		}
		spin_lock_irq(&phba->hbalock);
	}
	/*
	 * if next_fcf_pri was not set above and the list is not empty then
	 * we have failed flogis on all of them. So reset flogi failed
	 * and start at the beginning.
	 */
	if (!next_fcf_pri && !list_empty(&phba->fcf.fcf_pri_list)) {
		list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
			fcf_pri->fcf_rec.flag &= ~LPFC_FCF_FLOGI_FAILED;
			/*
			 * the 1st priority that has not FLOGI failed
			 * will be the highest.
			 */
			if (!next_fcf_pri)
				next_fcf_pri = fcf_pri->fcf_rec.priority;
			spin_unlock_irq(&phba->hbalock);
			if (fcf_pri->fcf_rec.priority == next_fcf_pri) {
				rc = lpfc_sli4_fcf_rr_index_set(phba,
						fcf_pri->fcf_rec.fcf_index);
				if (rc)
					return 0;
			}
			spin_lock_irq(&phba->hbalock);
		}
	} else
		ret = 1;
	spin_unlock_irq(&phba->hbalock);

	return ret;
}
/**
 * lpfc_sli4_fcf_rr_next_index_get - Get next eligible fcf record index
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is to get the next eligible FCF record index in a round
 * robin fashion. If the next eligible FCF record index equals to the
 * initial roundrobin FCF record index, LPFC_FCOE_FCF_NEXT_NONE (0xFFFF)
 * shall be returned, otherwise, the next eligible FCF record's index
 * shall be returned.
 **/
uint16_t
lpfc_sli4_fcf_rr_next_index_get(struct lpfc_hba *phba)
{
	uint16_t next_fcf_index;

initial_priority:
	/* Search start from next bit of currently registered FCF index */
	next_fcf_index = phba->fcf.current_rec.fcf_indx;

next_priority:
	/* Determine the next fcf index to check */
	next_fcf_index = (next_fcf_index + 1) % LPFC_SLI4_FCF_TBL_INDX_MAX;
	next_fcf_index = find_next_bit(phba->fcf.fcf_rr_bmask,
				       LPFC_SLI4_FCF_TBL_INDX_MAX,
				       next_fcf_index);

	/* Wrap around condition on phba->fcf.fcf_rr_bmask */
	if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		/*
		 * If we have wrapped then we need to clear the bits that
		 * have been tested so that we can detect when we should
		 * change the priority level.
		 */
		next_fcf_index = find_next_bit(phba->fcf.fcf_rr_bmask,
					       LPFC_SLI4_FCF_TBL_INDX_MAX, 0);
	}


	/* Check roundrobin failover list empty condition */
	if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX ||
		next_fcf_index == phba->fcf.current_rec.fcf_indx) {
		/*
		 * If next fcf index is not found check if there are lower
		 * Priority level fcf's in the fcf_priority list.
		 * Set up the rr_bmask with all of the avaiable fcf bits
		 * at that level and continue the selection process.
		 */
		if (lpfc_check_next_fcf_pri_level(phba))
			goto initial_priority;
		lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"2844 No roundrobin failover FCF available\n");
		if (next_fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX)
			return LPFC_FCOE_FCF_NEXT_NONE;
		else {
			lpfc_printf_log(phba, KERN_WARNING, LOG_FIP,
				"3063 Only FCF available idx %d, flag %x\n",
				next_fcf_index,
			phba->fcf.fcf_pri[next_fcf_index].fcf_rec.flag);
			return next_fcf_index;
		}
	}

	if (next_fcf_index < LPFC_SLI4_FCF_TBL_INDX_MAX &&
		phba->fcf.fcf_pri[next_fcf_index].fcf_rec.flag &
		LPFC_FCF_FLOGI_FAILED)
		goto next_priority;

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2845 Get next roundrobin failover FCF (x%x)\n",
			next_fcf_index);

	return next_fcf_index;
}

/**
 * lpfc_sli4_fcf_rr_index_set - Set bmask with eligible fcf record index
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine sets the FCF record index in to the eligible bmask for
 * roundrobin failover search. It checks to make sure that the index
 * does not go beyond the range of the driver allocated bmask dimension
 * before setting the bit.
 *
 * Returns 0 if the index bit successfully set, otherwise, it returns
 * -EINVAL.
 **/
int
lpfc_sli4_fcf_rr_index_set(struct lpfc_hba *phba, uint16_t fcf_index)
{
	if (fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2610 FCF (x%x) reached driver's book "
				"keeping dimension:x%x\n",
				fcf_index, LPFC_SLI4_FCF_TBL_INDX_MAX);
		return -EINVAL;
	}
	/* Set the eligible FCF record index bmask */
	set_bit(fcf_index, phba->fcf.fcf_rr_bmask);

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2790 Set FCF (x%x) to roundrobin FCF failover "
			"bmask\n", fcf_index);

	return 0;
}

/**
 * lpfc_sli4_fcf_rr_index_clear - Clear bmask from eligible fcf record index
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine clears the FCF record index from the eligible bmask for
 * roundrobin failover search. It checks to make sure that the index
 * does not go beyond the range of the driver allocated bmask dimension
 * before clearing the bit.
 **/
void
lpfc_sli4_fcf_rr_index_clear(struct lpfc_hba *phba, uint16_t fcf_index)
{
	struct lpfc_fcf_pri *fcf_pri;
	if (fcf_index >= LPFC_SLI4_FCF_TBL_INDX_MAX) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2762 FCF (x%x) reached driver's book "
				"keeping dimension:x%x\n",
				fcf_index, LPFC_SLI4_FCF_TBL_INDX_MAX);
		return;
	}
	/* Clear the eligible FCF record index bmask */
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry(fcf_pri, &phba->fcf.fcf_pri_list, list) {
		if (fcf_pri->fcf_rec.fcf_index == fcf_index) {
			list_del_init(&fcf_pri->list);
			break;
		}
	}
	spin_unlock_irq(&phba->hbalock);
	clear_bit(fcf_index, phba->fcf.fcf_rr_bmask);

	lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
			"2791 Clear FCF (x%x) from roundrobin failover "
			"bmask\n", fcf_index);
}

/**
 * lpfc_mbx_cmpl_redisc_fcf_table - completion routine for rediscover FCF table
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is the completion routine for the rediscover FCF table mailbox
 * command. If the mailbox command returned failure, it will try to stop the
 * FCF rediscover wait timer.
 **/
void
lpfc_mbx_cmpl_redisc_fcf_table(struct lpfc_hba *phba, LPFC_MBOXQ_t *mbox)
{
	struct lpfc_mbx_redisc_fcf_tbl *redisc_fcf;
	uint32_t shdr_status, shdr_add_status;

	redisc_fcf = &mbox->u.mqe.un.redisc_fcf_tbl;

	shdr_status = bf_get(lpfc_mbox_hdr_status,
			     &redisc_fcf->header.cfg_shdr.response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status,
			     &redisc_fcf->header.cfg_shdr.response);
	if (shdr_status || shdr_add_status) {
		lpfc_printf_log(phba, KERN_ERR, LOG_FIP,
				"2746 Requesting for FCF rediscovery failed "
				"status x%x add_status x%x\n",
				shdr_status, shdr_add_status);
		if (phba->fcf.fcf_flag & FCF_ACVL_DISC) {
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_ACVL_DISC;
			spin_unlock_irq(&phba->hbalock);
			/*
			 * CVL event triggered FCF rediscover request failed,
			 * last resort to re-try current registered FCF entry.
			 */
			lpfc_retry_pport_discovery(phba);
		} else {
			spin_lock_irq(&phba->hbalock);
			phba->fcf.fcf_flag &= ~FCF_DEAD_DISC;
			spin_unlock_irq(&phba->hbalock);
			/*
			 * DEAD FCF event triggered FCF rediscover request
			 * failed, last resort to fail over as a link down
			 * to FCF registration.
			 */
			lpfc_sli4_fcf_dead_failthrough(phba);
		}
	} else {
		lpfc_printf_log(phba, KERN_INFO, LOG_FIP,
				"2775 Start FCF rediscover quiescent timer\n");
		/*
		 * Start FCF rediscovery wait timer for pending FCF
		 * before rescan FCF record table.
		 */
		lpfc_fcf_redisc_wait_start_timer(phba);
	}

	mempool_free(mbox, phba->mbox_mem_pool);
}

/**
 * lpfc_sli4_redisc_fcf_table - Request to rediscover entire FCF table by port.
 * @phba: pointer to lpfc hba data structure.
 *
 * This routine is invoked to request for rediscovery of the entire FCF table
 * by the port.
 **/
int
lpfc_sli4_redisc_fcf_table(struct lpfc_hba *phba)
{
	LPFC_MBOXQ_t *mbox;
	struct lpfc_mbx_redisc_fcf_tbl *redisc_fcf;
	int rc, length;

	/* Cancel retry delay timers to all vports before FCF rediscover */
	lpfc_cancel_all_vport_retry_delay_timer(phba);

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox) {
		lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2745 Failed to allocate mbox for "
				"requesting FCF rediscover.\n");
		return -ENOMEM;
	}

	length = (sizeof(struct lpfc_mbx_redisc_fcf_tbl) -
		  sizeof(struct lpfc_sli4_cfg_mhdr));
	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_FCOE,
			 LPFC_MBOX_OPCODE_FCOE_REDISCOVER_FCF,
			 length, LPFC_SLI4_MBX_EMBED);

	redisc_fcf = &mbox->u.mqe.un.redisc_fcf_tbl;
	/* Set count to 0 for invalidating the entire FCF database */
	bf_set(lpfc_mbx_redisc_fcf_count, redisc_fcf, 0);

	/* Issue the mailbox command asynchronously */
	mbox->vport = phba->pport;
	mbox->mbox_cmpl = lpfc_mbx_cmpl_redisc_fcf_table;
	rc = lpfc_sli_issue_mbox(phba, mbox, MBX_NOWAIT);

	if (rc == MBX_NOT_FINISHED) {
		mempool_free(mbox, phba->mbox_mem_pool);
		return -EIO;
	}
	return 0;
}

/**
 * lpfc_sli4_fcf_dead_failthrough - Failthrough routine to fcf dead event
 * @phba: pointer to lpfc hba data structure.
 *
 * This function is the failover routine as a last resort to the FCF DEAD
 * event when driver failed to perform fast FCF failover.
 **/
void
lpfc_sli4_fcf_dead_failthrough(struct lpfc_hba *phba)
{
	uint32_t link_state;

	/*
	 * Last resort as FCF DEAD event failover will treat this as
	 * a link down, but save the link state because we don't want
	 * it to be changed to Link Down unless it is already down.
	 */
	link_state = phba->link_state;
	lpfc_linkdown(phba);
	phba->link_state = link_state;

	/* Unregister FCF if no devices connected to it */
	lpfc_unregister_unused_fcf(phba);
}

/**
 * lpfc_sli_get_config_region23 - Get sli3 port region 23 data.
 * @phba: pointer to lpfc hba data structure.
 * @rgn23_data: pointer to configure region 23 data.
 *
 * This function gets SLI3 port configure region 23 data through memory dump
 * mailbox command. When it successfully retrieves data, the size of the data
 * will be returned, otherwise, 0 will be returned.
 **/
static uint32_t
lpfc_sli_get_config_region23(struct lpfc_hba *phba, char *rgn23_data)
{
	LPFC_MBOXQ_t *pmb = NULL;
	MAILBOX_t *mb;
	uint32_t offset = 0;
	int rc;

	if (!rgn23_data)
		return 0;

	pmb = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!pmb) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"2600 failed to allocate mailbox memory\n");
		return 0;
	}
	mb = &pmb->u.mb;

	do {
		lpfc_dump_mem(phba, pmb, offset, DMP_REGION_23);
		rc = lpfc_sli_issue_mbox(phba, pmb, MBX_POLL);

		if (rc != MBX_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_INIT,
					"2601 failed to read config "
					"region 23, rc 0x%x Status 0x%x\n",
					rc, mb->mbxStatus);
			mb->un.varDmp.word_cnt = 0;
		}
		/*
		 * dump mem may return a zero when finished or we got a
		 * mailbox error, either way we are done.
		 */
		if (mb->un.varDmp.word_cnt == 0)
			break;
		if (mb->un.varDmp.word_cnt > DMP_RGN23_SIZE - offset)
			mb->un.varDmp.word_cnt = DMP_RGN23_SIZE - offset;

		lpfc_sli_pcimem_bcopy(((uint8_t *)mb) + DMP_RSP_OFFSET,
				       rgn23_data + offset,
				       mb->un.varDmp.word_cnt);
		offset += mb->un.varDmp.word_cnt;
	} while (mb->un.varDmp.word_cnt && offset < DMP_RGN23_SIZE);

	mempool_free(pmb, phba->mbox_mem_pool);
	return offset;
}

/**
 * lpfc_sli4_get_config_region23 - Get sli4 port region 23 data.
 * @phba: pointer to lpfc hba data structure.
 * @rgn23_data: pointer to configure region 23 data.
 *
 * This function gets SLI4 port configure region 23 data through memory dump
 * mailbox command. When it successfully retrieves data, the size of the data
 * will be returned, otherwise, 0 will be returned.
 **/
static uint32_t
lpfc_sli4_get_config_region23(struct lpfc_hba *phba, char *rgn23_data)
{
	LPFC_MBOXQ_t *mboxq = NULL;
	struct lpfc_dmabuf *mp = NULL;
	struct lpfc_mqe *mqe;
	uint32_t data_length = 0;
	int rc;

	if (!rgn23_data)
		return 0;

	mboxq = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mboxq) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3105 failed to allocate mailbox memory\n");
		return 0;
	}

	if (lpfc_sli4_dump_cfg_rg23(phba, mboxq))
		goto out;
	mqe = &mboxq->u.mqe;
	mp = (struct lpfc_dmabuf *) mboxq->context1;
	rc = lpfc_sli_issue_mbox(phba, mboxq, MBX_POLL);
	if (rc)
		goto out;
	data_length = mqe->un.mb_words[5];
	if (data_length == 0)
		goto out;
	if (data_length > DMP_RGN23_SIZE) {
		data_length = 0;
		goto out;
	}
	lpfc_sli_pcimem_bcopy((char *)mp->virt, rgn23_data, data_length);
out:
	mempool_free(mboxq, phba->mbox_mem_pool);
	if (mp) {
		lpfc_mbuf_free(phba, mp->virt, mp->phys);
		kfree(mp);
	}
	return data_length;
}

/**
 * lpfc_sli_read_link_ste - Read region 23 to decide if link is disabled.
 * @phba: pointer to lpfc hba data structure.
 *
 * This function read region 23 and parse TLV for port status to
 * decide if the user disaled the port. If the TLV indicates the
 * port is disabled, the hba_flag is set accordingly.
 **/
void
lpfc_sli_read_link_ste(struct lpfc_hba *phba)
{
	uint8_t *rgn23_data = NULL;
	uint32_t if_type, data_size, sub_tlv_len, tlv_offset;
	uint32_t offset = 0;

	/* Get adapter Region 23 data */
	rgn23_data = kzalloc(DMP_RGN23_SIZE, GFP_KERNEL);
	if (!rgn23_data)
		goto out;

	if (phba->sli_rev < LPFC_SLI_REV4)
		data_size = lpfc_sli_get_config_region23(phba, rgn23_data);
	else {
		if_type = bf_get(lpfc_sli_intf_if_type,
				 &phba->sli4_hba.sli_intf);
		if (if_type == LPFC_SLI_INTF_IF_TYPE_0)
			goto out;
		data_size = lpfc_sli4_get_config_region23(phba, rgn23_data);
	}

	if (!data_size)
		goto out;

	/* Check the region signature first */
	if (memcmp(&rgn23_data[offset], LPFC_REGION23_SIGNATURE, 4)) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2619 Config region 23 has bad signature\n");
			goto out;
	}
	offset += 4;

	/* Check the data structure version */
	if (rgn23_data[offset] != LPFC_REGION23_VERSION) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
			"2620 Config region 23 has bad version\n");
		goto out;
	}
	offset += 4;

	/* Parse TLV entries in the region */
	while (offset < data_size) {
		if (rgn23_data[offset] == LPFC_REGION23_LAST_REC)
			break;
		/*
		 * If the TLV is not driver specific TLV or driver id is
		 * not linux driver id, skip the record.
		 */
		if ((rgn23_data[offset] != DRIVER_SPECIFIC_TYPE) ||
		    (rgn23_data[offset + 2] != LINUX_DRIVER_ID) ||
		    (rgn23_data[offset + 3] != 0)) {
			offset += rgn23_data[offset + 1] * 4 + 4;
			continue;
		}

		/* Driver found a driver specific TLV in the config region */
		sub_tlv_len = rgn23_data[offset + 1] * 4;
		offset += 4;
		tlv_offset = 0;

		/*
		 * Search for configured port state sub-TLV.
		 */
		while ((offset < data_size) &&
			(tlv_offset < sub_tlv_len)) {
			if (rgn23_data[offset] == LPFC_REGION23_LAST_REC) {
				offset += 4;
				tlv_offset += 4;
				break;
			}
			if (rgn23_data[offset] != PORT_STE_TYPE) {
				offset += rgn23_data[offset + 1] * 4 + 4;
				tlv_offset += rgn23_data[offset + 1] * 4 + 4;
				continue;
			}

			/* This HBA contains PORT_STE configured */
			if (!rgn23_data[offset + 2])
				phba->hba_flag |= LINK_DISABLED;

			goto out;
		}
	}

out:
	kfree(rgn23_data);
	return;
}

/**
 * lpfc_wr_object - write an object to the firmware
 * @phba: HBA structure that indicates port to create a queue on.
 * @dmabuf_list: list of dmabufs to write to the port.
 * @size: the total byte value of the objects to write to the port.
 * @offset: the current offset to be used to start the transfer.
 *
 * This routine will create a wr_object mailbox command to send to the port.
 * the mailbox command will be constructed using the dma buffers described in
 * @dmabuf_list to create a list of BDEs. This routine will fill in as many
 * BDEs that the imbedded mailbox can support. The @offset variable will be
 * used to indicate the starting offset of the transfer and will also return
 * the offset after the write object mailbox has completed. @size is used to
 * determine the end of the object and whether the eof bit should be set.
 *
 * Return 0 is successful and offset will contain the the new offset to use
 * for the next write.
 * Return negative value for error cases.
 **/
int
lpfc_wr_object(struct lpfc_hba *phba, struct list_head *dmabuf_list,
	       uint32_t size, uint32_t *offset)
{
	struct lpfc_mbx_wr_object *wr_object;
	LPFC_MBOXQ_t *mbox;
	int rc = 0, i = 0;
	uint32_t shdr_status, shdr_add_status;
	uint32_t mbox_tmo;
	union lpfc_sli4_cfg_shdr *shdr;
	struct lpfc_dmabuf *dmabuf;
	uint32_t written = 0;

	mbox = mempool_alloc(phba->mbox_mem_pool, GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	lpfc_sli4_config(phba, mbox, LPFC_MBOX_SUBSYSTEM_COMMON,
			LPFC_MBOX_OPCODE_WRITE_OBJECT,
			sizeof(struct lpfc_mbx_wr_object) -
			sizeof(struct lpfc_sli4_cfg_mhdr), LPFC_SLI4_MBX_EMBED);

	wr_object = (struct lpfc_mbx_wr_object *)&mbox->u.mqe.un.wr_object;
	wr_object->u.request.write_offset = *offset;
	sprintf((uint8_t *)wr_object->u.request.object_name, "/");
	wr_object->u.request.object_name[0] =
		cpu_to_le32(wr_object->u.request.object_name[0]);
	bf_set(lpfc_wr_object_eof, &wr_object->u.request, 0);
	list_for_each_entry(dmabuf, dmabuf_list, list) {
		if (i >= LPFC_MBX_WR_CONFIG_MAX_BDE || written >= size)
			break;
		wr_object->u.request.bde[i].addrLow = putPaddrLow(dmabuf->phys);
		wr_object->u.request.bde[i].addrHigh =
			putPaddrHigh(dmabuf->phys);
		if (written + SLI4_PAGE_SIZE >= size) {
			wr_object->u.request.bde[i].tus.f.bdeSize =
				(size - written);
			written += (size - written);
			bf_set(lpfc_wr_object_eof, &wr_object->u.request, 1);
		} else {
			wr_object->u.request.bde[i].tus.f.bdeSize =
				SLI4_PAGE_SIZE;
			written += SLI4_PAGE_SIZE;
		}
		i++;
	}
	wr_object->u.request.bde_count = i;
	bf_set(lpfc_wr_object_write_length, &wr_object->u.request, written);
	if (!phba->sli4_hba.intr_enable)
		rc = lpfc_sli_issue_mbox(phba, mbox, MBX_POLL);
	else {
		mbox_tmo = lpfc_mbox_tmo_val(phba, mbox);
		rc = lpfc_sli_issue_mbox_wait(phba, mbox, mbox_tmo);
	}
	/* The IOCTL status is embedded in the mailbox subheader. */
	shdr = (union lpfc_sli4_cfg_shdr *) &wr_object->header.cfg_shdr;
	shdr_status = bf_get(lpfc_mbox_hdr_status, &shdr->response);
	shdr_add_status = bf_get(lpfc_mbox_hdr_add_status, &shdr->response);
	if (rc != MBX_TIMEOUT)
		mempool_free(mbox, phba->mbox_mem_pool);
	if (shdr_status || shdr_add_status || rc) {
		lpfc_printf_log(phba, KERN_ERR, LOG_INIT,
				"3025 Write Object mailbox failed with "
				"status x%x add_status x%x, mbx status x%x\n",
				shdr_status, shdr_add_status, rc);
		rc = -ENXIO;
	} else
		*offset += wr_object->u.response.actual_write_length;
	return rc;
}

/**
 * lpfc_cleanup_pending_mbox - Free up vport discovery mailbox commands.
 * @vport: pointer to vport data structure.
 *
 * This function iterate through the mailboxq and clean up all REG_LOGIN
 * and REG_VPI mailbox commands associated with the vport. This function
 * is called when driver want to restart discovery of the vport due to
 * a Clear Virtual Link event.
 **/
void
lpfc_cleanup_pending_mbox(struct lpfc_vport *vport)
{
	struct lpfc_hba *phba = vport->phba;
	LPFC_MBOXQ_t *mb, *nextmb;
	struct lpfc_dmabuf *mp;
	struct lpfc_nodelist *ndlp;
	struct lpfc_nodelist *act_mbx_ndlp = NULL;
	struct Scsi_Host  *shost = lpfc_shost_from_vport(vport);
	LIST_HEAD(mbox_cmd_list);
	uint8_t restart_loop;

	/* Clean up internally queued mailbox commands with the vport */
	spin_lock_irq(&phba->hbalock);
	list_for_each_entry_safe(mb, nextmb, &phba->sli.mboxq, list) {
		if (mb->vport != vport)
			continue;

		if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) &&
			(mb->u.mb.mbxCommand != MBX_REG_VPI))
			continue;

		list_del(&mb->list);
		list_add_tail(&mb->list, &mbox_cmd_list);
	}
	/* Clean up active mailbox command with the vport */
	mb = phba->sli.mbox_active;
	if (mb && (mb->vport == vport)) {
		if ((mb->u.mb.mbxCommand == MBX_REG_LOGIN64) ||
			(mb->u.mb.mbxCommand == MBX_REG_VPI))
			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
		if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
			act_mbx_ndlp = (struct lpfc_nodelist *)mb->context2;
			/* Put reference count for delayed processing */
			act_mbx_ndlp = lpfc_nlp_get(act_mbx_ndlp);
			/* Unregister the RPI when mailbox complete */
			mb->mbox_flag |= LPFC_MBX_IMED_UNREG;
		}
	}
	/* Cleanup any mailbox completions which are not yet processed */
	do {
		restart_loop = 0;
		list_for_each_entry(mb, &phba->sli.mboxq_cmpl, list) {
			/*
			 * If this mailox is already processed or it is
			 * for another vport ignore it.
			 */
			if ((mb->vport != vport) ||
				(mb->mbox_flag & LPFC_MBX_IMED_UNREG))
				continue;

			if ((mb->u.mb.mbxCommand != MBX_REG_LOGIN64) &&
				(mb->u.mb.mbxCommand != MBX_REG_VPI))
				continue;

			mb->mbox_cmpl = lpfc_sli_def_mbox_cmpl;
			if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
				ndlp = (struct lpfc_nodelist *)mb->context2;
				/* Unregister the RPI when mailbox complete */
				mb->mbox_flag |= LPFC_MBX_IMED_UNREG;
				restart_loop = 1;
				spin_unlock_irq(&phba->hbalock);
				spin_lock(shost->host_lock);
				ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
				spin_unlock(shost->host_lock);
				spin_lock_irq(&phba->hbalock);
				break;
			}
		}
	} while (restart_loop);

	spin_unlock_irq(&phba->hbalock);

	/* Release the cleaned-up mailbox commands */
	while (!list_empty(&mbox_cmd_list)) {
		list_remove_head(&mbox_cmd_list, mb, LPFC_MBOXQ_t, list);
		if (mb->u.mb.mbxCommand == MBX_REG_LOGIN64) {
			mp = (struct lpfc_dmabuf *) (mb->context1);
			if (mp) {
				__lpfc_mbuf_free(phba, mp->virt, mp->phys);
				kfree(mp);
			}
			ndlp = (struct lpfc_nodelist *) mb->context2;
			mb->context2 = NULL;
			if (ndlp) {
				spin_lock(shost->host_lock);
				ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
				spin_unlock(shost->host_lock);
				lpfc_nlp_put(ndlp);
			}
		}
		mempool_free(mb, phba->mbox_mem_pool);
	}

	/* Release the ndlp with the cleaned-up active mailbox command */
	if (act_mbx_ndlp) {
		spin_lock(shost->host_lock);
		act_mbx_ndlp->nlp_flag &= ~NLP_IGNR_REG_CMPL;
		spin_unlock(shost->host_lock);
		lpfc_nlp_put(act_mbx_ndlp);
	}
}

/**
 * lpfc_drain_txq - Drain the txq
 * @phba: Pointer to HBA context object.
 *
 * This function attempt to submit IOCBs on the txq
 * to the adapter.  For SLI4 adapters, the txq contains
 * ELS IOCBs that have been deferred because the there
 * are no SGLs.  This congestion can occur with large
 * vport counts during node discovery.
 **/

uint32_t
lpfc_drain_txq(struct lpfc_hba *phba)
{
	LIST_HEAD(completions);
	struct lpfc_sli_ring *pring = &phba->sli.ring[LPFC_ELS_RING];
	struct lpfc_iocbq *piocbq = 0;
	unsigned long iflags = 0;
	char *fail_msg = NULL;
	struct lpfc_sglq *sglq;
	union lpfc_wqe wqe;
	int txq_cnt = 0;

	spin_lock_irqsave(&phba->hbalock, iflags);
	list_for_each_entry(piocbq, &pring->txq, list) {
		txq_cnt++;
	}

	if (txq_cnt > pring->txq_max)
		pring->txq_max = txq_cnt;

	spin_unlock_irqrestore(&phba->hbalock, iflags);

	while (!list_empty(&pring->txq)) {
		spin_lock_irqsave(&phba->hbalock, iflags);

		piocbq = lpfc_sli_ringtx_get(phba, pring);
		if (!piocbq) {
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
				"2823 txq empty and txq_cnt is %d\n ",
				txq_cnt);
			break;
		}
		sglq = __lpfc_sli_get_sglq(phba, piocbq);
		if (!sglq) {
			__lpfc_sli_ringtx_put(phba, pring, piocbq);
			spin_unlock_irqrestore(&phba->hbalock, iflags);
			break;
		}
		txq_cnt--;

		/* The xri and iocb resources secured,
		 * attempt to issue request
		 */
		piocbq->sli4_lxritag = sglq->sli4_lxritag;
		piocbq->sli4_xritag = sglq->sli4_xritag;
		if (NO_XRI == lpfc_sli4_bpl2sgl(phba, piocbq, sglq))
			fail_msg = "to convert bpl to sgl";
		else if (lpfc_sli4_iocb2wqe(phba, piocbq, &wqe))
			fail_msg = "to convert iocb to wqe";
		else if (lpfc_sli4_wq_put(phba->sli4_hba.els_wq, &wqe))
			fail_msg = " - Wq is full";
		else
			lpfc_sli_ringtxcmpl_put(phba, pring, piocbq);

		if (fail_msg) {
			/* Failed means we can't issue and need to cancel */
			lpfc_printf_log(phba, KERN_ERR, LOG_SLI,
					"2822 IOCB failed %s iotag 0x%x "
					"xri 0x%x\n",
					fail_msg,
					piocbq->iotag, piocbq->sli4_xritag);
			list_add_tail(&piocbq->list, &completions);
		}
		spin_unlock_irqrestore(&phba->hbalock, iflags);
	}

	/* Cancel all the IOCBs that cannot be issued */
	lpfc_sli_cancel_iocbs(phba, &completions, IOSTAT_LOCAL_REJECT,
				IOERR_SLI_ABORTED);

	return txq_cnt;
}
