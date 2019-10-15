/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : AtInit.h
  版 本 号   : 初稿
  作    者   : 鲁琳/l60609
  生成日期   : 2012年1月22日
  最近修改   :
  功能描述   : AtInit.c 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2012年1月22日
    作    者   : 鲁琳/l60609
    修改内容   : 创建文件

******************************************************************************/

#ifndef __ATINIT_H__
#define __ATINIT_H__

/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"
#include "AtCtx.h"
#include "AtInputProc.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/
/* 多时隙等级最大宏定义 */
#define AT_GAS_GRR_MULTISLOT_CLASS_MAX  (12)


/*****************************************************************************
  3 枚举定义
*****************************************************************************/

/*****************************************************************************
 枚举名    : AT_GAS_HIGH_MULTISLOT_CLASS_ENUM
 枚举说明  : High multislot class等级
 修改历史  :
 1.日    期    : 2011年12月13日
   作    者    : C00173809
   修改内容    : 新建枚举类型, for V7R1 GTL PhaseI
*****************************************************************************/
enum AT_GAS_HIGH_MULTISLOT_CLASS_ENUM
{
    AT_GAS_HIGH_MULTISLOT_CLASS_0,
    AT_GAS_HIGH_MULTISLOT_CLASS_1,
    AT_GAS_HIGH_MULTISLOT_CLASS_2,
    AT_GAS_HIGH_MULTISLOT_CLASS_3,

    AT_GAS_HIGH_MULTISLOT_CLASS_BUTT
};
typedef VOS_UINT8 AT_GAS_HIGH_MULTISLOT_CLASS_ENUM_UINT8;


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
 结构名    : AT_NV_UE_CAPABILITY_STRU
 协议表格  : 无
 结构说明  : 存储在NV中的UE能力信息
*****************************************************************************/
typedef struct
{
    VOS_UINT8                           aucReserved1[11];                       /* 留作以后扩展 */
    VOS_UINT8                           ucAsRelIndicator;                       /* Access Stratum Release Indicator             */

    VOS_UINT8                           ucHSDSCHSupport;                        /* 是否支持enHSDSCHSupport的标志                */
    VOS_UINT8                           ucHSDSCHPhyCategory;                    /* 支持HS-DSCH物理层的类型标志                  */

    VOS_UINT8                           ucMacEhsSupport;
    VOS_UINT8                           ucHSDSCHPhyCategoryExt;
    VOS_UINT8                           ucMultiCellSupport;                     /* 是否支持 Multi cell support,如果支持MultiCell,Ex2存在 */
    VOS_UINT8                           ucHSDSCHPhyCategoryExt2;                /* HS-DSCH physical layer category extension 2 */

    VOS_UINT8                           aucReserved2[17];                       /* 留作以后扩展 */

    VOS_UINT8                           ucHSDSCHPhyCategoryExt3;                /* HS-DSCH physical layer category extension 3 */
    VOS_UINT8                           ucDcMimoSupport;                        /* 是否支持DC+MIMO */

    VOS_UINT8                           aucReserved3[3];                        /* 留作以后扩展 */

} AT_NV_UE_CAPABILITY_STRU;

/* Deleted by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-10, begin */
/* Deleted by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-10, end */


/* Modified by z00161729 for cerssi, 2012-11-21, begin */
/*****************************************************************************
结构名    : AT_NV_CELL_SIGN_REPORT_CFG_STRU
结构说明  : 信号质量主动上报相关配置信息
1.日    期  : 2012年11月21日
  作    者  : z00161729
  修改内容  : 支持^cerssi新增结构
*****************************************************************************/
typedef struct
{
    AT_CERSSI_REPORT_TYPE_ENUM_UINT8    enSignThreshold;    /* 信号变化门限,当RSSI变化超过该值，接入层需要主动上报信号质量，取值0表示接入层按默认值处理 */
    /* Modified by t00212959 for DTS2013010809978, 2013-1-9, begin */
    VOS_UINT8                           ucMinRptTimerInterval;     /* 间隔上报的时间   */
    /* Modified by t00212959 for DTS2013010809978, 2013-1-9, end */
} AT_NV_CELL_SIGN_REPORT_CFG_STRU;
/* Modified by z00161729 for cerssi, 2012-11-21, end */

/*****************************************************************************
  8 UNION定义
*****************************************************************************/


/*****************************************************************************
  9 OTHERS定义
*****************************************************************************/


/*****************************************************************************
  10 函数声明
*****************************************************************************/
extern VOS_VOID AT_ReadPlatformNV(VOS_VOID);
extern VOS_VOID AT_ReadClientConfigNV(VOS_VOID);

extern VOS_VOID AT_GetCpmsMtMem(
           MODEM_ID_ENUM_UINT16                enModemId,
           MN_MSG_MEM_STORE_ENUM_U8           *penSmMemStore
       );
extern AT_CGSMS_SEND_DOMAIN_ENUM_U8 AT_SendDomainNvimToProto(
    VOS_UINT32                           ulNvimSendDomain
    );
extern VOS_VOID AT_ReadSmsSendDomainNV(VOS_VOID);
extern VOS_VOID AT_ReadSmsMeStorageInfoNV(VOS_VOID);
extern VOS_VOID AT_ReadSmsClass0TailorNV(VOS_VOID);
extern VOS_VOID AT_ReadSmsNV(VOS_VOID);

/* Deleted by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-3, begin */
/* Deleted by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-3, end */

extern VOS_VOID  AT_ReadRoamCapaNV(VOS_VOID);
extern VOS_VOID  AT_ReadSystemAppConfigNV(VOS_VOID);

extern VOS_VOID AT_ReadAtDislogPwdNV(VOS_VOID);
extern VOS_VOID AT_ReadAtRightPasswordNV(VOS_VOID);
extern VOS_VOID AT_ReadAtDissdPwdNV(VOS_VOID);
extern VOS_VOID AT_ReadNotSupportRetValueNV(VOS_VOID);
extern VOS_VOID AT_ReadE5NV(VOS_VOID);

extern VOS_UINT32 AT_IsAbortCmdCharValid(
    VOS_UINT8                          *pucAbortCmdChar,
    VOS_UINT32                          ulLen
    );
extern VOS_VOID AT_ReadAbortCmdParaNV(VOS_VOID);

extern VOS_VOID AT_ReadSysNV(VOS_VOID);

extern VOS_VOID AT_ReadCellSignReportCfgNV( VOS_VOID );

extern VOS_VOID AT_ReadPppDialErrCodeNV( VOS_VOID );
extern VOS_VOID AT_ReadReportRegActFlgNV( VOS_VOID );
extern VOS_VOID AT_ReadCregAndCgregCiFourByteRptNV( VOS_VOID );
#if (FEATURE_ON == FEATURE_IPV6)
extern VOS_VOID AT_ReadIpv6CapabilityNV( VOS_VOID );
#endif
extern VOS_VOID AT_ReadDialConnectDisplayRateNV(VOS_VOID);

extern VOS_VOID  AT_ReadPsNV(VOS_VOID);
extern VOS_VOID  AT_ReadCsNV(VOS_VOID);

#if (FEATURE_ON == FEATURE_LTE)
extern VOS_VOID AT_ReadLTENV(VOS_VOID);
#endif

extern VOS_UINT32 AT_ValidateWasCategory(
           AT_NV_UE_CAPABILITY_STRU           *pstUeCapability
       );
extern VOS_UINT32 AT_ValidateWasCategoryExt(
           AT_NV_UE_CAPABILITY_STRU           *pstUeCapability
       );
extern VOS_UINT32 AT_ValidateWasCategoryExt2(
           AT_NV_UE_CAPABILITY_STRU           *pstUeCapability
       );
extern VOS_UINT32 AT_ValidateWasCategoryExt3(
           AT_NV_UE_CAPABILITY_STRU           *pstUeCapability
       );
extern VOS_UINT8 AT_GetWasDefaultCategory(
    AT_NV_UE_CAPABILITY_STRU           *pstUeCapability
    );
extern VOS_UINT8 AT_CalcWasCategory(
                                    AT_NV_UE_CAPABILITY_STRU           *pstUeCapability
                                    );

extern VOS_VOID AT_ReadWasCapabilityNV(VOS_VOID);
extern VOS_VOID AT_ReadGasCapabilityNV(VOS_VOID);

extern VOS_UINT32  AT_ReadPhyNV(VOS_VOID);

extern VOS_VOID  AT_ReadNV(VOS_VOID);

extern VOS_VOID AT_InitDeviceCmd(VOS_VOID);

extern VOS_VOID AT_UsbSwitchGwMode(VOS_VOID);
extern VOS_VOID AT_InitPort(VOS_VOID);
extern VOS_VOID AT_InitStk(VOS_VOID);
extern VOS_VOID AT_InitPara(VOS_VOID);

extern VOS_UINT32  At_PidInit(enum VOS_INIT_PHASE_DEFINE enPhase);

/* Added by f62575 for DTS2013012408620, 2013-01-24, begin */
VOS_VOID AT_ReadSsNV( VOS_VOID );
/* Added by f62575 for DTS2013012408620, 2013-01-24, end */

/* Added by j00174725 for UART-MODEM Project, 2013-09-21, begin */
VOS_VOID AT_ReadUartCfgNV(VOS_VOID);
/* Added by j00174725 for UART-MODEM Project, 2013-09-21, end */

/* Add by j00174725 for V3R3 HSUART PHASEIII, 2013-12-28, Begin */
VOS_VOID AT_UsbEnableCB(VOS_VOID);

VOS_VOID AT_UsbDisableCB(VOS_VOID);

VOS_VOID AT_HsicEnableCB(VOS_VOID);

VOS_VOID AT_HsicDisableCB(VOS_VOID);

VOS_VOID AT_ReadPortBuffCfgNV(VOS_VOID);

/* Add by j00174725 for V3R3 HSUART PHASEIII, 2013-12-28, End */


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

#endif /* end of AtInit.h */

