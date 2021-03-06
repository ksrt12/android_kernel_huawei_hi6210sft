/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : AtEventReport.h
  版 本 号   : 初稿
  作    者   : sunshaohua
  生成日期   : 2010年1月20日
  最近修改   :
  功能描述   : AtEventReport.c 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2010年1月20日
    作    者   : sunshaohua
    修改内容   : 创建文件

******************************************************************************/


#ifndef __ATEVENTREPORT_H__
#define __ATEVENTREPORT_H__


/*****************************************************************************
  1 其他头文件包含
*****************************************************************************/
#include "vos.h"
#include "TafApsApi.h"

/* Added by Y00213812 for VoLTE_PhaseI 项目, 2013-07-08, begin */
#include "siappstk.h"
/* Added by Y00213812 for VoLTE_PhaseI 项目, 2013-07-08, end */
/* Added by s00217060 for VoLTE_PhaseI  项目, 2013-08-16, begin */
#include "MnCallApi.h"
/* Added by s00217060 for VoLTE_PhaseI  项目, 2013-08-16, end */

#include "AtParse.h"
#include "AtCtx.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#pragma pack(4)

/*****************************************************************************
  2 宏定义
*****************************************************************************/
#define AT_UNKNOWN_CLCK_CLASS  (0)


/* Added by l00198894 for STK&DCM Project, 2012/09/18, begin */
#define AT_GET_CURC_RPT_CTRL_STATUS_MAP_TBL_PTR()    (&g_aenAtCurcRptCmdTable[0])
#define AT_GET_CURC_RPT_CTRL_STATUS_MAP_TBL_SIZE()   (sizeof(g_aenAtCurcRptCmdTable)/sizeof(AT_RPT_CMD_INDEX_ENUM_UINT8))
/* Added by l00198894 for STK&DCM Project, 2012/09/18, end */

/* Added by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-8, begin */
#define AT_GET_UNSOLICITED_RPT_CTRL_STATUS_MAP_TBL_PTR()    (&g_aenAtUnsolicitedRptCmdTable[0])
#define AT_GET_UNSOLICITED_RPT_CTRL_STATUS_MAP_TBL_SIZE()   (sizeof(g_aenAtUnsolicitedRptCmdTable)/sizeof(AT_RPT_CMD_INDEX_ENUM_UINT8))
/* Added by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-8, end */

/* Added by A00165503 for STK&DCM Project, 2012-09-25, Begin */
#define AT_GET_CME_CALL_ERR_CODE_MAP_TBL_PTR()  (g_astAtCmeCallErrCodeMapTbl)
#define AT_GET_CME_CALL_ERR_CODE_MAP_TBL_SIZE() (sizeof(g_astAtCmeCallErrCodeMapTbl)/sizeof(AT_CME_CALL_ERR_CODE_MAP_STRU))
/* Added by A00165503 for STK&DCM Project, 2012-09-25, End */

/* Added by w00176964 for SS FDN&Call Control, 2013-5-17, begin */
#define AT_GET_CMS_SMS_ERR_CODE_MAP_TBL_PTR()  (g_astAtCmsSmsErrCodeMapTbl)
#define AT_GET_CMS_SMS_ERR_CODE_MAP_TBL_SIZE() (sizeof(g_astAtCmsSmsErrCodeMapTbl)/sizeof(AT_CMS_SMS_ERR_CODE_MAP_STRU))

/* Added by w00176964 for SS FDN&Call Control, 2013-5-17, end */

/* Added by n00269697 for VTLTE, 2014-06-16, Begin */
#define AT_EVT_IS_VIDEO_CALL(enCallType)            \
           ((MN_CALL_TYPE_VIDEO_TX == enCallType)   \
         || (MN_CALL_TYPE_VIDEO_RX == enCallType)   \
         || (MN_CALL_TYPE_VIDEO    == enCallType))

#define AT_EVT_IS_CS_VIDEO_CALL(enCallType, enVoiceDomain)  \
           ((MN_CALL_TYPE_VIDEO         == enCallType)      \
         && (TAF_CALL_VOICE_DOMAIN_3GPP == enVoiceDomain))

#define AT_EVT_IS_PS_VIDEO_CALL(enCallType, enVoiceDomain)  \
           (((MN_CALL_TYPE_VIDEO        == enCallType)      \
          || (MN_CALL_TYPE_VIDEO_TX     == enCallType)      \
          || (MN_CALL_TYPE_VIDEO_RX     == enCallType))     \
         && (TAF_CALL_VOICE_DOMAIN_IMS  == enVoiceDomain))

#define AT_EVT_REL_IS_NEED_CLR_TIMER_STATUS_CMD(CmdCurrentOpt)  \
           ((AT_CMD_D_CS_VOICE_CALL_SET == CmdCurrentOpt)       \
         || (AT_CMD_D_CS_DATA_CALL_SET  == CmdCurrentOpt)       \
         || (AT_CMD_APDS_SET            == CmdCurrentOpt)       \
         || (AT_CMD_CHLD_EX_SET         == CmdCurrentOpt)       \
         || (AT_CMD_A_SET               == CmdCurrentOpt)       \
         || (AT_CMD_END_SET             == CmdCurrentOpt))
/* Added by n00269697 for VTLTE, 2014-06-16, End */

/*****************************************************************************
  3 枚举定义
*****************************************************************************/
/*****************************************************************************
 枚举名    : AT_CS_CALL_STATE_ENUM
 结构说明  : CS域呼叫状态枚举

  1.日    期   : 2012年09月18日
    作    者   : l00198894
    修改内容   : STK补充特性及DCM需求开发项目新增枚举

*****************************************************************************/
enum AT_CS_CALL_STATE_ENUM
{
    AT_CS_CALL_STATE_ORIG               = 0,                                    /* originate a MO Call */
    AT_CS_CALL_STATE_CALL_PROC,                                                 /* Call is Proceeding */
    AT_CS_CALL_STATE_ALERTING,                                                  /* Alerting,MO Call */
    AT_CS_CALL_STATE_CONNECT,                                                   /* Call Connect */
    AT_CS_CALL_STATE_RELEASED,                                                  /* Call Released */
    AT_CS_CALL_STATE_INCOMMING,                                                 /* Incoming Call */
    AT_CS_CALL_STATE_WAITING,                                                   /* Waiting Call */
    AT_CS_CALL_STATE_HOLD,                                                      /* Hold Call */
    AT_CS_CALL_STATE_RETRIEVE,                                                  /* Call Retrieved */

    AT_CS_CALL_STATE_BUTT
};
typedef VOS_UINT8 AT_CS_CALL_STATE_ENUM_UINT8;

/* Added by n00269697 for V3R3C60_eCall项目, 2014-3-29, begin */
enum AT_ECALL_TYPE_ENUM
{
    AT_ECALL_TYPE_TEST                  = 0,                                    /* test ecall */
    AT_ECALL_TYPE_RECFGURATION,                                                 /* reconfiguration call */
    AT_ECALL_TYPE_MIEC,                                                         /* manually initiated ecall */
    AT_ECALL_TYPE_AIEC,                                                         /* automatic initiated ecall */

    AT_ECALL_TYPE_BUTT
};
typedef VOS_UINT8  AT_ECALL_TYPE_ENUM_U8;
/* Added by n00269697 for V3R3C60_eCall项目, 2014-3-29, end */

/*****************************************************************************
  4 全局变量声明
*****************************************************************************/
extern VOS_UINT32                       g_ulAtAppDialModeCnf;

extern VOS_UINT32                       g_ulLcStartTime;


/*****************************************************************************
  5 消息头定义
*****************************************************************************/


/*****************************************************************************
  6 消息定义
*****************************************************************************/


/*****************************************************************************
  7 STRUCT定义
*****************************************************************************/
typedef TAF_VOID (*AT_SMS_RSP_PROC_FUN)(
    TAF_UINT8                           ucIndex,
    MN_MSG_EVENT_INFO_STRU              *pstEvent
);

typedef TAF_VOID (*AT_QRY_PARA_PROC_FUNC)(TAF_UINT8   ucIndex,
                                          TAF_UINT8   OpId,
                                          TAF_VOID    *pPara);

/*lint -e958 -e959 修改人:l60609;原因:64bit*/
typedef struct
{
    VOS_UINT32                          QueryType;                              /* 查询类型 */
    AT_QRY_PARA_PROC_FUNC               AtQryParaProcFunc;                      /* 查询类型对应的处理函数 */
}AT_QUERY_TYPE_FUNC_STRU;
/*lint +e958 +e959 修改人:l60609;原因:64bit*/

/* Added by f62575 for V9R1 STK升级, 2013-6-26, begin */
/*****************************************************************************
 结构名  : AT_SMS_ERROR_CODE_MAP_STRU
 结构说明: 短信接收发送结果正式响应错误码与AT模块错误码映射结构

 修改历史      :
  1.日    期   : 2012年9月25日
    作    者   : A00165503
    修改内容   : 新增结构
*****************************************************************************/
typedef struct
{
    TAF_MSG_ERROR_ENUM_UINT32           enMsgErrorCode;
    AT_RRETURN_CODE_ENUM_UINT32         enAtErrorCode;
}AT_SMS_ERROR_CODE_MAP_STRU;
/* Added by f62575 for V9R1 STK升级, 2013-6-26, end */

/* Added by A00165503 for STK&DCM Project, 2012-09-25, Begin */
/*****************************************************************************
 结构名  : AT_CME_CALL_ERR_CODE_MAP_STRU
 结构说明: CS域错误码与CME错误码映射结构

 修改历史      :
  1.日    期   : 2012年9月25日
    作    者   : A00165503
    修改内容   : 新增结构
*****************************************************************************/
typedef struct
{
    VOS_UINT32                          ulCmeCode;
    TAF_CS_CAUSE_ENUM_UINT32            enCsCause;

} AT_CME_CALL_ERR_CODE_MAP_STRU;
/* Added by A00165503 for STK&DCM Project, 2012-09-25, End */

/* Added by w00176964 for SS FDN&Call Control, 2013-5-17, begin */
/*****************************************************************************
 结构名  : AT_CMS_SMS_ERR_CODE_MAP_STRU
 结构说明: SMS域错误码与CMS错误码映射结构

 修改历史      :
  1.日    期   : 2013年5月17日
    作    者   : w00176964
    修改内容   : 新增结构
*****************************************************************************/
typedef struct
{
    VOS_UINT32                          ulCmsCode;
    VOS_UINT32                          ulSmsCause;

} AT_CMS_SMS_ERR_CODE_MAP_STRU;

/* Added by w00176964 for SS FDN&Call Control, 2013-5-17, end */


/*****************************************************************************
  8 UNION定义
*****************************************************************************/


/*****************************************************************************
  9 OTHERS定义
*****************************************************************************/


/*****************************************************************************
  10 函数声明
*****************************************************************************/
extern TAF_UINT32 At_ChangeSTKCmdNo(TAF_UINT32 ulCmdType, TAF_UINT8 *ucCmdNo );

VOS_VOID AT_CsUus1InfoEvtIndProc(
    VOS_UINT8                           ucIndex,
    MN_CALL_EVENT_ENUM_U32              enEvent,
    MN_CALL_INFO_STRU                   *pstCallInfo
);

/* Added by A00165503 for DTS2014050700880, 2014-05-07, Begin */
/*****************************************************************************
 函 数 名  : AT_LogPrintMsgProc
 功能描述  : LOG打印消息处理
 输入参数  : pstMsg --- 消息指针
 输出参数  : 无
 返 回 值  : VOS_VOID
*****************************************************************************/
VOS_VOID AT_LogPrintMsgProc(TAF_MNTN_LOG_PRINT_STRU *pstMsg);
/* Added by A00165503 for DTS2014050700880, 2014-05-07, End */


/* Added by f00179208 for At Project, 2011-10-8, Begin */
/*****************************************************************************
 函 数 名  : AT_CsClccInfoEvtIndProc
 功能描述  : 收到CALL上报的CLCC消息的处理
 输入参数  : 无
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2011年10月08日
    作    者   : f00179208
    修改内容   : 新生成函数

*****************************************************************************/
VOS_VOID At_CsAllCallInfoEvtCnfProc(
    MN_AT_IND_EVT_STRU *pstData,
    TAF_UINT16 usLen
);
/* Added by f00179208 for At Project, 2011-10-8, End */


/* Added by A00165503 for AT Project, 2011-10-06, begin */
/*****************************************************************************
 函 数 名  : AT_MnPsEvtProc
 功能描述  : PS域事件处理函数
 输入参数  : pstEvt                     - PS域事件
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_VOID AT_RcvTafPsEvt(
    TAF_PS_EVT_STRU                     *pstEvt
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpErrorInd
 功能描述  :
 输入参数  : pEvtInfo                   - 事件内容(不包含EvtId)
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpErrorInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpActivateCnf
 功能描述  :
 输入参数  : pEvtInfo                   - 事件内容(不包含EvtId)
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpActivateCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpActivateRej
 功能描述  :
 输入参数  : pEvtInfo                   - 事件内容(不包含EvtId)
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpActivateRej(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpManageInd
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpManageInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpActivateInd
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpActivateInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtPdpModifyCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpModifyCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpModifyRej
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpModifyRej(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpModifiedInd
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpModifiedInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpDeactivateCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpDeactivateCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtPdpDeactivatedInd
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtPdpDeactivatedInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/* Deleted by Y00213812 for VoLTE_PhaseI 项目, 2013-07-08, begin */
/* Deleted by Y00213812 for VoLTE_PhaseI 项目, 2013-07-08, end */

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtCallOrigCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtCallOrigCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtCallEndCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtCallEndCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtCallEndCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtCallModifyCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtCallAnswerCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtCallAnswerCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsCallEvtCallHangupCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsCallEvtCallHangupCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetPrimPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetPrimPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetPrimPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetPrimPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);


/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetSecPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetSecPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetSecPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetSecPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvPsSetPdpTftInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetTftInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetTftInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_VOID
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetTftInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetUmtsQosInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetUmtsQosInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetUmtsQosInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetUmtsQosInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetUmtsQosMinInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetUmtsQosMinInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetUmtsQosMinInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetUmtsQosMinInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetDynamicUmtsQosInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetDynamicUmtsQosInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetPdpStateCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetPdpStateCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/* Modified  by l00130025 for DTS2011120604361, 2011-12-19, begin */
/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtCgactQryCnf
 功能描述  : Cgact查询命令返回
 输入参数  : pEvtInfo                   - 事件内容, MN_PS_EVT_STRU去除EvtId
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtCgactQryCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtCgeqnegTestCnf
 功能描述  : Cgeqneg测试命令返回
 输入参数  : pEvtInfo - 事件内容, MN_PS_EVT_STRU去除EvtId
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/

VOS_UINT32 AT_RcvTafPsEvtCgeqnegTestCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
/* Modified  by l00130025 for DTS2011120604361, 2011-12-19, end */

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetPdpStateCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetPdpStateCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetPdpIpAddrInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetPdpIpAddrInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetAnsModeInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetAnsModeInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetAnsModeInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetAnsModeInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetDynamicPrimPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetDynamicPrimPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetDynamicSecPdpContextInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetDynamicSecPdpContextInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetDynamicTftInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetDynamicTftInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetEpsQosInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetEpsQosInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetEpsQosInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetEpsQosInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetDynamicEpsQosInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetDynamicEpsQosInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetDsFlowInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetDsFlowInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtClearDsFlowInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtClearDsFlowInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtConfigDsFlowRptCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtConfigDsFlowRptCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtReportDsFlowInd
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtReportDsFlowInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetPdpAuthInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetPdpAuthInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetPdpAuthInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetPdpAuthInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetPdpDnsInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetPdpDnsInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetPdpDnsInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetPdpDnsInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtSetAuthDataInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtSetAuthDataInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetAuthDataInfoCnf
 功能描述  :
 输入参数  : VOS_VOID                           *pEvtInfo
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetAuthDataInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetGprsActiveTypeCnf
 功能描述  : ID_EVT_TAF_PS_GET_D_GPRS_ACTIVE_TYPE_CNF事件处理函数, 用于PPP拨号
 输入参数  : pEvtInfo                   - 事件内容, MN_PS_EVT_STRU去除EvtId
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetGprsActiveTypeCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtPppDialOrigCnf
 功能描述  : ID_EVT_TAF_PS_PPP_DIAL_ORIG_CNF事件处理函数, 用于PPP拨号
 输入参数  : pEvtInfo                   - 事件内容, MN_PS_EVT_STRU去除EvtId
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtPppDialOrigCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
/* Added by A00165503 for AT Project, 2011-10-06, end */

/* Added by c00173809 for PS Project,2011-12-17,Begin */
#if (FEATURE_ON == FEATURE_IPV6)
/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtReportDsFlowInd
 功能描述  : ID_EVT_TAF_PS_IPV6_INFO_IND事件处理函数, 用于处理IPV6的RA参数
 输入参数  : pEvtInfo                   - 事件内容,
 输出参数  : 无
 返 回 值  : VOS_UINT32
 调用函数  :
 被调函数  :
*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtReportRaInfo(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
#endif
/* Added by c00173809 for PS Project,2011-12-17,End */

/* Added by l60609 for PS Project,2012-12-21,Begin */
VOS_UINT32 AT_RcvTafPsEvtPdpDisconnectInd(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
/* Added by l60609 for PS Project,2012-12-21,End */
/* Added by z00161729 for V7R1 phase IV, 2011-12-8, begin */

VOS_VOID AT_QryParaRspCeregProc(
    VOS_UINT8                           ucIndex,
    VOS_UINT8                           ucOpId,
    VOS_VOID                           *pPara
);
/* Added by z00161729 for V7R1 phase IV, 2011-12-8, end */

/* Added by l00198894 for C50_IPC Project, 2012/02/16, begin */
VOS_UINT32 AT_RcvTafPsEvtGetDynamicDnsInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);

/* Added by l00198894 for C50_IPC Project, 2012/02/16, end */

/* Deleted by l00198894 for V9R1 STK升级, 2013/07/11 */

TAF_VOID  At_StkCsinIndPrint(TAF_UINT8 ucIndex,SI_STK_EVENT_INFO_STRU *pEvent);

/* Add by w00199382 for V7代码同步, 2012-04-07, Begin   */
#if (FEATURE_ON==FEATURE_LTE)
/*DTS2012022307067             : h00135900 start in 2011-02-23*/
VOS_UINT32 atReadLtecsCnfProc(VOS_UINT8   ucIndex,VOS_VOID    *pEvtInfo);
/*DTS2012022307067             : h00135900 end in 2011-02-23*/

/* DTS2012031701487 w00182550 start in 2012-03-20 */
VOS_UINT32 atReadCemodeCnfProc(VOS_UINT8   ucIndex,VOS_VOID    *pEvtInfo);
/* DTS2012031701487 w00182550 end in 2012-03-20 */

/* DTS2012031509135            : x00126983 2012-3-15 AT^PDPROFMOD + */
VOS_UINT32 AT_RcvTafPsEvtSetPdprofInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
/* DTS2012031509135            : x00126983 2012-3-15 AT^PDPROFMOD - */
#endif

/* Add by w00199382 for V7代码同步, 2012-04-07, End   */

/* Added by w00166186 for V7R1C50 AT&T&DCM, 2012-6-11, begin */
VOS_VOID At_RcvMnCallEccNumIndProc(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);

VOS_VOID At_RcvMmaPlmnChangeIndProc(
    TAF_UINT8                           ucIndex,
    TAF_PHONE_EVENT_INFO_STRU           *pEvent
);
VOS_VOID AT_ConvertNasMccToBcdType(
    VOS_UINT32                          ulNasMcc,
    VOS_UINT32                         *pulMcc
);

/* Added by w00166186 for V7R1C50 AT&T&DCM, 2012-6-11, end */

/* Added by L60609 for V7R1C50 AT&T&DCM, 2012-6-19, begin */
VOS_VOID AT_RcvMmaNsmStatusInd(
    TAF_UINT8                           ucIndex,
    TAF_PHONE_EVENT_INFO_STRU          *pEvent
);
/* Added by L60609 for V7R1C50 AT&T&DCM, 2012-6-19, end */

/* Added by l00198894 for STK&DCM Project, 2012/09/18, begin */
/* Modified by l60609 for DSDA Phase III, 2013-3-5, Begin */
VOS_UINT32 AT_CheckRptCmdStatus(
    VOS_UINT8                          *pucRptCfg,
    AT_CMD_RPT_CTRL_TYPE_ENUM_UINT8     enRptCtrlType,
    AT_RPT_CMD_INDEX_ENUM_UINT8         enRptCmdIndex
);
/* Modified by l60609 for DSDA Phase III, 2013-3-5, End */

/* Modified by Y00213812 for VoLTE_PhaseII, 2013-09-18, begin */
/* Modified by l00227485 for DSDA PhaseII, 2012-12-25, begin */
VOS_VOID AT_ReportCCallstateResult(
    VOS_UINT16                          usClientId,
    VOS_UINT8                           ucCallId,
    VOS_UINT8                          *pucRptCfg,
    AT_CS_CALL_STATE_ENUM_UINT8         enCallState,
    TAF_CALL_VOICE_DOMAIN_ENUM_UINT8    enVoiceDomain
);
/* Modified by l00227485 for DSDA PhaseII, 2012-12-25, end */
/* Modified by Y00213812 for VoLTE_PhaseII, 2013-09-18, end */

/* Modified by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-17, begin */
VOS_VOID AT_ReportCCallstateHoldList(
    MN_CALL_EVT_HOLD_STRU              *pstHoldEvt,
    AT_CS_CALL_STATE_ENUM_UINT8         enCallState
);
VOS_VOID AT_ReportCCallstateRetrieveList(
    MN_CALL_EVT_RETRIEVE_STRU          *pstRetrieveEvt,
    AT_CS_CALL_STATE_ENUM_UINT8         enCallState
);
/* Modified by s00217060 for 主动上报AT命令控制下移至C核, 2013-4-17, end */


VOS_VOID AT_ReportCendResult(
    VOS_UINT8                           ucIndex,
    MN_CALL_INFO_STRU                  *pstCallInfo );

VOS_VOID AT_CSCallStateReportProc(
    MN_AT_IND_EVT_STRU                 *pstData );
/* Added by l00198894 for STK&DCM Project, 2012/09/18, end */

/* Added by y00213812 for STK&DCM Project, 2012/09/18, begin */
VOS_VOID At_SetClprCnf(MN_AT_IND_EVT_STRU *pstData);

/* Added by y00213812 for STK&DCM Project, 2012/09/18, end */

VOS_VOID  At_StkCcinIndPrint(TAF_UINT8 ucIndex, SI_STK_EVENT_INFO_STRU *pstEvent);

VOS_VOID At_StkHvsmrIndPrint(VOS_UINT8 ucIndex, SI_STK_EVENT_INFO_STRU *pstEvent);

/* Modified by z00161729 for cerssi and cnmr, 2012-11-21, begin */
VOS_VOID AT_RcvMmaRssiChangeInd(
    TAF_UINT8                           ucIndex,
    TAF_PHONE_EVENT_INFO_STRU          *pEvent
);
/* Modified by z00161729 for cerssi and cnmr, 2012-11-21, end */

/* Deleted by l00198894 for V9R1 STK升级, 2013/07/11 */

/* Added by w00176964 for V7R1C50_DCM接入禁止小区信息上报, 2012-12-11, begin */
VOS_VOID AT_RcvMmaAcInfoChangeInd(
    VOS_UINT8                           ucIndex,
    TAF_PHONE_EVENT_INFO_STRU          *pstEvent
);
/* Added by w00176964 for V7R1C50_DCM接入禁止小区信息上报, 2012-12-11, end */

/* Added by l00208543 for V9R1 RejectInfo Report, 2013-8-15, begin */
VOS_VOID AT_PhRegRejInfoProc(TAF_UINT8* pData,TAF_UINT16 usLen);
/* Added by l00208543 for V9R1 RejectInfo Report, 2013-8-15, end */

/* Added by l00208543 for DTS2013100904573, 2013-10-09, begin */
VOS_VOID AT_PhPlmnSelectInfoProc(TAF_UINT8* pData,TAF_UINT16 usLen);
/* Added by l00208543 for DTS2013100904573, 2013-10-09, end */

/* Added by w00242748 for NETSCAN, 2013-10-16, begin */
VOS_VOID AT_PhNetScanCnfProc(
    TAF_UINT8                          *pData,
    TAF_UINT16                          usLen
);
/* Added by w00242748 for NETSCAN, 2013-10-16, end */

TAF_UINT32 AT_ProcOperModeWhenLteOn(VOS_UINT8 ucIndex);

/* Added by l00171473 for DTS2013010800120 语音带宽信息上报, 2013-1-5, begin */

VOS_VOID AT_RcvMnCallChannelInfoInd(VOS_VOID *pEvtInfo);

/* Added by l00171473 for DTS2013010800120 语音带宽信息上报, 2013-1-5, end */

/* Added by Y00213812 for DTS2013012309236, 2013-01-23, begin */
/**********************************************************
 函 数 名  : At_RcvPlmnQryCnf
 功能描述  : 参数查询结果^PLMN的上报处理
 输入参数  : ucIndex
             OpId
             pPara
 输出参数  : 无
 返 回 值  :
*************************************************************/
VOS_VOID At_RcvPlmnQryCnf(
    VOS_UINT8                           ucIndex,
    VOS_UINT8                           OpId,
    VOS_VOID                           *pPara
);

/*****************************************************************************
 函 数 名  : At_RcvXlemaQryCnf
 功能描述  : 收到CALL紧急呼号码查询的处理
 输入参数  : MN_AT_IND_EVT_STRU   pstData
             VOS_UINT16           usLen
 输出参数  : 无
 返 回 值  : VOS_VOID
*****************************************************************************/
VOS_VOID At_RcvXlemaQryCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);
/* Added by Y00213812 for DTS2013012309236, 2013-01-23, end */

/* Added by l00198894 for DTS2013011803106, 2013/01/18, begin */
/*****************************************************************************
 函 数 名  : AT_GetSsEventErrorCode
 功能描述  : 从SS Event中获取AT命令错误码
 输入参数  : VOS_UINT8                           ucIndex -- AT通道索引号
             TAF_SS_CALL_INDEPENDENT_EVENT_STRU *pEvent  -- SS Event消息
 输出参数  : 无
 返 回 值  : AT_RRETURN_CODE_ENUM_UINT32   -- AT命令错误码
*****************************************************************************/
VOS_UINT32 AT_GetSsEventErrorCode(
    VOS_UINT8                           ucIndex,
    TAF_SS_CALL_INDEPENDENT_EVENT_STRU *pEvent);
/* Added by l00198894 for DTS2013011803106, 2013/01/18, end */

/* Added by s00217060 for V9R1_SVLTE, 2013-6-11, begin */
VOS_VOID AT_RcvUserSrvStateQryCnf(
    VOS_UINT8                           ucIndex,
    VOS_UINT8                           OpId,
    VOS_VOID                           *pPara
);
/* Added by s00217060 for V9R1_SVLTE, 2013-6-11, end */

/* Modified by z00234330 for DTS2013061707906, 2013/6/7, begin */

extern TAF_UINT8 At_GetClckClassFromBsCode(TAF_SS_BASIC_SERVICE_STRU *pstBs);
/* Modified by z00234330 for DTS2013061707906, 2013/6/7, end */

/* Added by l00198894 for V9R1 STK升级, 2013/07/11, begin */
VOS_VOID AT_RcvTafCallStartDtmfCnf(
    MN_AT_IND_EVT_STRU                 *pstData
);

VOS_VOID AT_RcvTafCallStopDtmfCnf(
    MN_AT_IND_EVT_STRU                 *pstData
);

VOS_VOID AT_RcvTafCallStartDtmfRslt(
    MN_AT_IND_EVT_STRU                 *pstData
);

VOS_VOID AT_RcvTafCallStopDtmfRslt(
    MN_AT_IND_EVT_STRU                 *pstData
);
/* Added by l00198894 for V9R1 STK升级, 2013/07/11, end */

/* Added by x65241 for ACC&SPLMN, 2013-10-15 Begin */
VOS_VOID AT_PhEOPlmnQueryCnfProc(TAF_UINT8 *pData);
VOS_VOID AT_PhEOPlmnSetCnfProc(TAF_UINT8 *pData);
/* Added by x65241 for ACC&SPLMN, 2013-10-15 End */

extern VOS_VOID AtBase64Encode(const VOS_VOID* pdata,const VOS_UINT32 data_size,VOS_VOID* out_pcode);

/* Add by j00174725 for V3R3 HSUART PHASEIII, 2013-12-28, Begin */
VOS_VOID AT_FlushSmsIndication(VOS_VOID);
VOS_UINT32  AT_IsClientBlock(VOS_VOID);

/* Add by j00174725 for V3R3 HSUART PHASEIII, 2013-12-28, End */

/* Added by s00217060 for VoLTE_PhaseI  项目, 2013-07-09, begin */
VOS_VOID At_RcvTafCallOrigCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    TAF_UINT16                          usLen
);
VOS_VOID At_RcvTafCallSupsCmdCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    TAF_UINT16                          usLen
);

/* Added by s00217060 for VoLTE_PhaseI  项目, 2013-07-09, end */

/* Added by Y00213812 for VoLTE_PhaseI 项目, 2013-07-08, begin */
/*****************************************************************************
 函 数 名  : AT_RcvTafPsEvtGetCidSdfInfoCnf
 功能描述  : ID_MSG_TAF_GET_CID_SDF_CNF事件处理函数
 输入参数  : VOS_UINT8                  ucIndex,
             VOS_VOID                  *pEvtInfo       - 事件内容, MN_PS_EVT_STRU去除EvtId
 输出参数  : 无
 返 回 值  : VOS_UINT32

*****************************************************************************/
VOS_UINT32 AT_RcvTafPsEvtGetCidSdfInfoCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
/* Added by Y00213812 for VoLTE_PhaseI 项目, 2013-07-08, end */

/* Added by s00217060 for VoLTE_PhaseI  项目, 2013-08-16, begin */
VOS_UINT32 AT_ConvertCallError(TAF_CS_CAUSE_ENUM_UINT32 enCause);
/* Added by s00217060 for VoLTE_PhaseI  项目, 2013-08-16, end */

/* Added by Y00213812 for VoLTE_PhaseII, 2013-09-18, begin */
#if (FEATURE_ON == FEATURE_IMS)
VOS_VOID At_ProcQryClccResult(
    VOS_UINT8                           ucNumOfCalls,
    MN_CALL_INFO_QRY_CNF_STRU          *pstCallInfos,
    VOS_UINT8                           ucIndex
);

/* Added by f00179208 for VTLTE, 2014-06-16, Begin */
VOS_VOID At_RcvTafCallModifyCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);
VOS_VOID At_RcvTafCallAnswerRemoteModifyCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);
VOS_VOID At_RcvTafCallModifyStatusInd(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);
/* Added by f00179208 for VTLTE, 2014-06-16, End */

/* Added by f00179208 for enhanced conference, 2014-11-28, Begin */
VOS_VOID AT_RcvTafEconfDialCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);

VOS_VOID AT_RcvTafEconfNotifyInd(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);

VOS_VOID AT_RcvTafGetEconfInfoCnf(
    MN_AT_IND_EVT_STRU                 *pstData,
    VOS_UINT16                          usLen
);

/* Added by f00179208 for enhanced conference, 2014-11-28, End */
#endif
/* Added by Y00213812 for VoLTE_PhaseII, 2013-09-18, end */

#if (FEATURE_ON == FEATURE_ECALL)
/* Added by j00174725 for V3R3C60_eCall项目, 2014-3-29, begin */
VOS_UINT32 At_ProcVcReportEcallStateEvent(
    VOS_UINT8                           ucIndex,
    APP_VC_EVENT_INFO_STRU             *pstVcEvtInfo
);

VOS_UINT32 At_ProcVcSetEcallCfgEvent(
    VOS_UINT8                           ucIndex,
    APP_VC_EVENT_INFO_STRU             *pstVcEvtInfo
);
/* Added by j00174725 for V3R3C60_eCall项目, 2014-3-29, end */

/* Added by n00269697 for V3R3C60_eCall项目, 2014-3-29, begin */
VOS_UINT32 At_RcvTafCallQryEcallInfoCnf(MN_AT_IND_EVT_STRU *pEvtInfo);
/* Added by n00269697 for V3R3C60_eCall项目, 2014-3-29, end */
#endif

/* Added by n00269697 for V3R3C60_eCall项目, 2014-3-29, begin */
VOS_VOID At_ChangeEcallTypeToCallType(
    MN_CALL_TYPE_ENUM_U8                enEcallType,
    MN_CALL_TYPE_ENUM_U8               *enCallType
);

PS_BOOL_ENUM_UINT8 At_CheckReportCendCallType(
    MN_CALL_TYPE_ENUM_U8                enCallType
);

PS_BOOL_ENUM_UINT8 At_CheckReportOrigCallType(
    MN_CALL_TYPE_ENUM_U8                enCallType
);

PS_BOOL_ENUM_UINT8 At_CheckReportConfCallType(
    MN_CALL_TYPE_ENUM_U8                enCallType
);

PS_BOOL_ENUM_UINT8 At_CheckUartRingTeCallType(
    MN_CALL_TYPE_ENUM_U8                enCallType
);
/* Added by n00269697 for V3R3C60_eCall项目, 2014-3-29, end */

/* Added by x00182239 for DTS2014120310853, 2015-1-14, Begin */
/*****************************************************************************
 函 数 名  : AT_ReportSmMeFull
 功能描述  : 协议栈短信存储域满事件上报的处理
 输入参数  : ucIndex - 用户索引
             enMemStore  - 存储介质类型
 输出参数  : 无
 返 回 值  :
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2015年1月14日
    作    者   : xuechao
    修改内容   : 新生成函数
*****************************************************************************/
VOS_VOID AT_ReportSmMeFull(
    VOS_UINT8                           ucIndex,
    MN_MSG_MEM_STORE_ENUM_U8            enMemStore
);
/* Added by x00182239 for DTS2014120310853, 2015-1-14, End */

TAF_UINT32 At_HexText2AsciiStringSimple(
    TAF_UINT32                          MaxLength,
    TAF_INT8                           *headaddr,
    TAF_UINT8                          *pucDst,
    TAF_UINT32                          ulLen,
    TAF_UINT8                          *pucStr
);

/* Added by n00269697 for VTLTE, 2014-06-16, Begin */
PS_BOOL_ENUM_UINT8 At_CheckOrigCnfCallType(
    MN_CALL_INFO_STRU                  *pstCallInfo,
    VOS_UINT8                           ucIndex
);
/* Added by n00269697 for VTLTE, 2014-06-16, End */


/* Added by z00301431 for DTS2015072702142, 2015-7-29 begin */
VOS_UINT32 AT_RcvTafPsEvtSetImsPdpCfgCnf(
    VOS_UINT8                           ucIndex,
    VOS_VOID                           *pEvtInfo
);
/* Added by z00301431 for DTS2015072702142, 2015-7-29 end */
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


#endif


