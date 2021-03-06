/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : CsdLog.h
  版 本 号   : 初稿
  作    者   : w00199382
  生成日期   : 2011年12月17日
  最近修改   :
  功能描述   : CsdLog.h 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2011年12月17日
    作    者   : w00199382
    修改内容   : 创建文件

******************************************************************************/

#ifndef __CSDLOG_H__
#define __CSDLOG_H__

/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"
#include "pslog.h"
#include "PsCommonDef.h"



#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/
/*================================================*/
/* 数值宏定义 */
/*================================================*/
#define CSD_MEM_FAIL()\
        PS_LOG(WUEPS_PID_OM, 0, PS_PRINT_ERROR, "CSD Mem Operation Failed!");
#define CSD_MSG_FAIL()\
        PS_LOG(WUEPS_PID_OM, 0, PS_PRINT_ERROR, "CSD Msg Opration Failed!");
#define CSD_TIMER_FAIL()\
        PS_LOG(WUEPS_PID_OM, 0, PS_PRINT_ERROR, "CSD Timer Opration Failed!");

/*================================================*/
/* 功能函数宏定义 */
/*================================================*/
#ifndef SUBMOD_NULL
#define    SUBMOD_NULL                                                  (0)
#endif

#define CSD_INFO_LOG(Mod, String)\
        PS_LOG ( (Mod), SUBMOD_NULL,  PS_PRINT_INFO, (String) )
#define CSD_INFO_LOG1(Mod, String,Para1)\
        PS_LOG1 ( (Mod), SUBMOD_NULL, PS_PRINT_INFO, (String), (VOS_INT32)(Para1) )
#define CSD_INFO_LOG2(Mod, String,Para1,Para2)\
        PS_LOG2 ( (Mod), SUBMOD_NULL, PS_PRINT_INFO, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2) )
#define CSD_INFO_LOG3(Mod, String,Para1,Para2,Para3)\
        PS_LOG3 ( (Mod), SUBMOD_NULL, PS_PRINT_INFO, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3) )
#define CSD_INFO_LOG4(Mod, String,Para1,Para2,Para3,Para4)\
        PS_LOG4 ( (Mod), SUBMOD_NULL, PS_PRINT_INFO, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3), (VOS_INT32)(Para4) )

#define CSD_NORMAL_LOG(Mod, String)\
        PS_LOG ( (Mod), SUBMOD_NULL,  PS_PRINT_NORMAL, (String) )
#define CSD_NORMAL_LOG1(Mod, String,Para1)\
        PS_LOG1 ( (Mod), SUBMOD_NULL, PS_PRINT_NORMAL, (String), (VOS_INT32)(Para1) )
#define CSD_NORMAL_LOG2(Mod, String,Para1,Para2)\
        PS_LOG2 ( (Mod), SUBMOD_NULL, PS_PRINT_NORMAL, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2) )
#define CSD_NORMAL_LOG3(Mod, String,Para1,Para2,Para3)\
        PS_LOG3 ( (Mod), SUBMOD_NULL, PS_PRINT_NORMAL, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3) )
#define CSD_NORMAL_LOG4(Mod, String,Para1,Para2,Para3,Para4)\
        PS_LOG4 ( (Mod), SUBMOD_NULL, PS_PRINT_NORMAL, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3), (VOS_INT32)(Para4) )

#define CSD_WARNING_LOG(Mod, String)\
        PS_LOG ( (Mod), SUBMOD_NULL,  PS_PRINT_WARNING, (String) )
#define CSD_WARNING_LOG1(Mod, String,Para1)\
        PS_LOG1 ( (Mod), SUBMOD_NULL, PS_PRINT_WARNING, (String), (VOS_INT32)(Para1) )
#define CSD_WARNING_LOG2(Mod, String,Para1,Para2)\
        PS_LOG2 ( (Mod), SUBMOD_NULL, PS_PRINT_WARNING, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2) )
#define CSD_WARNING_LOG3(Mod, String,Para1,Para2,Para3)\
        PS_LOG3 ( (Mod), SUBMOD_NULL, PS_PRINT_WARNING, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3) )
#define CSD_WARNING_LOG4(Mod, String,Para1,Para2,Para3,Para4)\
        PS_LOG4 ( (Mod), SUBMOD_NULL, PS_PRINT_WARNING, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3), (VOS_INT32)(Para4) )

#define CSD_ERROR_LOG(Mod, String)\
        PS_LOG ( (Mod), SUBMOD_NULL,  PS_PRINT_ERROR, (String) )
#define CSD_ERROR_LOG1(Mod, String,Para1)\
        PS_LOG1 ( (Mod), SUBMOD_NULL, PS_PRINT_ERROR, (String), (VOS_INT32)(Para1) )
#define CSD_ERROR_LOG2(Mod, String,Para1,Para2)\
        PS_LOG2 ( (Mod), SUBMOD_NULL, PS_PRINT_ERROR, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2) )
#define CSD_ERROR_LOG3(Mod, String,Para1,Para2,Para3)\
        PS_LOG3 ( (Mod), SUBMOD_NULL, PS_PRINT_ERROR, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3) )
#define CSD_ERROR_LOG4(Mod, String,Para1,Para2,Para3,Para4)\
        PS_LOG4 ( (Mod), SUBMOD_NULL, PS_PRINT_ERROR, (String), (VOS_INT32)(Para1), (VOS_INT32)(Para2), (VOS_INT32)(Para3), (VOS_INT32)(Para4) )

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

#endif /* end of CsdLog.h */
