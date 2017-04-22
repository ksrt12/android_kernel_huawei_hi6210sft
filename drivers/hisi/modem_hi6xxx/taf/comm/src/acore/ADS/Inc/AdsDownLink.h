/******************************************************************************

                  ��Ȩ���� (C), 2001-2011, ��Ϊ�������޹�˾

 ******************************************************************************
  �� �� ��   : AdsDownLink.h
  �� �� ��   : ����
  ��    ��   : ³��/l60609
  ��������   : 2011��12��7��
  ����޸�   :
  ��������   : AdsDlProcData.c ��ͷ�ļ�
  �����б�   :
  �޸���ʷ   :
  1.��    ��   : 2011��12��7��
    ��    ��   : ³��/l60609
    �޸�����   : �����ļ�

******************************************************************************/

#ifndef __ADSDOWNLINK_H__
#define __ADSDOWNLINK_H__

/*****************************************************************************
  1 ����ͷ�ļ�����
*****************************************************************************/
#include "AdsCtx.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


#pragma pack(4)

/*****************************************************************************
  2 �궨��
*****************************************************************************/
/* ����ADS����IP TYPE */
#define ADS_DL_IP_TYPE_FLAG                     (0x0200)

/* bd_cd_noeqBD��len��CD�ĳ��Ȳ��ȴ�����ʾ */
#define ADS_DL_IPF_RD_RSLT_BDCD_LEN_ERR_FLG     (0x4000)    /* 0100 0000 0000 0000 B */

/* pkt_parse_err���ݽ�������ָʾ */
#define ADS_DL_IPF_RD_RSLT_PKT_PARSE_ERR_FLG    (0x2000)    /* 0010 0000 0000 0000 B */

/* bd_pkt_noeqBD��len��IP��ͷָʾ��len���ȴ���ָʾ */
#define ADS_DL_IPF_RD_RSLT_BD_PKT_LEN_ERR_FLG   (0x1000)    /* 0001 0000 0000 0000 B */

/* head_len_err IPV4���ȴ���ָʾ�ź� */
#define ADS_DL_IPF_RD_RSLT_HDR_LEN_ERR_FLG      (0x0800)    /* 0000 1000 0000 0000 B */

/* version_err�汾�Ŵ���ָʾ */
#define ADS_DL_IPF_RD_RSLT_VER_ERR_FLG          (0x0400)    /* 0000 0100 0000 0000 B */

/* ����IPF BEARID ������־λ */
#define ADS_DL_IPF_BEARID_ERR_PKT_FLAG          (ADS_DL_IPF_RD_RSLT_BDCD_LEN_ERR_FLG |  \
                                                 ADS_DL_IPF_RD_RSLT_PKT_PARSE_ERR_FLG | \
                                                 ADS_DL_IPF_RD_RSLT_HDR_LEN_ERR_FLG |   \
                                                 ADS_DL_IPF_RD_RSLT_VER_ERR_FLG)

/* Added by A00165503 for DTS2012020708222, 2011-04-12, Begin */
/* �����RD Result, �Ƿ�Ϊ�쳣Packet */
#define ADS_DL_IPF_RD_RSLT_IS_ERR_PKT(usRslt)       ((usRslt) & ADS_DL_IPF_BEARID_ERR_PKT_FLAG)

/* Added by l60609 for V3R3 PPP RPOJECT 2013-06-07, Begin */
/* �����RD Result, �Ƿ�ΪVersion���� */
#define ADS_DL_IPF_RD_RSLT_IS_VER_ERR_PKT(usRslt)   ((usRslt) & ADS_DL_IPF_RD_RSLT_VER_ERR_FLG)
/* Added by l60609 for V3R3 PPP RPOJECT 2013-06-07, End */

/* ��RD Out���ȡ���ƺ�����ݰ� */
#if(FEATURE_OFF == FEATURE_SKB_EXP)
#define ADS_DL_GET_DATA_FROM_IPF_OUT(ulOut)         (phys_to_virt(ulOut))
#else
#define ADS_DL_GET_DATA_FROM_IPF_OUT(ulOut)         ((VOS_UINT8*)(ulOut))
#endif

/* ��RD PktLen���ȡ���ƺ�����ݰ����� */
#define ADS_DL_GET_LEN_FROM_IPF_PKT_LEN(usPktLen)   ((VOS_UINT16)(usPktLen))

/* ��RD User Field 1���ȡC��TTF Memory */
#define ADS_DL_GET_TTFMEM_FROM_IPF_USR2(ulUsr2)     ((VOS_VOID*)(ulUsr2))

/* ��RD Result���ȡC��IP������ */
#define ADS_DL_GET_IP_TYPE_FROM_IPF_RSLT(usRslt)    ((ADS_PKT_TYPE_ENUM_UINT8)(((ADS_DL_IPF_RESULT_STRU*)&(usRslt))->usIpType))
/* Added by A00165503 for DTS2012020708222, 2011-04-12, End */

/* Modified by l60609 for DSDA Phase II, 2012-11-24, Begins */
/* RD��user field 1����1byteΪModem id����1byteΪRab Id����ȡMODEM ID */
#define ADS_DL_GET_MODEM_ID_FROM_RD_USER_FIELD1(i)  ((i >> 8) & 0xFF)

/* RD��user field 1����1byteΪModem id����1byteΪRab Id����ȡRAB ID */
#define ADS_DL_GET_RAB_ID_FROM_RD_USER_FIELD1(i)    (i & 0xFFU)

/* ����AD0���ݰ��ĳ��� */
#define ADS_DL_AD0_DATA_LEN                          (448)

/* ����AD1���ݰ��ĳ��� */
#define ADS_DL_AD1_DATA_LEN                          (1536 + 14)
/* Modified by l60609 for DSDA Phase II, 2012-11-24, End */

/*****************************************************************************
  3 ö�ٶ���
*****************************************************************************/


/*****************************************************************************
  4 ȫ�ֱ�������
*****************************************************************************/


/*****************************************************************************
  5 ��Ϣͷ����
*****************************************************************************/


/*****************************************************************************
  6 ��Ϣ����
*****************************************************************************/


/*****************************************************************************
  7 STRUCT����
*****************************************************************************/

/* Added by A00165503 for DTS2012020708222, 2011-04-12, Begin */
/*****************************************************************************
 �ṹ��    : IPF_RESULT
 �ṹ˵��  : Ӳ��Ip���˽���Ľṹ

  1.��    ��   : 2012��4��12��
    ��    ��   : A00165503
    ��    ��   : Ӳ��Ip���˽���Ľṹ
*****************************************************************************/
typedef struct
{
    VOS_UINT16                          usBearedId:6;
    VOS_UINT16                          usIpSegInfo:2;
    VOS_UINT16                          usUpperLayer:1;
    VOS_UINT16                          usIpType:1;
    VOS_UINT16                          usVerErr:1;
    VOS_UINT16                          usHdrLenErr:1;
    VOS_UINT16                          usPktLenNotEq:1;
    VOS_UINT16                          usPktLenErr:1;
    VOS_UINT16                          usBdCdNotEq:1;
    VOS_UINT16                          usResv:1;
} ADS_DL_IPF_RESULT_STRU;
/* Added by A00165503 for DTS2012020708222, 2011-04-12, End */

/*****************************************************************************
�ṹ��    : ADS_DL_RD_STRU
�ṹ˵��  : ADS��������RD��Ϣ

  1.��    ��   : 2011��12��6��
    ��    ��   : ³��/l60609
    �޸�����   : ����ADS��������RD��Ϣ
*****************************************************************************/
typedef struct
{
    VOS_UINT8                            ucRabId;                               /* Rab Id*/
    ADS_PKT_TYPE_ENUM_UINT8              enPktType;                             /* Pkt Type*/
    VOS_UINT8                            aucRsv[6];                             /* ���� */
    CDS_ADS_DL_IPF_BEARER_ID_ENUM_UINT32 enBearId;                              /* Bear Id*/
    VOS_UINT32                           ulLen;                                 /* ���ƺ�����ݰ����� */
    VOS_UINT8                           *pucData;                               /* ���ƺ�����ݰ�ָ�� */
    VOS_VOID                            *pTtfMem;                               /* Ŀ��TTFָ��*/
} ADS_DL_RD_STRU;



/*****************************************************************************
  8 UNION����
*****************************************************************************/


/*****************************************************************************
  9 OTHERS����
*****************************************************************************/


/*****************************************************************************
  10 ��������
*****************************************************************************/

VOS_VOID ADS_DL_ProcMsg(MsgBlock* pMsg);
VOS_VOID ADS_DL_ProcRd(
    IPF_RD_DESC_S                      *pstRdDesc
);
VOS_UINT32 ADS_DL_RcvAtMsg(MsgBlock* pMsg);
/* Modified by m00217266 for L-C��������Ŀ, 2014-1-23, begin */
VOS_UINT32 ADS_DL_RcvTafPdpStatusInd(MsgBlock *pMsg);
/* Modified by m00217266 for L-C��������Ŀ, 2014-1-23, end */
VOS_UINT32 ADS_DL_RcvCdsMsg(MsgBlock *pMsg);
VOS_INT32 ADS_DL_IpfIntCB(VOS_VOID);
VOS_VOID ADS_DL_SendNdClientDataInd(
    IPF_RD_DESC_S                      *pstRdDesc
);
#if 0
VOS_VOID ADS_DL_SendNdClientErrInd(
    IPF_RD_DESC_S                      *pstRdDesc
);
#endif

VOS_VOID ADS_DL_ProcIpfResult(VOS_VOID);
/* Added by A00165503 for DTS2012071003092, 2012-07-17, Begin */
VOS_VOID ADS_DL_RcvTiProtectExpired(
    VOS_UINT32                          ulParam,
    VOS_UINT32                          ulTimerName
);
/* Added by A00165503 for DTS2012071003092, 2012-07-17, End */
/* Added by A00165503 for DTS2012112206761, 2012-11-19, Begin */
VOS_UINT32 ADS_DL_IsFcAssemTuneNeeded(VOS_UINT32 ulRdNum);
/* Added by A00165503 for DTS2012112206761, 2012-11-19, End */

/* Added by l60609 for DSDA Phase II, 2012-12-20, Begin */
#if (FEATURE_OFF == FEATURE_SKB_EXP)
VOS_INT32 ADS_DL_IpfAdqEmptyCB(IPF_ADQ_EMPTY_E eAdqEmpty);
VOS_VOID ADS_DL_ProcIpfAdqEmtpyEvent(VOS_VOID);
VOS_VOID ADS_DL_RcvTiAdqEmptyExpired(
    VOS_UINT32                          ulParam,
    VOS_UINT32                          ulTimerName
);
VOS_VOID ADS_DL_ConfigAdq(
    IPF_AD_TYPE_E                       enAdType,
    VOS_UINT                            ulIpfAdNum,
    VOS_UINT                           *pulActAdNum
);
VOS_VOID ADS_DL_ProcAdq(VOS_VOID);
VOS_VOID ADS_DL_InitAdq(VOS_VOID);
#endif
/* Added by l60609 for DSDA Phase II, 2012-12-20, End */

/* Added by f00179208 for CCPU RESET, 2013-04-15, Begin */
VOS_UINT32 ADS_DL_RcvCcpuResetStartInd(
    MsgBlock                           *pstMsg
);
/* Added by f00179208 for CCPU RESET, 2013-04-15, End */
/* Added by f00179208 for DTS2013091408561, 2013-09-14, Begin */
VOS_UINT32 ADS_DL_RcvCcpuResetEndInd(
    MsgBlock                           *pstMsg
);
/* Added by f00179208 for DTS2013091408561, 2013-09-14, End */
VOS_VOID ADS_DL_FreeIpfUsedAd0(VOS_VOID);
VOS_VOID ADS_DL_FreeIpfUsedAd1(VOS_VOID);

#if (VOS_OS_VER == VOS_WIN32)
#pragma pack()
#else
#pragma pack(0)
#endif




#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of AdsDlProcData.h */