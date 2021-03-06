/*****************************************************************************/
/*                                                                           */
/*                Copyright 1999 - 2003, Huawei Tech. Co., Ltd.              */
/*                           ALL RIGHTS RESERVED                             */
/*                                                                           */
/* FileName: v_iddef.inc                                                     */
/*                                                                           */
/* Author:                                                                   */
/*                                                                           */
/* Version:  1.0                                                             */
/*                                                                           */
/* Date:                                                                     */
/*                                                                           */
/* Description: create this file                                             */
/*                                                                           */
/*****************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#ifndef _V_IDDEF_INC
#define _V_IDDEF_INC

/* 单板和PC工程C核请配置这里 */
#if (OSA_CPU_CCPU == VOS_OSA_CPU)
/* FID静态信息表 */
FID_Static_Table_Type g_aFidStaticTable[VOS_FID_BUTT] =
{
    {DOPRA_FID_SYS ,        "fSYS",         VOS_NULL_PTR,           VOS_START_FIRST,    0 ,     0  },
/* 以上是OSA 保留配置请不要动 */
    {WUEPS_FID_OM,          "OM",           WuepsOMFidInit,         VOS_START_THIRD,    6144,   0  },
    {I0_MAPS_USIMM_FID,     "USIMM_FID",    USIMM_FID_Init,         VOS_START_THIRD,    8192,   0  },
    {I0_MAPS_SI_FID,        "SI_FID",       WuepsSIFidInit,         VOS_START_THIRD,    0,      0  },
    {I0_UEPS_FID_SL,        "I0_SL_FID",    LLC_FID_Init,           VOS_START_THIRD,    16384,  0  },
    {UEPS_FID_RM,           "RM_FID",       WTTF_MAC_RLC_UlFidInit, VOS_START_THIRD,    16384,  0  },
    {I0_UEPS_FID_RR,        "I0_RR_FID",    GAS_FidPidInit,         VOS_START_THIRD,    16384,  0  },
    {I0_UEPS_FID_GRM,       "I0_GRM_FID",   GRM_FID_Init,           VOS_START_THIRD,    16384,  0  },
    {WUEPS_FID_HPA,         "HPA_FID",      HPA_FIDInit,            VOS_START_THIRD,    2560,   0  },
    {I0_WUEPS_FID_MM,       "I0_MM_FID",    NAS_MML_FidInit,        VOS_START_THIRD,    16384,  0  },
    {I0_WUEPS_FID_CM,       "I0_CM_FID",    WuepsCmFidInit,         VOS_START_THIRD,    16384,  0  },
    {I0_WUEPS_FID_RABMSM,   "I0_RABMSM_FID",WuepsRabmFidInit,       VOS_START_THIRD,    16384,  0  },
    {I0_WUEPS_FID_TAF,      "I0_TAF_FID",   WuepsTafFidInit,        VOS_START_THIRD,    0,      0  },
    {WUEPS_FID_OM_CALLBACK, "OMCALLBACK",   OMCallBackFidInit,      VOS_START_THIRD,    5632,   0  },
    {WUEPS_FID_SPY,         "SPY",          Spy_FID_Init,           VOS_START_THIRD,    6144,   0  },
    {AGENT_FID_OM,          "AGENT_OM",     OM_AgentFidInit,        VOS_START_THIRD,    0,      0  },
    {UEPS_FID_FLOWCTRL_C,   "CCpuFlowCtrl", FC_CCORE_FidInit,       VOS_START_THIRD,    8192,   0  },
    {UEPS_FID_RM_DL,        "RM_DL_FID",    WTTF_MAC_RLC_DlFidInit, VOS_START_THIRD,    0,      0  },
    {WUEPS_FID_NOSIG,       "OM_NOSIG",     OM_NoSigFidInit,        VOS_START_THIRD,    0,      0  },
    {UEPS_FID_TTF_MEM_RB_FREE, "MMRB_FREE", TTF_MemRbFreeFidInit,   VOS_START_THIRD,    4096,   0  },
    {UEPS_FID_CDS,          "CDS_FID",      CDS_FidInit,            VOS_START_THIRD,    16384,  0  },
    {UEPS_FID_NDCLIENT,     "NCCLIENT_FID", NDCLIENT_FidInit,       VOS_START_THIRD,    8192,   0  },
    {I0_UEPS_FID_PBD,       "I0_PBD_FID",   PBD_FID_Init,           VOS_START_THIRD,    16384,  0  },
    {WUEPS_FID_NVIM_FLUSH,  "NV_FLUSH",     VOS_NULL_PTR,           VOS_START_THIRD,    8192,   0  },
    {WUEPS_FID_SLEEP,       "SLEEP",        WuepsSleepFidInit,      VOS_START_THIRD,    4096,   0  },
    {I0_DSP_FID_APM,        "DSP_APM0",     UPHY_APM_FidInit,       VOS_START_THIRD,    8192,   0  },
    {DSP_FID_UPA,           "DSP_UPA",      UPHY_UCOM_UpaFidInit,   VOS_START_THIRD,    8192,   0  },
    {DSP_FID_WPHY,          "DSP_WPHY",     UPHY_UCOM_WphyFidInit,  VOS_START_THIRD,    8192,   0  },
    {I0_DSP_FID_GPHY,       "DSP_GPHY0",    UPHY_UCOM_GphyFidInit,  VOS_START_THIRD,    8192,   0  },
    {I0_DSP_FID_DRX,        "DSP_DRX0",     UPHY_DRX_FidInit,       VOS_START_THIRD,    8192,   0  },
    {I0_DSP_FID_IDLE,       "DSP_IDLE0",    UPHY_IDLE_FidInit,      VOS_START_THIRD,    8192,   0  },
    {DSP_FID_WMEAS,         "DSP_WMEAS",    UPHY_UCOM_WmeasFidInit, VOS_START_THIRD,    8192,   0  },
    {DSP_FID_RCM,           "DSP_RCM",      RCM_DRV_FidInit,        VOS_START_THIRD,    8192,   0  },
    {DSP_FID_STARTUP,       "DSP_STARTUP",  UPHY_INIT_FidInit,      VOS_START_THIRD,    8192,   0  },
    {UEPS_FID_IPS_CCORE,    "IPS_CCORE",    IPS_MNTN_CCORE_FidInit, VOS_START_THIRD,    8192,   0  },
    {WUEPS_FID_CBPCA,       "CBPCA_FID",    CBPCA_FidInit,          VOS_START_THIRD,    8192,   0  },
    {WUEPS_FID_CSIMA,       "CSIMA_FID",    CSIMA_FidInit,          VOS_START_THIRD,    8192,   0  },
    {WUEPS_FID_CMMCA,       "CMMCA",        CMMCA_FidInit,          VOS_START_THIRD,    4096,   0  },
    {CCPU_FID_CBT,          "CBT",          CBTFidInit,             VOS_START_THIRD,    8192,   0  },
    {CCPU_FID_PAM_OM,       "PAM_OM",       PAMOM_FID_Init,         VOS_START_THIRD,    8192,   0  },
    {UEPS_FID_MTC,          "MTC",          MTC_FidInit,            VOS_START_THIRD,    4096,   0  },

#if ( FEATURE_MULTI_MODEM == FEATURE_ON )
    {I1_MAPS_USIMM_FID,     "USIMM_FID1",   I1_USIMM_FID_Init,      VOS_START_THIRD,    8192,   0  },
    {I1_MAPS_SI_FID,        "SI_FID1",      I1_WuepsSIFidInit,      VOS_START_THIRD,    0,      0  },
    {I1_WUEPS_FID_SLEEP,    "I1_SLEEP",     I1_WuepsSleepFidInit,   VOS_START_THIRD,    4096,   0  },
    {I1_UEPS_FID_RR,        "RR_FID1",      I1_GAS_FidPidInit,      VOS_START_THIRD,    16384,  0  },
    {I1_WUEPS_FID_MM,       "MM_FID1",      I1_NAS_MML_FidInit,     VOS_START_THIRD,    16384,  0  },
    {I1_WUEPS_FID_CM,       "CM_FID1",      I1_WuepsCmFidInit,      VOS_START_THIRD,    16384,  0  },
    {I1_WUEPS_FID_RABMSM,   "RABMSM_FID1",  I1_WuepsRabmFidInit,    VOS_START_THIRD,    16384,  0  },
    {I1_WUEPS_FID_TAF,      "TAF_FID1",     I1_WuepsTafFidInit,     VOS_START_THIRD,    0,      0  },
    {I1_UEPS_FID_SL,        "SL_FID1",      I1_LLC_FID_Init,        VOS_START_THIRD,    16384,  0  },
    {I1_UEPS_FID_PBD,       "PBD_FID1",     I1_PBD_FID_Init,        VOS_START_THIRD,    16384,  0  },
    {I1_UEPS_FID_GRM,       "GRM_FID1",     I1_GRM_FID_Init,        VOS_START_THIRD,    16384,  0  },
    {I1_DSP_FID_APM,        "DSP_APM1",     I1_UPHY_APM_FidInit,    VOS_START_THIRD,    8192,   0  },
    {I1_DSP_FID_GPHY,       "DSP_GPHY1",    I1_UPHY_UCOM_GphyFidInit,VOS_START_THIRD,   8192,   0  },
    {I1_DSP_FID_DRX,        "DSP_DRX1",     I1_UPHY_DRX_FidInit,    VOS_START_THIRD,    8192,   0  },
    {I1_DSP_FID_IDLE,       "DSP_IDLE1",    I1_UPHY_IDLE_FidInit,   VOS_START_THIRD,    8192,   0  },
    {UEPS_FID_RRM,          "RRM_FID",      RRM_FidInit,            VOS_START_THIRD,    8192,   0  },
#endif

#if (RAT_MODE != RAT_GU)
    {MSP_FID_DIAG_CCPU,     "mspDiag",      MSP_DiagFidInit,        VOS_START_THIRD,    8192,   0  },
    {MSP_L4_FID,            "mspL4",        MSP_L4FidInit,          VOS_START_THIRD,    16384,  0  },
    {MSP_FTM_FID,           "mspFTM",       MSP_FtmFidInit,         VOS_START_THIRD,    8192,   0  },
    {PS_FID_RRC,            "LRRC_FID",     RRC_FidInit,            VOS_START_THIRD,    16384,  0  },
    {PS_FID_CM,             "LCM_FID",      CM_FidInit,             VOS_START_THIRD,    16384,  0  },
    {PS_FID_OM,             "LPSOM_FID",    PS_OM_FidInit,          VOS_START_THIRD,    4096,   0  },
    {PS_FID_RM_UL,          "MACRLCUL_FID", LMAC_RLC_UlFidInit,     VOS_START_THIRD,    16384,  0  },
    {PS_FID_RM_DL,          "MACRLCDL_FID", LMAC_RLC_DlFidInit,     VOS_START_THIRD,    16384,  0  },
    {PS_FID_HPA,            "LHPA_FID",     LHPA_FidPidInit,        VOS_START_THIRD,    4096,   0  },
    {PS_FID_IMSA,           "IMSA_FID",     IMSA_FidInit,           VOS_START_THIRD,    16384,  0  },
    {PS_FID_IMSVA,          "IMSVA_FID",    IMSVA_FidInit,          VOS_START_THIRD,    8192,   0  },

#if (FEATURE_ON == FEATURE_UE_MODE_TDS)
    {HL1_FID_SNDCMD,        "SNDCMD_FID",   hl100_SndCmd_FidInit,   VOS_START_THIRD,    0,      0  },
    {TPS_FID_MAC,           "TPS_FID_MAC",  process_mac_FidInit,    VOS_START_THIRD,    0,      0  },
    {TPS_FID_PDC,           "TPS_FID_PDC",  process_pdc_FidInit,    VOS_START_THIRD,    0,      0  },
#endif

#endif

 #if (FEATURE_ON == FEATURE_UE_MODE_CDMA)
    {UEPS_FID_CAS1X,        "CAS1X_FID",    CAS_1X_FidPidInit,      VOS_START_THIRD,    16384,  0  },
    {MSPS_FID_CTTF_1X_REV,  "1X_R_FID",     CTTF_1X_RevFidInit,     VOS_START_THIRD,    0,      0  },
    {MSPS_FID_CTTF_1X_FWD,  "1X_F_FID",     CTTF_1X_FwdFidInit,     VOS_START_THIRD,    0,      0  },
    {DSP_FID_CPROC,         "CPROC_STUB",   DSP_CPROC_FidInit,      VOS_START_THIRD,    0,      0  },
    {CPROC_FID_1X,          "CPROC_1X",     CPROC_1X_FidInit,       VOS_START_THIRD,    8192,   0  },
    {CPROC_FID_EVDO,        "CPROC_EVDO",   CPROC_EVDO_FidInit,     VOS_START_THIRD,    8192,   0  },
    {UEPS_FID_CNAS,         "CNAS_FID",     CNAS_FidInit,           VOS_START_THIRD,    16384,  0  },
#endif
#if (FEATURE_ON == FEATURE_BASTET)
    {UEPS_FID_BASTET,       "BST_FidInit",  BST_FidInit,            VOS_START_THIRD,    4096,   0  },
#endif
#if (VOS_WIN32 == VOS_OS_VER)
    {WUEPS_FID_AT,          "NAS_AT",       WuepsATFidInit,         VOS_START_THIRD,    6144,   0  },
    {ACPU_FID_OM,           "ACPUOM",       OM_AcpuFidInit,         VOS_START_THIRD,    6144,   0  },
    {WUEPS_FID_ITTSTUB,     "ITTSTUB",      WuepsFidITTStubInit,    VOS_START_THIRD,    10240,  0  },
    {ACPU_FID_ADS_UL,       "ADS_UL",       ADS_UL_FidInit ,        VOS_START_THIRD,    0,      0  },
    {ACPU_FID_ADS_DL,       "ADS_DL",       ADS_DL_FidInit ,        VOS_START_THIRD,    0,      0  },
    {ACPU_FID_CSD,          "CSD",          CSD_FidInit,            VOS_START_THIRD,    0,      0  },
    {ACPU_FID_RNIC,         "RNIC",         RNIC_FidInit,           VOS_START_THIRD,    0,      0  },
    {ACPU_FID_TAFAGENT,     "TAF_AGENT",    TAF_AGENT_FidInit,      VOS_START_THIRD,    6144,   0  },
    {UEPS_FID_IMM_RB_FREE,  "IMM_RB_FREE",  IMM_RbMemFreeFidInit,   VOS_START_THIRD,    4096,   0  },
    {MSP_APP_DS_FID,        "mspAppDS",     VOS_NULL_PTR,           VOS_START_THIRD,    8192,   0  },
    {ACPU_FID_CBT,          "CBT",          CBTAppFidInit,          VOS_START_THIRD,    8192,   0  },
    {ACPU_FID_USIMM,        "USIM",         USIMM_APP_FID_Init,     VOS_START_THIRD,    8192,   0  },
    {ACPU_FID_PCVOICE,      "PCVOICE",      PCV_AcpuFidInit,        VOS_START_THIRD,    8192,   0  },
    {ACPU_FID_PAM_OM,       "PAM_OM",       PAMOM_APP_FID_Init,     VOS_START_THIRD,    8192,   0  },
#endif

};
#endif

/* 单板A核请配置这里 */
#if (OSA_CPU_ACPU == VOS_OSA_CPU)
/* FID静态信息表 */
FID_Static_Table_Type g_aFidStaticTable[VOS_FID_BUTT] =
{
    {DOPRA_FID_SYS ,            "fSYS",            VOS_NULL_PTR,            VOS_START_FIRST,    0,     0  },
/* 以上是OSA 保留配置请不要动 */
    {UEPS_FID_IMM_RB_FREE,      "IMM_RB_FREE",     IMM_RbMemFreeFidInit,    VOS_START_THIRD,    4096,  0  },
    {MSP_APP_DS_FID,            "mspAppDS",        DMS_DsFidInit,           VOS_START_THIRD,    8192,  0  },
    {PS_FID_APP_NDIS_PPP_DIPC,  "AppNdisPppDipc",  APP_NDIS_FidInit,        VOS_START_THIRD,    16384, 0  },
    {ACPU_FID_OM_CALLBACK,      "ACPUOMCB",        OM_AcpuCallBackFidInit,  VOS_START_THIRD,    16384, 0  },
    {ACPU_FID_OM,               "ACPU_OM ",        OM_AcpuFidInit,          VOS_START_THIRD,    16384, 0  },
    {ACPU_FID_RNIC,             "RNIC",            RNIC_FidInit ,           VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_NFEXT,            "ACPU_NFEXT",      NFExt_FidInit,           VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_ADS_UL,           "ADS_UL",          ADS_UL_FidInit ,         VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_ADS_DL,           "ADS_DL",          ADS_DL_FidInit ,         VOS_START_THIRD,    5120,  0  },
    {ACPU_FID_CSD,              "CSD",             CSD_FidInit ,            VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_TAFAGENT,         "TAF_AGENT",       TAF_AGENT_FidInit ,      VOS_START_THIRD,    16384, 0  },
    {ACPU_FID_CPULOAD,          "CpuLoadMonitor",  CPULOAD_FidInit ,        VOS_START_THIRD,    4096,  0  },
    {UEPS_FID_FLOWCTRL_A,       "ACpuFlowCtrl",    FC_ACORE_FidInit ,       VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_CBT,              "CBT",             CBTAppFidInit,           VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_USIMM,            "USIM",            USIMM_APP_FID_Init,      VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_PCVOICE,          "PCVOICE",         PCV_AcpuFidInit,         VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_PAM_OM,           "PAM_OM",          PAMOM_APP_FID_Init,      VOS_START_THIRD,    8192,  0  },

#if (RAT_MODE != RAT_GU)
    {MSP_FID_DIAG_ACPU,         "mspAppDiag",      MSP_AppDiagFidInit,       VOS_START_THIRD,   16384, 0  },
#endif

#if (FEATURE_VOICE_UP == FEATURE_ON)
    {ACPU_FID_MED_RT,           "MED_Voice_RT",    VOICE_RtFidInit,         VOS_START_THIRD,    8192,  0  },
    {ACPU_FID_MED,              "MED_Voice",       VOICE_NormalFidInit,     VOS_START_THIRD,    16384, 0  },
#endif

#if (FEATURE_ON == FEATURE_BASTET)
    {ACPU_FID_BASTET_COMM,      "BASTET_COMM",     BASTET_CommFidInit,      VOS_START_THIRD,    8192,  0  },
#endif

};
#endif

#endif /* _V_IDDEF_INC */

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */


