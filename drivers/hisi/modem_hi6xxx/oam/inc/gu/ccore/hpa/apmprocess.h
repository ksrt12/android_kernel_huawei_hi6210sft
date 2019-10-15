/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : apmprocess.h
  版 本 号   : 初稿
  作    者   : s00207770
  生成日期   : 2013年3月5日
  最近修改   :
  功能描述   : apmprocess.h 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2013年3月5日
    作    者   : s00207770
    修改内容   : 创建文件

******************************************************************************/

#ifndef __APMPROCESS_H__
#define __APMPROCESS_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/


#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/
#define APM_LDF_LOAD_PHY_DELAY_TIME         (15)   /* 上移DSP时加载DSP需要延时的时间 400ms */


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


#if ((VOS_OS_VER == VOS_WIN32) || (VOS_OS_VER == VOS_NUCLEUS))
#pragma pack()
#else
#pragma pack(0)
#endif



#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif

#endif /* end of apmprocess.h */
