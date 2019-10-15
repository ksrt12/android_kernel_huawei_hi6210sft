#ifndef MMC_HEALTH_DIAG_H
#define MMC_HEALTH_DIAG_H



#define MAX_ERR_TIMES 5
#define MAX_ERR_TYPE 5
#define PRT_TIME_PERIOD 10000000000  //10s
#define SD_IO_BUSY 95
#define MAX_REPORT_TIMES 10
#define LOW_SPEED_WARTING_VALUE 512

/* BEGIN PN:DTS2016032808027, Modified by d00168349, 2016/04/11 */
#define MAX_WRITE_SPEED_ABNOR_TIMES 4
/* END PN:DTS2016032808027, Modified by d00168349, 2016/04/11 */

enum MMC_BLK_REPORT_ERR
{
   MMC_BLK_STUCK_IN_PRG_ERR = 0x20,
   MMC_BLK_WR_SPEED_ERR,
   MMC_BLK_RW_CHECK_ERR,
   MMC_RO_CHECK_ERR,
   MMC_RO_ERR,
};

struct mmc_blk_err_report_info
{
   unsigned int err_num;
   char err_info[12];
};



void mmc_diag_sd_health_status(struct gendisk *disk,int status);
unsigned int mmc_calculate_ioworkload_and_rwspeed(unsigned long long time,struct request *rqc,struct gendisk *disk);
int mmc_calculate_rw_size(unsigned long long time,unsigned int rq_byte,struct request *rqc);
void mmc_clear_report_info();
int mmc_get_rw_status(int status);
int mmc_trigger_ro_check(struct request *rqc,struct gendisk *disk,unsigned int read_only);
extern unsigned int mmc_get_sd_speed();
#endif