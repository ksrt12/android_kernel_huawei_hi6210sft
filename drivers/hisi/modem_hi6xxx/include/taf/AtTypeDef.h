/******************************************************************************

                  版权所有 (C), 2001-2013, 华为技术有限公司

 ******************************************************************************
  文 件 名   : AtTypeDef.h
  版 本 号   : 初稿
  作    者   :
  生成日期   : 2013年4月28日
  最近修改   :
  功能描述   : AT公共的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2013年4月28日
    作    者   :
    修改内容   : 创建文件

******************************************************************************/

#ifndef __ATTPYEDEF_H__
#define __ATTPYEDEF_H__

/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"
#include "om.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/
#ifndef __AT_DISABLE_OM__
#define AT_INFO_LOG(String)            PS_LOG(WUEPS_PID_AT, 0, PS_PRINT_INFO, String)
#define AT_NORM_LOG(String)            PS_LOG(WUEPS_PID_AT, 0, PS_PRINT_NORMAL, String)
#define AT_NORM_LOG1(String,Para1)     PS_LOG1(WUEPS_PID_AT, 0, PS_PRINT_NORMAL, String, (TAF_INT32)Para1)
#define AT_WARN_LOG(String)            PS_LOG(WUEPS_PID_AT, 0, PS_PRINT_WARNING, String)
    /* Added by f62575 for AT Project 2011-10-04,  Begin */
#define AT_WARN_LOG1(String,Para1)     PS_LOG1(WUEPS_PID_AT, 0, PS_PRINT_WARNING, String, (TAF_INT32)Para1)
    /* Added by f62575 for AT Project 2011-10-04,  End */
#define AT_ERR_LOG(String)             PS_LOG(WUEPS_PID_AT, 0, PS_PRINT_ERROR, String)
    /* Added by A00165503 for AT Project, 2011-10-06, begin */
#define AT_ERR_LOG1(String, Para1)     PS_LOG1(WUEPS_PID_AT, 0, PS_PRINT_ERROR, String, (VOS_INT32)Para1)
    /* Added by A00165503 for AT Project, 2011-10-06, end */
#define AT_LOG1(String, Para1)         PS_LOG1(WUEPS_PID_AT, 0, PS_PRINT_INFO, String, (TAF_INT32)Para1)
#else
#define AT_INFO_LOG(String)
#define AT_NORM_LOG(String)
#define AT_WARN_LOG(String)
#define AT_ERR_LOG(String)
#define AT_LOG1(String, Para1)
#endif

/*****************************************************************************
  3 枚举定义
*****************************************************************************/
/*================================USER TYPE Begin================================*/
/*****************************************************************************
 枚举名    : AT_USER_TYPE_ENUM
 枚举说明  : AT用户类型
 1.日    期   : 2012年11月30日
   作    者   : l00227485
   修改内容   : 创建
*****************************************************************************/
enum AT_USER_TYPE_ENUM
{
    AT_USBCOM_USER                      = 0,
    AT_COM_USER                         = 3,
    AT_MODEM_USER                       = 4,
    AT_IPCCOM_USER                      = 6,
    AT_NDIS_USER                        = 7,
    AT_CTR_USER                         = 8,
    AT_APP_USER                         = 9,
    AT_SOCK_USER                        = 10,
    AT_APP_SOCK_USER                    = 11,
    AT_UART_USER                        = 12,
    AT_HSIC1_USER                       = 13,
    AT_HSIC2_USER                       = 14,
    AT_HSIC3_USER                       = 15,

    /* Added by L47619 for V7R1C50 A-GPS Project, 2012/06/28, begin */
    AT_HSIC4_USER                       = 16,
    /* Added by L47619 for V7R1C50 A-GPS Project, 2012/06/28, end */

    /* Added by L60609 for MUX，2012-08-03,  Begin */
    AT_MUX1_USER                        = 17,
    AT_MUX2_USER                        = 18,
    AT_MUX3_USER                        = 19,
    AT_MUX4_USER                        = 20,
    AT_MUX5_USER                        = 21,
    AT_MUX6_USER                        = 22,
    AT_MUX7_USER                        = 23,
    AT_MUX8_USER                        = 24,
    /* Added by L60609 for MUX，2012-08-03,  End */

    /* Added by A00165503 for UART-MODEM Project, 2013-11-12, Begin */
    AT_HSUART_USER                      = 25,
    /* Added by A00165503 for UART-MODEM Project, 2013-11-12, End */

    AT_BUTT_USER                        = 0xff
};
typedef VOS_UINT8 AT_USER_TYPE;

/*================================USER TYPE End================================*/

typedef VOS_UINT8 AT_MODE_TYPE;
/* Modified by j00174725 for UART-MODEM Project, 2013-09-21, begin */
#define AT_CMD_MODE                     (0)
#define AT_DATA_MODE                    (1)
#define AT_ONLINE_CMD_MODE              (2)
/* Modified by j00174725 for UART-MODEM Project, 2013-09-21, end */

typedef VOS_UINT8 AT_IND_MODE_TYPE;
#define AT_IND_MODE                     (0)
#define AT_NO_IND_MODE                  (1)

/*****************************************************************************
  4 全局变量声明
*****************************************************************************/


/*****************************************************************************
  5 消息头定义
*****************************************************************************/


/*****************************************************************************
  6 消息定义
*****************************************************************************/


/*****************************************************************************
  7 STRUCT定义
*****************************************************************************/


/*****************************************************************************
  8 UNION定义
*****************************************************************************/


/*****************************************************************************
  9 OTHERS定义
*****************************************************************************/

/*****************************************************************************
  10 函数声明
*****************************************************************************/

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

#endif /* end of AtTypeDef.h */
