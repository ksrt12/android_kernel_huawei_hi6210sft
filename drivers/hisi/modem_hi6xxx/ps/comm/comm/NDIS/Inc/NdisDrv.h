/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : NdisDrv.h
  版 本 号   : 初稿
  作    者   : 翟鹏 00282933
  生成日期   : 2014年9月1日
  最近修改   :
  功能描述   : 协议栈封装底软API接口
  函数列表   :
  修改历史   :
  1.日    期   : 2014年9月1日
    作    者   : 翟鹏 00282933
    修改内容   : 创建文件

******************************************************************************/

#ifndef __NDISDRV_H__
#define __NDISDRV_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "mdrv.h"

#if (VOS_OS_VER != VOS_WIN32)
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif

/*****************************************************************************
  2 宏定义
*****************************************************************************/
/*BSP_OM_GetBoardType*/
#define NDIS_VER_GET_INFO mdrv_ver_get_info

/*mdrv_nv_readex*/
#define NDIS_NV_READX NV_ReadEx

/*DIAG_TraceReport*/
#define NDIS_DIAG_TRACEREPORT DIAG_ReportLayerMessageLog

/*DRV_UDI_IOCTL*/
#define NDIS_UDI_IOCTL mdrv_udi_ioctl

/*udi_write*/
#define NDIS_UDI_WRITE mdrv_udi_write

/*****************************************************************************
  3 枚举定义
*****************************************************************************/

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

#if (VOS_OS_VER != VOS_WIN32)
#pragma pack()
#else
#pragma pack(pop)
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif

