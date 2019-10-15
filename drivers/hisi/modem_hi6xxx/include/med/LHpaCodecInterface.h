/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : LHpaCodecInterface.h
  版 本 号   : V1.00
  作    者   : hujianbo 00238358
  生成日期   : 2014年11月17日
  最近修改   :
  功能描述   : LHpaCodecInterface.h 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2014年11月17日
    作    者   : hujianbo 00238358
    修改内容   : 创建文件
******************************************************************************/

#ifndef __LHPACODECINTERFACE_H__
#define __LHPACODECINTERFACE_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"

#pragma pack(4)

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
enum LHPA_VOICE_MSG_TYPE_ENUM
{
    ID_LHPA_VOICE_SYNC_IND           = 0x94D0,           /* _H2ASN_MsgChoice LHPA_VOICE_SYNC_IND_STRU */

    LHPA_VOICE_MSG_TYPE_BUTT
} ;
typedef VOS_UINT16  LHPA_VOICE_MSG_TYPE_ENUM_UINT16;


/*****************************************************************************
  6 消息定义
*****************************************************************************/

/*****************************************************************************
 实体名称  : LHPA_VOICE_SYNC_IND_STRU
 功能描述  : VoLTE语音业务启动后，LHPA发送该消息给CODEC进行时序同步
            |------------------|-------------------|-----OnDuration-----|
            |                  |<--uhwSendOffset-->|
            |<---uhwWakeUpOffset------------------>|
            |                  |                   |
            |                  |                   |
            HIFI唤醒时刻       HIFI发送语音包时刻  CDRX OnDuration起始时刻
*****************************************************************************/
typedef struct
{
    VOS_MSG_HEADER                                                              /*_H2ASN_Skip*/
    VOS_UINT16                          uhwMsgId;                               /* 待定*/ /*_H2ASN_Skip*/
    VOS_UINT16                          uhwOpId;                                /* 每发送1条消息，该字段加1*/

    VOS_UINT32                          uwOnDurationSlice;                      /* LPHY给出的LTE CDRX OnDuration起始时刻*/
    VOS_UINT16                          uhwWakeUpOffset;                        /* 以uwOnDurationSlice为基点，HIFI唤醒提前量*/
    VOS_UINT16                          uhwSendOffset;                          /* 以uwOnDurationSlice为基点，HIFI发送语音包前量 */
}LHPA_VOICE_SYNC_IND_STRU;

/*****************************************************************************
  7 STRUCT定义
*****************************************************************************/


/*****************************************************************************
  8 UNION定义
*****************************************************************************/
#if 0                                                                           /* _H2ASN_Skip */

/*****************************************************************************
  H2ASN顶级消息结构定义
*****************************************************************************/
/* 需要更改相应的ASN编码,去掉L方式,仅保留V方式 */
typedef struct
{
    LHPA_VOICE_MSG_TYPE_ENUM_UINT16          enMsgID;                              /* _H2ASN_MsgChoice_Export LHPA_VOICE_MSG_TYPE_ENUM_UINT16 */

    VOS_UINT8                           aucMsgBlock[2];
    /***************************************************************************
        _H2ASN_MsgChoice_When_Comment          LHPA_VOICE_MSG_TYPE_ENUM_UINT16
    ****************************************************************************/
}VOICE_LHPA_TRACE_MSG_DATA;

/* 协议消息结构*/
typedef struct
{
    VOS_UINT32 uwSenderCpuId;
    VOS_UINT32 uwSenderPid;
    VOS_UINT32 uwReceiverCpuId;
    VOS_UINT32 uwReceiverPid;
    VOS_UINT32 uwLength;
    VOICE_LHPA_TRACE_MSG_DATA              stMsgData;
}VoiceLhpaTrace_MSG;

#endif    																		/* _H2ASN_Skip */


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

#endif /* end of LHpaCodecInterface.h */



