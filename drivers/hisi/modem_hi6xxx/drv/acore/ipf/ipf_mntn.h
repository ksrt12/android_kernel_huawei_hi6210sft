/*************************************************************************
*   版权所有(C) 1987-2012, 深圳华为技术有限公司.
*
*   文 件 名 :  ipf_mntn.h
*
*   作    者 :  zhoushan
*
*   描    述 :  ipf_mntn.c头文件
*
*  修改历史   :
*  1.日    期   : 2013年6月3日
*    作    者   : 周珊 00212992
*    修改内容   : 创建文件
*************************************************************************/
/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/

#ifndef __IPF_MNTN_H__
#define __IPF_MNTN_H__

#include "drv_global.h"
#include "ipf_driver.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
/*****************************************************************************
  2 宏定义
*****************************************************************************/

/*****************************************************************************
  3 枚举定义
*****************************************************************************/

/*****************************************************************************
  4 消息头定义
*****************************************************************************/

/*****************************************************************************
  5 消息定义
*****************************************************************************/

/*****************************************************************************
  6 STRUCT定义
*****************************************************************************/

/*****************************************************************************
  7 UNION定义
*****************************************************************************/


/*****************************************************************************
  8 OTHERS定义
*****************************************************************************/


/*****************************************************************************
  9 全局变量声明
*****************************************************************************/


/*****************************************************************************
  10 函数声明
*****************************************************************************/
extern BSP_VOID BSP_IPF_Help(BSP_VOID);
extern BSP_S32 BSP_IPF_RegDump(BSP_VOID);
extern BSP_S32 BSP_IPF_DbgInfo(BSP_VOID);
extern BSP_VOID BSP_IPF_MemInfo(BSP_VOID);
extern BSP_S32 BSP_IPF_BDInfo(IPF_CHANNEL_TYPE_E eChnType, BSP_U32 u32BdqPtr);
extern BSP_S32 BSP_IPF_RDInfo(IPF_CHANNEL_TYPE_E eChnType, BSP_U32 u32RdqPtr);
extern BSP_S32 BSP_IPF_CHInfo(IPF_CHANNEL_TYPE_E eChnType);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* __IPF_MNTN_H__ */
