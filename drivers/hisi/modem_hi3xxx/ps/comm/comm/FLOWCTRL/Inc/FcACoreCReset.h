/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : FcACoreCReset.h
  版 本 号   : 初稿
  作    者   : caikai
  生成日期   : 2013年4月22日
  最近修改   :
  功能描述   : FcACoreCReset.c 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2013年4月22日
    作    者   : caikai
    修改内容   : 创建文件

******************************************************************************/

#ifndef __FCACORECRESET_H__
#define __FCACORECRESET_H__


/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include  "vos.h"
#include  "PsTypeDef.h"
#include  "mdrv.h"
#include  "FcIntraMsg.h"



#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/
/* 底软C核复位模块等待FcACore完成回调事务超时时间 */
#define         FC_ACORE_CRESET_TIMEOUT_LEN         (5000)

/* 注册给底软C核复位接口的回调函数的优先级 */
#define         FC_ACORE_CRESET_CALLBACK_PRIOR      (32)


/*****************************************************************************
  3 枚举定义
*****************************************************************************/
/*====================================*//* 指示C核复位前后的枚举 */
enum FC_ACORE_CRESET_MOMENT_ENUM
{
    FC_ACORE_CRESET_BEFORE_RESET = 0,       /* 代表C核复位处理时 */
    FC_ACORE_CRESET_AFTER_RESET,            /* 代表C核复位成功后 */
    FC_ACORE_CRESET_BUTT
};
typedef VOS_UINT8 FC_ACORE_CRESET_MOMENT_ENUM_UINT8;

/*****************************************************************************
  4 全局变量声明
*****************************************************************************/
extern VOS_SEM      g_ulFcACoreCResetDoneSem;


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

extern VOS_UINT32 FC_ACORE_CResetProc(FC_ACORE_CRESET_MOMENT_ENUM_UINT8 enCResetMoment);
extern VOS_INT FC_ACORE_CResetCallback(DRV_RESET_CB_MOMENT_E enParam, VOS_INT userdata);
extern VOS_VOID FC_ACORE_CResetRcvStartRsp(VOS_VOID);
extern VOS_UINT32  FC_ACORE_CResetSendNotify(FC_MSG_TYPE_ENUM_UINT16     usMsgName);

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

#endif /* end of FcACoreCReset.h */

