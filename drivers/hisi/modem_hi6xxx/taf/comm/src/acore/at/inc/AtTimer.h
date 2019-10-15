/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : AtTimer.h
  版 本 号   : 初稿
  作    者   : 鲁琳/l60609
  生成日期   : 2011年10月24日
  最近修改   :
  功能描述   : AtTimer.c的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2011年10月24日
    作    者   : 鲁琳/l60609
    修改内容   : 创建文件

******************************************************************************/
#ifndef __AT_TIMER_H__
#define __AT_TIMER_H__


/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include  "vos.h"


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
 枚举名    : AT_TIMER_OPERATION_TYPE_ENUM
 结构说明  : AT定时器操作的取值

  1.日    期   : 2011年10月24日
    作    者   : 鲁琳/l60609
    修改内容   : AT Project：将AT TIMER隔离出来单独维护
*****************************************************************************/
enum AT_TIMER_OPERATION_TYPE_ENUM
{
    AT_TIMER_OPERATION_START,
    AT_TIMER_OPERATION_STOP
};
typedef VOS_UINT32  AT_TIMER_OPERATION_TYPE_ENUM_UINT32;

/* Added by A00165503 for UART-MODEM Project, 2013-09-23, Begin */
/*****************************************************************************
 枚举名    : AT_TIMER_STATUS_ENUM
 枚举说明  : AT定时器状态

  1.日    期   : 2013年9月23日
    作    者   : A00165503
    修改内容   : 新增枚举
*****************************************************************************/
enum AT_TIMER_STATUS_ENUM
{
    AT_TIMER_STATUS_STOP,               /* 定时器停止状态 */
    AT_TIMER_STATUS_RUNNING,            /* 定时器运行状态 */

    AT_TIMER_STATUS_BUTT
};
typedef VOS_UINT32 AT_TIMER_STATUS_ENUM_UINT32;
/* Added by A00165503 for UART-MODEM Project, 2013-09-23, End */


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
 结构名    : AT_TIMER_OPERATION_STRU
 结构说明  : AT定时器操作的结构，用于SDT中显示

  1.日    期   : 2011年10月24日
    作    者   : 鲁琳/l60609
    修改内容   : AT Project：将AT TIMER隔离出来单独维护
*****************************************************************************/
typedef struct
{
    VOS_MSG_HEADER
    VOS_UINT32                          ulMsgName;
    VOS_UINT32                          ulTimeLen;
    AT_TIMER_OPERATION_TYPE_ENUM_UINT32 enTimeAction;
}AT_TIMER_OPERATION_STRU;


/*****************************************************************************
  8 UNION定义
*****************************************************************************/


/*****************************************************************************
  9 OTHERS定义
*****************************************************************************/


/*****************************************************************************
  10 函数声明
*****************************************************************************/
VOS_UINT32 AT_StopRelTimer(
    VOS_UINT32                          ulName,
    HTIMER                             *phTm
);

VOS_UINT32 At_StartTimer(
    VOS_UINT32                          usLength,
    VOS_UINT8                           ucIndex
);

VOS_UINT32 AT_StartRelTimer(
    HTIMER                             *phTm,
    VOS_UINT32                          ulLength,
    VOS_UINT32                          ulName,
    VOS_UINT32                          ulParam,
    VOS_UINT8                           ucMode
);


#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of AtTimer.h */

