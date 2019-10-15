/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : CdsLPdcpInterface.h
  版 本 号   : 初稿
  作    者   : h00133115
  生成日期   : 2011年12月10日
  最近修改   :
  功能描述   : CDS与LPDCP之间的接口
  函数列表   :
  修改历史   :
  1.日    期   : 2011年12月10日
    作    者   :
    修改内容   : 创建文件

******************************************************************************/

#ifndef __CDSLPDCPINTERFACE_H__
#define __CDSLPDCPINTERFACE_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"
#include "LPSCommon.h"
#include "CdsInterface.h"

#if (VOS_OS_VER != VOS_WIN32)
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif


/*****************************************************************************
  2 宏定义
*****************************************************************************/


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
 结构名    : CDS_LPDCP_DATA_STRU
 协议表格  :
 ASN.1描述 :
 结构说明  : CDS与LPDCP之间的原语
*****************************************************************************/
typedef struct
{
    VOS_UINT8                           ucRbId;
    VOS_UINT8                           aucRev[3];
    VOS_UINT32                          ulSduLen;
    TTF_MEM_ST                         *pstSdu;
}CDS_LPDCP_DATA_STRU;

/*****************************************************************************
  8 UNION定义
*****************************************************************************/


/*****************************************************************************
  9 OTHERS定义
*****************************************************************************/


/*****************************************************************************
  10 函数声明
*****************************************************************************/

extern VOS_VOID CDS_LPDCP_DataReq (CDS_LPDCP_DATA_STRU *pstPdcpData);
extern VOS_VOID CDS_LTE_DL_DataInd (CDS_LPDCP_DATA_STRU *pstDsLpdcpData);


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

#endif /* end of CdsLPdcpInterface.h */
