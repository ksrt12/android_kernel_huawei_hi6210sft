/******************************************************************************

                  版权所有 (C), 2001-2013, 华为技术有限公司

 ******************************************************************************
  文 件 名   : MtaMtcInterface.h
  版 本 号   : 初稿
  作    者   : l00198894
  生成日期   : 2013年10月21日
  最近修改   :
  功能描述   : MTA 与 MTC模块共用头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2013年10月21日
    作    者   : l00198894
    修改内容   : 创建文件

******************************************************************************/

#ifndef _MTCMTAINTERFACE_H_
#define _MTCMTAINTERFACE_H_

/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"
#include "PsTypeDef.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/

/*****************************************************************************
  3 枚举定义
*****************************************************************************/
/*****************************************************************************
枚举名    : MTA_MTC_MSG_ID_ENUM
结构说明  : MTA模块和MTC模块之间的消息

  1.日    期   : 2013年10月21日
    作    者   : l00198894
    修改内容   : V9R1C50 SVLTE离网重选项目新增
*****************************************************************************/
enum MTA_MTC_MSG_ID_ENUM
{
    /* 消息名称 */                         /* 消息ID */                         /* 备注 */
    /* MTA --> MTC */
    ID_MTA_MTC_GSM_CELLINFO_QRY_REQ     = 0x0001,                               /* _H2ASN_MsgChoice MTA_MTC_RESERVE_STRU*/

    /* MTC --> MTA */
    ID_MTC_MTA_PS_TRANSFER_IND          = 0x0002,                               /* _H2ASN_MsgChoice MTC_MTA_PS_TRANSFER_IND_STRU */
    /* Add by j00174725 for RF&LCD INTRUSION, 2014-03-04, Begin */
    ID_MTC_MTA_MIPICLK_INFO_IND         = 0x0004,                               /* _H2ASN_MsgChoice MTC_MTA_MIPICLK_INFO_IND_STRU*/
    /* Add by j00174725 for RF&LCD INTRUSION, 2014-03-04, End */
    ID_MTC_MTA_GSM_CELLINFO_QRY_CNF     = 0x0006,                               /* _H2ASN_MsgChoice MTC_MTA_GSM_CELLINFO_QRY_CNF_STRU*/

    ID_MTA_MTC_MSG_ID_BUTT
};
typedef VOS_UINT32  MTA_MTC_MSG_ID_ENUM_UINT32;

/* Added by l00198894 for ECID Custom Config, 2014-11-17, begin */
/*****************************************************************************
 枚举名    : MTA_MTC_RESULT_ENUM
 枚举说明  : MTA和MTC的之间的结果码

  1.日    期   : 2014年11月17日
    作    者   : l00198894
    修改内容   : 新增枚举
*****************************************************************************/
enum MTA_MTC_RESULT_ENUM
{
    MTA_MTC_RESULT_NO_ERROR             = 0x0000,                               /* 消息处理正常 */
    MTA_MTC_RESULT_ERROR,

    MTA_MTC_RESULT_BUTT
};
typedef VOS_UINT32  MTA_MTC_RESULT_ENUM_UINT32;
/* Added by l00198894 for ECID Custom Config, 2014-11-17, end */

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
/* Added by l00198894 for ECID Custom Config, 2014-11-17, begin */
/*******************************************************************************
 结构名    : MTA_MTC_PLMN_ID_STRU
 结构说明  : PLMN ID结构体
 1.日    期   : 2014年11月17日
   作    者   : l00198894
   修改内容   : 新增结构体
*******************************************************************************/
typedef struct
{
    VOS_UINT32                          ulMcc;              /* MCC,3 bytes      */
    VOS_UINT32                          ulMnc;              /* MNC,2 or 3 bytes */
} MTA_MTC_PLMN_ID_STRU;

/*******************************************************************************
 结构名    : MTA_MTC_RESERVE_STRU
 结构说明  : MTA与MTC消息保留结构体
 1.日    期   : 2014年11月17日
   作    者   : l00198894
   修改内容   : 新增结构体
*******************************************************************************/
typedef struct
{
    MSG_HEADER_STRU                     stMsgHeader;                            /*_H2ASN_Skip*/
    VOS_UINT8                           aucReserved[4];                         /* 保留位 */
} MTA_MTC_RESERVE_STRU;
/* Added by l00198894 for ECID Custom Config, 2014-11-17, end */

/*******************************************************************************
 结构名    : MTC_MTA_PS_TRANSFER_IND_STRU
 结构说明  : ID_MTC_MTA_PS_TRANSFER_IND的消息结构

 1.日    期   : 2013年10月21日
   作    者   : l00198894
   修改内容   : V9R1C50 SVLTE离网重选项目新增
*******************************************************************************/
typedef struct
{
    MSG_HEADER_STRU                     stMsgHeader;                            /*_H2ASN_Skip*/
    VOS_UINT8                           ucCause;                                /* PS域切换原因值 */
    VOS_UINT8                           aucReserved[3];                         /*保留位*/
} MTC_MTA_PS_TRANSFER_IND_STRU;

/* Add by j00174725 for RF&LCD INTRUSION, 2014-03-04, Begin */
/*******************************************************************************
 结构名    : MTC_MTA_MIPICLK_INFO_IND_STRU
 结构说明  : ID_MTC_MTA_MIPICLK_IND的消息结构

 1.日    期   : 2014年03月04日
   作    者   : j00174725
   修改内容   : RF&LCD INTRUSION项目新增
*******************************************************************************/
typedef struct
{
    MSG_HEADER_STRU                     stMsgHeader;                            /*_H2ASN_Skip*/
    VOS_UINT16                          usMipiCLk;                              /* 可选MIPICLK值集合 */
    VOS_UINT8                           aucReserved[2];                         /*保留位*/
} MTC_MTA_MIPICLK_INFO_IND_STRU;
/* Add by j00174725 for RF&LCD INTRUSION, 2014-03-04, End */

/* Added by l00198894 for ECID Custom Config, 2014-11-17, begin */
/*******************************************************************************
 结构名    : MTC_GSM_CELLINFO_EX_STRU
 结构说明  : GSM小区扩展信息结构体

 1.日    期   : 2014年11月17日
   作    者   : l00198894
   修改内容   : 新增结构体
*******************************************************************************/
typedef struct
{
    VOS_UINT32                          ulValidFlg;                             /* GSM小区信息有效性标示 */
    MTA_MTC_PLMN_ID_STRU                stPlmnId;                               /* PLMN ID */
    VOS_UINT16                          usLac;                                  /* LAC */
    VOS_UINT16                          usCellId;                               /* 小区ID */
} MTC_GSM_CELLINFO_EX_STRU;

/*******************************************************************************
 结构名    : MTC_MTA_GSM_CELLINFO_QRY_CNF_STRU
 结构说明  : ID_MTC_MTA_GSM_CELLINFO_QRY_CNF的消息结构

 1.日    期   : 2014年11月17日
   作    者   : l00198894
   修改内容   : 新增结构体
*******************************************************************************/
typedef struct
{
    MSG_HEADER_STRU                     stMsgHeader;                            /*_H2ASN_Skip*/
    MTA_MTC_RESULT_ENUM_UINT32          enResult;                               /* 操作结果 */
    MTC_GSM_CELLINFO_EX_STRU            stCellInfoEx;                           /* GSM小区信息 */
} MTC_MTA_GSM_CELLINFO_QRY_CNF_STRU;
/* Added by l00198894 for ECID Custom Config, 2014-11-17, end */

/*****************************************************************************
  8 UNION定义
*****************************************************************************/


/*****************************************************************************
  9 OTHERS定义
*****************************************************************************/

/*****************************************************************************
  H2ASN顶级消息结构定义
*****************************************************************************/
typedef struct
{
    MTA_MTC_MSG_ID_ENUM_UINT32          enMsgID;                                /*_H2ASN_MsgChoice_Export MTA_MTC_MSG_ID_ENUM_UINT32*/
    VOS_UINT8                           aucMsgBlock[4];
    /***************************************************************************
        _H2ASN_MsgChoice_When_Comment          MTA_MTC_MSG_ID_ENUM_UINT32
    ****************************************************************************/
} MTA_MTC_MSG_DATA;
/*_H2ASN_Length UINT32*/

typedef struct
{
    VOS_MSG_HEADER
    MTA_MTC_MSG_DATA                    stMsgData;
} MtaMtcInterface_MSG;

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

#endif /* end of MtaMtcInterface.h */

