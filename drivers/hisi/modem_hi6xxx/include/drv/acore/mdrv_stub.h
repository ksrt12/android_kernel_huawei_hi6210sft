#ifndef __MDRV_ACORE_STUB_H__
#define __MDRV_ACORE_STUB_H__
#ifdef __cplusplus
extern "C"
{
#endif

#include <mdrv_memory_common.h>
#include "mdrv_misc.h"
#include "mdrv_udi.h"
#include "mdrv_pm.h"
#include "mdrv_chg.h"

#if defined(CHIP_BB_HI6210)
/*****************************************************************************
 * �� �� ��  : BSP_MALLOC
 *
 * ��������  : BSP ��̬�ڴ����
 *
 * �������  : sz: ����Ĵ�С(byte)
 *             flags: �ڴ�����(�ݲ�ʹ��,Ԥ��)
 * �������  : ��
 * �� �� ֵ  : ����������ڴ�ָ��
 *****************************************************************************/
static inline void* BSP_MALLOC(unsigned int u32Size, MEM_POOL_TYPE enFlags) {
    return 0;
}

/*****************************************************************************
 * �� �� ��  : BSP_FREE
 *
 * ��������  : BSP ��̬�ڴ��ͷ�
 *
 * �������  : ptr: ��̬�ڴ�ָ��
 * �������  : ��
 * �� �� ֵ  : ��
 *****************************************************************************/
static inline void  BSP_FREE(void* pMem) {
}

/*****************************************************************
Function: free_mem_size_get
Description:
    get mem size
Input:
    N/A
Output:
    N/A
Return:
    free mem size
*******************************************************************/
static inline unsigned int FREE_MEM_SIZE_GET(void)
{
    return 0;
}

static inline int BSP_DLOAD_GetDloadType(void)
{
    return 1;
}


/*****************************************************************************
 Function   : BSP_PWC_SetTimer4WakeSrc
 Description: ����timer4��Ϊ����Դ
 Input      :
 Return     : void
 Other      :
*****************************************************************************/
static inline void BSP_PWC_SetTimer4WakeSrc(void)
{
    return;
}


/*****************************************************************************
 Function   : BSP_PWC_DelTimer4WakeSrc
 Description: ����timer4����Ϊ����Դ
 Input      :
            :
 Return     : void
 Other      :
*****************************************************************************/
static inline void BSP_PWC_DelTimer4WakeSrc(void)
{
    return;
}

/*****************************************************************************
 �� �� ��  : DRV_PWRCTRL_SLEEPVOTE_LOCK
 ��������  : �����ֹ˯��ͶƱ�ӿڡ�
 �������  : enClientId:PWC_CLIENT_ID_E
 �������  : None
 �� �� ֵ  : PWC_PARA_INVALID/PWC_SUCCESS
*****************************************************************************/
static inline unsigned int  DRV_PWRCTRL_SLEEPVOTE_LOCK(PWC_CLIENT_ID_E enClientId)
{
    return 0;
}

/*****************************************************************************
 �� �� ��  : DRV_PWRCTRL_SLEEPVOTE_UNLOCK
 ��������  : ��������˯��ͶƱ�ӿڡ�
 �������  : enClientId:PWC_CLIENT_ID_E
 �������  : None
 �� �� ֵ  : PWC_PARA_INVALID/PWC_SUCCESS
*****************************************************************************/
static inline unsigned int  DRV_PWRCTRL_SLEEPVOTE_UNLOCK(PWC_CLIENT_ID_E enClientId)
{
    return 0;
}

/*****************************************************************************
 �� �� ��  : BSP_PWRCTRL_StandbyStateCcpu/BSP_PWRCTRL_StandbyStateAcpu
 ��������  : AT^PSTANDBY
 �������  :
 �������  :
 ����ֵ��
*****************************************************************************/
static inline unsigned int DRV_PWRCTRL_STANDBYSTATEACPU(unsigned int ulStandbyTime, unsigned int ulSwitchtime)
{
    return 0;
}

/*dload begin*/

#if 1
/* Begin: DRVΪOAMʵ��NV���ݻָ����̶��ṩ�Ľӿ� */

/********************************************************************************************************
 �� �� ��  : DRV_UPGRADE_NV_SET_RESULT
 ��������  : OAMͨ���ýӿڸ��ߵ���NV�����ɹ��˻���ʧ����
 �������  : status��0      ��ʾNV�����ɹ�
                     ����ֵ ��ʾNV����ʧ��
 �������  :
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ����ע��  : V3R3 SFT �� V9 BBIT ��׮
             V9 SFT  ʵ��
                ���NV�����ɹ���������fastboot�������óɹ���ʶ
                ���NV����ʧ�ܣ�������fastboot��������ʧ�ܱ�ʶ

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �����ɺ���
********************************************************************************************************/
static inline int DRV_UPGRADE_NV_SET_RESULT(int status)
{
    return 0;
}

/********************************************************************************************************
 �� �� ��  : DRV_NV_FILE_SEC_CHECK
 ��������  : ���ļ�����У��
 �������  : pnvFilePath����У���ļ���·��
 �������  :
 �� �� ֵ  : 0 :    �ļ�У����ȷ
             -1��   �ļ�У��ʧ��
 ����ע��  : Ŀǰ�������߱���ֱ�Ӵ�׮���Ժ�ʵ��

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �����ɺ���
********************************************************************************************************/
static inline int DRV_NV_FILE_SEC_CHECK(char * pnvFilePath)
{
    return 0;
}

/********************************************************************************************************
 �� �� ��  : BSP_DLOAD_NVBackupRead
 ��������  : ��Flash�е�NV�������ȡ���ݣ�ʵ��NV��Ļָ����ܡ�
 �������  : len����NV�������ʼ����ʼ����Ҫ��ȡ��NV��ȣ��ֽ�������
 �������  : pRamAddr��Ŀ��RAM��ַ�����ڴ�Ŷ�����NV�����ݡ�
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ����ע��  : V3R3 SFT �� V9 BBIT ����ԭ��
             V9 SFT  ������nand�ӿ��޸�Ϊ����emmc�ӿڶ�дNV������

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �޸ĺ���
********************************************************************************************************/
static inline int DRV_NVBACKUP_READ(unsigned char *pRamAddr, unsigned int len)
{
    return 0;
}

/********************************************************************************************************
 �� �� ��  : BSP_DLOAD_NVBackupWrite
 ��������  : ���ض�����д��Flash�е�NV�������ʵ��NV��ı��ݹ��ܡ�
 �������  : pRamAddr��ԴRAM��ַ�����ڴ����Ҫд���NV�����ݡ�
             len����NV�������ʼ����ʼ����Ҫд���NV��ȣ��ֽ�������
 �������  : �ޡ�
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ����ע��  : V3R3 SFT �� V9 BBIT ����ԭ��
             V9 SFT  ������nand�ӿ��޸�Ϊ����emmc�ӿڶ�дNV������

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �޸ĺ���
********************************************************************************************************/

static inline int DRV_NVBACKUP_WRITE(unsigned char *pRamAddr, unsigned int len)
{
    return 0;
}





#endif

static inline  unsigned int DRV_SDMMC_GET_OPRT_STATUS(void)
{
    return 0;
}

static inline unsigned int DRV_SDMMC_USB_STATUS(void)
{
    return 0;
}

/*ԭdrv_wifi.h*/
unsigned int USB_ETH_DrvSetHostAssembleParam(unsigned int ulHostOUtTimeout);
unsigned int USB_ETH_DrvSetDeviceAssembleParam(unsigned int ulEthTxMinNum,
                                                unsigned int ulEthTxTimeout,
                                                unsigned int ulEthRxMinNum,
                                                unsigned int ulEthRxTimeout);


/*************************************************
 �� �� ��   : BSP_MNTN_ProductTypeGet
 ��������   : ���ص�ǰ��Ʒ����
 �������   : ��
 �������   : ��
 �� �� ֵ   :0:STICK
             1:MOD
             2:E5
             3:CPE
*************************************************/
extern unsigned int BSP_MNTN_ProductTypeGet(void);
#define DRV_PRODUCT_TYPE_GET()   BSP_MNTN_ProductTypeGet()

#if 1 /*drv_usb.h*/

typedef enum
{
    WWAN_WCDMA    = 1,/*WCDMAģʽ*/
    WWAN_CDMA    = 2  /*CDMAģʽ*/
}NDIS_WWAN_MODE;

/*�ܽ��źŵĶ���*/
#define IO_CTRL_FC                      (0x02)
#define IO_CTRL_DSR                     (0x20)
#define IO_CTRL_DTR                     (0x01)
#define IO_CTRL_RFR                     (0x04)
#define IO_CTRL_CTS                     (0x10)
#define IO_CTRL_RI                      (0x40)
#define IO_CTRL_DCD                     (0x80)

/* �豸ö�����˿ڸ��� */
#define DYNAMIC_PID_MAX_PORT_NUM        17

/* NV��50091�ṹ�������豸��Ҫö�ٵĶ˿���̬ */
typedef struct
{
    unsigned int ulStatus;
    unsigned char aucFirstPortStyle[DYNAMIC_PID_MAX_PORT_NUM];  /* �豸�л�ǰ�˿���̬ */
    unsigned char aucRewindPortStyle[DYNAMIC_PID_MAX_PORT_NUM]; /* �豸�л���˿���̬ */
    unsigned char reserved[22];
}DRV_DYNAMIC_PID_TYPE_STRU;

typedef unsigned int (*MNTN_ERRLOGREGFUN)(char * cFileName,unsigned int ulFileId, unsigned int ulLine,
                unsigned int ulErrNo, void * pBuf, unsigned int ulLen);

/*****************************************************************************
 �� �� ��  : BSP_USB_SetPid
 ��������  :
 �������  : �ޡ�
 �������  : �ޡ�
 �� �� ֵ  : �ޡ�
*****************************************************************************/
extern int BSP_USB_SetPid(unsigned char u2diagValue);
#define DRV_SET_PID(u2diagValue)    BSP_USB_SetPid(u2diagValue)

/*****************************************************************************
��������   BSP_USB_PortTypeQuery
��������:  ��ѯ��ǰ���豸ö�ٵĶ˿���ֵ̬
��������� stDynamicPidType  �˿���̬
��������� stDynamicPidType  �˿���̬
����ֵ��   0:    ��ѯ�ɹ�
           ��������ѯʧ��
*****************************************************************************/
unsigned int BSP_USB_PortTypeQuery(DRV_DYNAMIC_PID_TYPE_STRU *pstDynamicPidType);
#define DRV_SET_PORT_QUIRY(a)   BSP_USB_PortTypeQuery(a)

/*****************************************************************************
��������   BSP_USB_PortTypeValidCheck
��������:  �ṩ���ϲ��ѯ�豸�˿���̬���úϷ��Խӿ�
           1���˿�Ϊ��֧�����ͣ�2������PCUI�ڣ�3�����ظ��˿ڣ�4���˵���������16��
           5����һ���豸��ΪMASS��
��������� pucPortType  �˿���̬����
           ulPortNum    �˿���̬����
����ֵ��   0:    �˿���̬�Ϸ�
           �������˿���̬�Ƿ�
*****************************************************************************/
unsigned int BSP_USB_PortTypeValidCheck(unsigned char *pucPortType, unsigned int ulPortNum);
#define DRV_USB_PORT_TYPE_VALID_CHECK(pucPortType, ulPortNum)  \
                    BSP_USB_PortTypeValidCheck(pucPortType, ulPortNum)

/*****************************************************************************
��������   BSP_USB_GetAvailabePortType
��������:  �ṩ���ϲ��ѯ��ǰ�豸֧�ֶ˿���̬�б��ӿ�
��������� ulPortMax    Э��ջ֧�����˿���̬����
�������:  pucPortType  ֧�ֵĶ˿���̬�б�
           pulPortNum   ֧�ֵĶ˿���̬����
����ֵ��   0:    ��ȡ�˿���̬�б��ɹ�
           ��������ȡ�˿���̬�б�ʧ��
*****************************************************************************/
unsigned int BSP_USB_GetAvailabePortType(unsigned char *pucPortType,
                            unsigned int *pulPortNum, unsigned int ulPortMax);
#define DRV_USB_GET_AVAILABLE_PORT_TYPE(pucPortType, pulPortNum, ulPortMax)  \
                BSP_USB_GetAvailabePortType(pucPortType, pulPortNum, ulPortMax)

/*****************************************************************************
 �� �� ��  : BSP_USB_GetDiagModeValue
 ��������  : ����豸���͡�
 �������  : �ޡ�
 �������  : ucDialmode:  0 - ʹ��Modem����; 1 - ʹ��NDIS����; 2 - Modem��NDIS����
              ucCdcSpec:   0 - Modem/NDIS������CDC�淶; 1 - Modem����CDC�淶;
                           2 - NDIS����CDC�淶;         3 - Modem/NDIS������CDC�淶
 �� �� ֵ  : VOS_OK/VOS_ERR
*****************************************************************************/
extern int BSP_USB_GetDiagModeValue(unsigned char *pucDialmode,
                             unsigned char *pucCdcSpec);
#define DRV_GET_DIAG_MODE_VALUE(pucDialmode, pucCdcSpec)    \
                             BSP_USB_GetDiagModeValue(pucDialmode, pucCdcSpec)

/*****************************************************************************
 �� �� ��  : BSP_USB_GetPortMode
 ��������  : ��ȡ�˿���̬ģʽ�����ضԽ����󣬴�׮��
 �������  : ��
 �������  :
 �� �� ֵ  :
*****************************************************************************/
extern unsigned char BSP_USB_GetPortMode(char*PsBuffer, unsigned int*Length );
#define DRV_GET_PORT_MODE(PsBuffer, Length)    BSP_USB_GetPortMode(PsBuffer,Length)

/*****************************************************************************
 �� �� ��  : BSP_USB_GetU2diagDefaultValue
 ��������  : ��ö˿�Ĭ��u2diagֵ
 �������  : �ޡ�
 �������  : �ޡ�
 ����ֵ��   u2diagֵ

*****************************************************************************/
extern unsigned int BSP_USB_GetU2diagDefaultValue(void);
#define DRV_GET_U2DIAG_DEFVALUE()    BSP_USB_GetU2diagDefaultValue()

/*****************************************************************************
 �� �� ��  : ErrlogRegFunc
 ��������  : USB MNTNע���쳣��־�ӿڣ�Porting��Ŀ�д�׮
                Added by c00204787 for errorlog,20120211
 �������  : �ޡ�
 �������  : �ޡ�
 ����ֵ��   ��

*****************************************************************************/
/*extern void ErrlogRegFunc(MNTN_ERRLOGREGFUN pRegFunc);
 */
extern void MNTN_ERRLOG_REG_FUNC(MNTN_ERRLOGREGFUN pRegFunc);

/*****************************************************************************
* �� �� ��  : DRV_DSP_AHB_RESET_CANCEL
* ��������  : ZSP AHB���߽⸴λ
* �������  : ��
* �������  : ��
* �� �� ֵ  : ��
* ����˵��  : ��
*****************************************************************************/
extern void DRV_DSP_AHB_RESET_CANCEL(void);

/*****************************************************************************
 �� �� ��  : BSP_USB_UdiagValueCheck
 ��������  : ���ӿ����ڼ��NV����USB��ֵ̬�ĺϷ���.
 �������  : �ޡ�
 �������  : �ޡ�
 �� �� ֵ  : 0��OK��
            -1��ERROR
*****************************************************************************/
extern int BSP_USB_UdiagValueCheck(unsigned int DiagValue);
#define DRV_UDIAG_VALUE_CHECK(DiagValue)     BSP_USB_UdiagValueCheck(DiagValue)
#define DRV_U2DIAG_VALUE_CHECK(DiagValue)     BSP_USB_UdiagValueCheck(DiagValue)

/*****************************************************************************
 �� �� ��  : BSP_USB_GetLinuxSysType
 ��������  : ���ӿ����ڼ��PC���Ƿ�ΪLinux���Թ��Linux��̨���β���ʧ�ܵ�����.
 �������  : �ޡ�
 �������  : �ޡ�
 �� �� ֵ  : 0��Linux��
            -1����Linux��
*****************************************************************************/
extern int BSP_USB_GetLinuxSysType(void) ;
#define DRV_GET_LINUXSYSTYPE()    BSP_USB_GetLinuxSysType()

/********************************************************
����˵����Э��ջע��HSICʹ��֪ͨ�ص�����
��������:
���������pFunc: HSICʹ�ܻص�����ָ��
�����������
�����������
����ֵ��  0���ɹ�
          1��ʧ��
********************************************************/
typedef void (*HSIC_UDI_ENABLE_CB_T)(void);
extern unsigned int BSP_HSIC_RegUdiEnableCB(HSIC_UDI_ENABLE_CB_T pFunc);
#define DRV_HSIC_REGUDI_ENABLECB(x) BSP_HSIC_RegUdiEnableCB(x)

/********************************************************
����˵����Э��ջע��HSICȥʹ��֪ͨ�ص�����
��������:
���������pFunc: HSICȥʹ�ܻص�����ָ��
�����������
�����������
����ֵ��  0���ɹ�
          1��ʧ��
********************************************************/
typedef void (*HSIC_UDI_DISABLE_CB_T)(void);
extern unsigned int BSP_HSIC_RegUdiDisableCB(HSIC_UDI_DISABLE_CB_T pFunc);
#define DRV_HSIC_REGUDI_DISABLECB(x) BSP_HSIC_RegUdiDisableCB(x)
/********************************************************
����˵����Э��ջ��ѯHSICö��״̬
��������:
�����������
�����������
�����������
����ֵ��  1��ö�����
          0��ö��δ���
********************************************************/
extern unsigned int BSP_GetHsicEnumStatus(void);
#define DRV_GET_HSIC_ENUM_STATUS() BSP_GetHsicEnumStatus()

/*****************************************************************************
* �� �� ��  : BSP_NCM_Write_Ready
* ��������  : ׼����������
* �������  : void
* �������  :
* �� �� ֵ  : void
* �޸ļ�¼  :
*****************************************************************************/
void BSP_NCM_Write_Ready(void);

/********************************************************
����˵���� ���ص�ǰNV���е��豸��ֵ̬���Ƿ�֧��PCSC
��������:
��������� dev_type���豸��ֵ̬��OAM��NV�ж�����

��������� �ޡ�
����ֵ��
          0: dev_type��֧��PCSC�豸��
          1��dev_type֧��PCSC�豸
********************************************************/
/*unsigned int pcsc_info_quiry(unsigned int ulDevType);*/
#define DRV_USB_PCSC_INFO_QUIRY(a)            MDRV_OK

typedef unsigned int (*pFunAPDUProcess)(unsigned int CmdType, unsigned char *pApdu, unsigned int ApduLen);
typedef unsigned int (*GetCardStatus)(void);

typedef struct
{
    pFunAPDUProcess   pFuncApdu;
    GetCardStatus  pFuncGetCardStatus;
}BSP_CCID_REGFUNC;

/*****************************************************************************
 �� �� ��  : pcsc_usim_int
 ��������  : ע��PCSC������պ���
 �������  :
 �������  : None
 �� �� ֵ  : void
*****************************************************************************/
extern void pcsc_usim_int(pFunAPDUProcess pFun1, GetCardStatus pFun2);
#define DRV_PCSC_REG_CALLBACK(pFun1, pFun2) pcsc_usim_int((pFun1), (pFun2))


/*****************************************************************************
 �� �� ��  : pcsc_usim_ctrl_cmd
 ��������  : PCSC����ظ�����
 �������  :
 �������  : None
 �� �� ֵ  : void
*****************************************************************************/
extern unsigned int pcsc_usim_ctrl_cmd(unsigned int CmdTpye, unsigned int Result,
                                        unsigned char *Buf, unsigned int Length);
#define DRV_PCSC_SEND_DATA(CmdType, Result, Buffer, Length) pcsc_usim_ctrl_cmd(CmdType, Result, Buffer, Length)

/* Porting���࣬��ʱ����Ϊ�� */
#define DRV_COM_RCV_CALLBACK_REGI(uPortNo,pCallback)    MDRV_OK

/************************************************************************
 * FUNCTION
 *       rndis_app_event_dispatch
 * DESCRIPTION
 *       ���翨�汾����Ӧ�ý��в��Ż�Ͽ���������
 * INPUTS
 *       ���в��Ż�Ͽ�����ָʾ
 * OUTPUTS
 *       NONE
 *************************************************************************/
extern void DRV_AT_SETAPPDAILMODE(unsigned int ulStatus);


/************************************************************************
 * FUNCTION
 *       PDP_ACT_DRV_CALLBACK
 * DESCRIPTION
 *       PDP�������õ����Ļص�������ԭ�������翨���������ط��ű���Ϣ�����ڴ�׮
 * INPUTS
 *       NONE
 * OUTPUTS
 *       NONE
 *************************************************************************/
extern int PDP_ACT_DRV_CALLBACK(void);

typedef void(* USB_NET_DEV_SWITCH_GATEWAY)
(
    void
);

/*****************************************************************************
 �� �� ��  : BSP_USB_NASSwitchGatewayRegExtFunc
 ��������  : ���ӿ�����NASע���л�����֪ͨ�ص�����
 �������  :�ص��ӿڡ�
 �������  : �ޡ�
 �� �� ֵ  : 0���ɹ�
                       ����:ʧ��
*****************************************************************************/
extern int BSP_USB_NASSwitchGatewayRegFunc(USB_NET_DEV_SWITCH_GATEWAY switchGwMode) ;
#define DRV_USB_NAS_SWITCH_GATEWAY_REGFUNC(switchGwMode)    BSP_USB_NASSwitchGatewayRegFunc(switchGwMode)
/*���翨��δ���� ����ʱAPI ��׮end*/

/************************************����궨��Ϊ�յĺ���******************************************/
/*****************************************************************************
 �� �� ��  : UsbStatusCallbackRegister
 ��������  : ��¼pCallBack��ȫ�ֱ����С�SD��������ؽӿڣ�V7�ȴ�׮
 �������  : �ޡ�
 �������  : �ޡ�
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
*****************************************************************************/
 /*extern int  UsbStatusCallbackRegister(pUsbState pCallBack);*/
#define DRV_USB_STATUS_CALLBACK_REGI(pCallBack)              MDRV_OK

/********************************************************************************************************
 �� �� ��  : USB_otg_switch_signal_set
 ��������  : ����BALONG��K3�Խ�ʱUSBͨ���л�GPIO���ſ���
 �������  : ��š����źź�ֵ
 �������  : ��

 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
********************************************************************************************************/
extern int USB_otg_switch_signal_set(unsigned char ucGroup, unsigned char ucPin, unsigned char ucValue);
#define DRV_GPIO_SET(group, pin, value) USB_otg_switch_signal_set(group, pin, value)

#define DRV_GPIO_USB_SWITCH     200
#define DRV_GPIO_HIGH           1
#define DRV_GPIO_LOW            0

/********************************************************************************************************
 �� �� ��  : USB_otg_switch_set
 ��������  : ����BALONG��K3�Խ�ʱMODEM�࿪����ر�USB PHY
 �������  : ������ر�
 �������  : ��

 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
********************************************************************************************************/
extern int USB_otg_switch_set(unsigned char ucValue);
#define DRV_USB_PHY_SWITCH_SET(value) USB_otg_switch_set(value)

#define AT_USB_SWITCH_SET_VBUS_VALID            1
#define AT_USB_SWITCH_SET_VBUS_INVALID          2

#define USB_SWITCH_ON       1
#define USB_SWITCH_OFF      0

/********************************************************************************************************
 �� �� ��  : USB_otg_switch_get
 ��������  : ����BALONG��K3�Խ�ʱMODEM��USB PHY״̬��ѯ
 �������  : ��
 �������  : ����USB PHY����״̬

 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
********************************************************************************************************/
extern int USB_otg_switch_get(unsigned char *pucValue);
#define DRV_USB_PHY_SWITCH_GET(value) USB_otg_switch_get(value)



#define UART_IOCTL_SET_WRITE_CB      0x7F001000
#define UART_IOCTL_SET_READ_CB       0x7F001001
#define UART_IOCTL_SET_EVT_CB        0x7F001002
#define UART_IOCTL_SET_FREE_CB       0x7F001003
#define UART_IOCTL_WRITE_ASYNC       0x7F001010
#define UART_IOCTL_GET_RD_BUFF       0x7F001011
#define UART_IOCTL_RETURN_BUFF       0x7F001012
#define UART_IOCTL_RELLOC_READ_BUFF  0x7F001013
#define UART_IOCTL_SEND_BUFF_CAN_DMA 0x7F001014
#define UART_IOCTL_SET_WRITE_TIMEOUT 0x7F001015
#define UART_IOCTL_SET_BAUD           0X7F001016
#define UART_IOCTL_SET_WLEN           0X7F001017
#define UART_IOCTL_SET_STP2           0X7F001018
#define UART_IOCTL_SET_EPS            0X7F001019
#define UART_IOCTL_IS_IMPORT_DONE    0x7F001020
#define UDI_UART_IOCTL_SET_READ_CB           UART_IOCTL_SET_READ_CB
#define UDI_UART_IOCTL_GET_READ_BUFFER_CB    UART_IOCTL_GET_RD_BUFF
#define UDI_UART_IOCTL_RETUR_BUFFER_CB       UART_IOCTL_RETURN_BUFF
#define UART_IOCTL_SET_MSC_READ_CB 0x7F001021
#define UART_IOCTL_MSC_WRITE_CMD 0x7F001022
#define UART_IOCTL_SWITCH_MODE_CB 0x7F001023
#define UART_IOCTL_SET_FLOW_CONTROL 0x7F001024
#define UART_IOCTL_SET_AC_SHELL 0x7F001025

/* UART UDI ������*/
#define UART_UDI_ERROR_BASE               0x80000000

#define UART_ERR_MODULE_NOT_INITED      ((int)(BSP_DEF_ERR(BSP_MODU_UART, (int) BSP_ERR_MODULE_NOT_INITED) | UART_UDI_ERROR_BASE))
#define UART_ERR_NULL_PTR      ((int)(BSP_DEF_ERR(BSP_MODU_UART, (int) BSP_ERR_NULL_PTR) | UART_UDI_ERROR_BASE))
#define UART_ERR_INVALID_PARA      ((int)(BSP_DEF_ERR(BSP_MODU_UART, (int) BSP_ERR_INVALID_PARA) | UART_UDI_ERROR_BASE))
#define UART_ERR_RETRY_TIMEOUT      ((int)(BSP_DEF_ERR(BSP_MODU_UART, (int) BSP_ERR_RETRY_TIMEOUT) | UART_UDI_ERROR_BASE))
#define UART_ERR_BUF_ALLOC_FAILED      ((int)(BSP_DEF_ERR(BSP_MODU_UART, (int) BSP_ERR_BUF_ALLOC_FAILED) | UART_UDI_ERROR_BASE))
#define UART_ERR_BUF_FREE_FAILED      ((int)(BSP_DEF_ERR(BSP_MODU_UART, (int) BSP_ERR_BUF_FREE_FAILED) | UART_UDI_ERROR_BASE))

#define UART_A_SHELL  (0x5A5A5A5A)
#define UART_C_SHELL  (0xA5A5A5A5)

/* UART����λ��*/
typedef enum
{
    WLEN_5_BITS,
    WLEN_6_BITS,
    WLEN_7_BITS,
    WLEN_8_BITS,
    WLEN_MAX
}UART_WLEN_ENUM;

/* UARTֹͣλ��*/
typedef enum
{
    STP2_OFF,
    STP2_ON,
    STP2_MAX
}uart_stp2_enum;

/* UARTУ�鷽ʽ*/
typedef enum
{
    PARITY_NO_CHECK,
    PARITY_CHECK_ODD,        /*��У��*/
    PARITY_CHECK_EVEN,        /*żУ��*/
    PARITY_CHECK_MARK,        /*У��λʼ��Ϊ1*/
    PARITY_CHECK_SPACE,       /*У��λʼ��Ϊ0*/
    PARITY_CHECK_MAX
}uart_parity_enum;

/* �첽�����շ��ṹ */
typedef struct tagUART_WR_ASYNC_INFO
{
    char* pBuffer;
    unsigned int u32Size;
    void* pDrvPriv;
}uart_wr_async_info;
/* ��buffer������Ϣ */
typedef struct tagUART_READ_BUFF_INFO
{
    unsigned int u32BuffSize;
    unsigned int u32BuffNum;
}uart_read_buff_info;
/* ���ؿ��ؽṹ */
typedef union
{
    unsigned int      value;
    struct
    {
        unsigned int  reserved1 : 14;/* bit[0-13] : reversed */
        unsigned int  rtsen         : 1;/* bit[14] : rts enable, up stream flow ctrl  */
        unsigned int  ctsen         : 1;/* bit[15] : cts enable, down stream flow ctrl */
        unsigned int  reserved2 : 16;/* bit[16-31] : reversed */
    } reg;
} uart_flow_ctrl_union;

typedef void (*uart_free_cb_t)(char* pBuff);
typedef void (*uart_read_cb_t)(void);
typedef void (*uart_msc_read_cb_t)(MODEM_MSC_STRU* pModemMsc);
typedef void (*uart_switch_mode_cb_t)(void);



typedef struct tagNCM_PKT_S
{
    unsigned char *pBuffer;       /* bufferָ��*/
    unsigned int  u32BufLen;      /* buffer���� */
}NCM_PKT_S;

/* NCM�豸����ö��*/
typedef enum tagNCM_DEV_TYPE_E
{
    NCM_DEV_DATA_TYPE,      /* ����ͨ�����ͣ�PSʹ��*/
    NCM_DEV_CTRL_TYPE       /* ����ͨ����MSP����AT����ʹ��*/
}NCM_DEV_TYPE_E;


/* ����װ�ͷź���ָ�� */
typedef void (*USBFreePktEncap)(void *PktEncap);




/* ����װ�ṹ��,NCM_IOCTL_SET_PKT_ENCAP_INFO�����ֶ�Ӧ�����ṹ��*/
typedef struct tagNCM_PKT_ENCAP_INFO_S
{
    int s32BufOft;      /* bufƫ���� */
    int s32LenOft;      /* lenƫ���� */
    int s32NextOft;     /* nextƫ���� */
}NCM_PKT_ENCAP_INFO_S;


typedef struct tagNCM_IPV6_DNS_NTF_S
{
    unsigned char* pu8Ipv6DnsNtf;
    unsigned int u32Length;
} NCM_AT_IPV6_DNS_NTF_S;


/*  �´��ϴ������ذ��ĸ���ͳ����Ϣ��
    �����͸�PC�������ȴ�PC�Ķ�ȡ,NCM_IOCTL_SET_PKT_STATICS�����ֶ�Ӧ�����ṹ*/
typedef struct tagNCM_IOCTL_PKT_STATISTICS_S
{
    unsigned int     u32TxOKCount;         /*���Ͱ���*/
    unsigned int     u32RxOKCount;         /*���հ���*/
    unsigned int     u32TxErrCount;        /*���ʹ���*/
    unsigned int     u32RxErrCount;        /*���մ���*/
    unsigned int     u32TxOverFlowCount;   /*�����������*/
    unsigned int     u32RxOverFlowCount;   /*�����������*/
    unsigned int     u32CurrentTx;         /*��������*/
    unsigned int     u32CurrentRx;         /*��������*/
} NCM_IOCTL_PKT_STATISTICS_S;


typedef struct tagNCM_PKT_INFO_S
{
    unsigned int u32PsRcvPktNum;              /* �հ��͵�PS�İ�����*/
    unsigned int u32RcvUnusedNum;             /* �հ�������PSҪ����������*/
    unsigned int u32NcmSendPktNum;            /* ��������*/
}NCM_PKT_INFO_S;

/*�����ص�ָ�붨��*/
typedef  void  (*FUNC_USB_LP_NOTIFY)(int iStatus);
/*****************************************************************************
 �� �� ��  : DRV_USB_LP_CB_REGISTER
 ��������  : L2״̬�����˳�֪ͨ�ص�����ע��ӿ�
 �������  : FUNC_USB_LP_NOTIFY *pUSBLPFunc�ص�����ָ��
 �������  : ��
 �� �� ֵ  : 0:    ע��ɹ�
           ������ע��ʧ��
*****************************************************************************/
int l2_notify_register(FUNC_USB_LP_NOTIFY pUSBLPFunc);
#define DRV_USB_LP_CB_REGISTER(pUSBLPFunc)  \
                l2_notify_register(pUSBLPFunc)

/*TCP/IPЭ��ջ��ά�ɲⲶ�����Ϣ��ʶ*/
enum IPS_MNTN_TRACE_IP_MSG_TYPE_ENUM
{
    /* IP ���ݰ���ά�ɲ��ϱ� */
    ID_IPS_TRACE_IP_ADS_UL                  = 0xD030,
    ID_IPS_TRACE_IP_ADS_DL                  = 0xD031,
    ID_IPS_TRACE_IP_USB_UL                  = 0xD032,
    ID_IPS_TRACE_IP_USB_DL                  = 0xD033,

    ID_IPS_TRACE_IP_MSG_TYPE_BUTT
};

/*******************************************************************************
  Function:     BSP_Modem_OS_Status_Swtich
  Description:  ָʾ Modem Ready ���� ��Ready״̬

  Input:        int enable
                ��0:����Modem����Ready״̬
                0:����Modem���ڷ�Ready״̬

  Output:
  Return:       0:    �����ɹ�
                -1:   ����ʧ��
  Others:       added by zkf52403
*******************************************************************************/
extern int BSP_Modem_OS_Status_Switch(int enable);
#define DRV_OS_STATUS_SWITCH(enable) BSP_Modem_OS_Status_Switch(enable)

struct sk_buff;
typedef void (*USB_IPS_MNTN_TRACE_CB_T)(struct sk_buff *skb,unsigned short usType);
extern unsigned int BSP_USB_RegIpsTraceCB(USB_IPS_MNTN_TRACE_CB_T pFunc);
#define DRV_USB_REG_IPS_TRACECB(x) BSP_USB_RegIpsTraceCB(x)

/*****************************************************************************
 �� �� ��  : USB_ETH_DrvSetRxFlowCtrl
 ��������  : ���ӿ���������
 �������  : unsigned int ulParam1  :����Ϊ��չ��
             unsigned int ulParam2  :����Ϊ��չ��
 �������  : ��
 �� �� ֵ  : unsigned int :��׮ʵ�֣�Ĭ�Ϸ���0
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2012��9��15��
    ��    ��   : ���� 00195127
    �޸�����   : �����ɺ���

*****************************************************************************/
unsigned int USB_ETH_DrvSetRxFlowCtrl(unsigned int ulParam1, unsigned int ulParam2);

#endif
#if 1

#define UDI_CAPA_BLOCK_READ    ((unsigned int)(1<<0)) /* ���豸��read�ӿ�Ϊ�����ӿ� */
#define UDI_CAPA_BLOCK_WRITE   ((unsigned int)(1<<1)) /* ���豸��write�ӿ�Ϊ�����ӿ� */
#define UDI_CAPA_READ_CB       ((unsigned int)(1<<2)) /* ���豸֧��read �ص����� */
#define UDI_CAPA_WRITE_CB      ((unsigned int)(1<<3)) /* ���豸֧��write �ص����� */
#define UDI_CAPA_BUFFER_LIST   ((unsigned int)(1<<4)) /* ���豸�Ķ�дbufferΪ�ڴ������ṹ(Ĭ��Ϊ��ͨ�ڴ��) */
#define UDI_CAPA_CTRL_OPT      ((unsigned int)(1<<5)) /* ���豸֧�ֿ��Ʋ��� */

#define MIN_UDI_HANDLE     0x5a0000
#define MAX_UDI_HANDLE     0x5a003f
#endif

#if 1  /*drv_uart_if.h*/
typedef enum _E_UART_CONSUMER_ID_
{
    CBP_UART_PORT_ID = 1,
    UART_CONSUMER_BUTT
}UART_CONSUMER_ID;

/*****************************************************************************
 �� �� ��  : DRV_UART_SEND
 ��������  : UART�ڷ�������
             ������ֻ֧������������ͬ�����������ݷ���
 �������  : uPortNo
             pDataBuffer
             uslength
 �������  : ��
 �� �� ֵ  :  0  ���ͳɹ�;  1  ����ʧ��;
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2013��12��19��
    ��    ��   :
    �޸�����   : �����ɺ���

*****************************************************************************/
extern int uart_core_send(UART_CONSUMER_ID uPortNo, unsigned char * pDataBuffer, unsigned int ulLength);
#define DRV_UART_SEND(uPortNo,pDataBuffer,ulLength) \
        uart_core_send(uPortNo,pDataBuffer,ulLength)
/*****************************************************************************
 �� �� ��  : DRV_UART_RCV_CALLBACK_REGI
 ��������  : UART�����ݽ����ϲ�Ӧ�ûص�����ע��ӿ�
 �������  : uPortNo
             pCallback

 �������  : ��
 �� �� ֵ  :  0 :ע��ɹ�;  -1 ��ע��ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2013��12��19��
    ��    ��   :
    �޸�����   : �����ɺ���

*****************************************************************************/
typedef int (*pUARTRecv)(UART_CONSUMER_ID uPortNo,unsigned char * pData, unsigned int ulLength);
extern int uart_core_recv_handler_register(UART_CONSUMER_ID uPortNo, pUARTRecv pCallback);
#define DRV_UART_RCV_CALLBACK_REGI(uPortNo,pCallback)\
        uart_core_recv_handler_register(uPortNo,pCallback)
#endif

/*audio*/
static inline int DRV_CODEC_OPEN(const char *devname, int flags, int mode)
{
    return 0;
}

static inline void BSP_OM_SoftReboot(void)
{
    return ;
}

static inline int DRV_SDMMC_GET_STATUS(void)
{
    return 0;
}

/*om use begin*/
/*VERSIONINFO_I���ݽṹ�а汾�ַ��������Ч�ַ�����*/
#define VER_MAX_LENGTH                  30

typedef struct
{
    unsigned char CompId;              /* ����ţ��μ�COMP_TYPE */
    unsigned char CompVer[VER_MAX_LENGTH+1];         /* ���汾���� 30 �ַ�+ \0 */
}VERSIONINFO_I;
typedef enum{
     ADDRTYPE8BIT,
     ADDRTYPE16BIT,
     ADDRTYPE32BIT
}ENADDRTYPE;

extern int BSP_OM_RegRead(unsigned int* u32RegAddr,
    ENADDRTYPE enAddrType, unsigned int *pu32Value);

extern int BSP_OM_RegWrite(unsigned int* u32RegAddr,
    ENADDRTYPE enAddrType, unsigned int u32Value);
typedef struct tagBSP_OM_NET_S
{
    unsigned int u32NetRxStatOverFlow;       /* ����FIFO���ͳ�Ƽ��� */
    unsigned int u32NetRxStatPktErr;         /* �����ܴ������� */
    unsigned int u32NetRxStatCrcErr;         /* ����CRC�������� */
    unsigned int u32NetRxStatLenErr;         /* ������Ч���Ȱ����� */
    unsigned int u32NetRxNoBufInt;           /* ����û��BUFFER�жϼ��� */
    unsigned int u32NetRxStopInt;            /* ����ֹͣ�жϼ��� */
    unsigned int u32NetRxDescErr;            /* �������������� */

    unsigned int u32NetTxStatUnderFlow;      /* ����FIFO����ͳ�Ƽ��� */
    unsigned int u32NetTxUnderFlowInt;       /* ����FIFO�����жϼ��� */
    unsigned int u32NetTxStopInt;            /* ����ֹͣ�жϼ��� */
    unsigned int u32NetTxDescErrPs;          /* ��������������(Ps) */
    unsigned int u32NetTxDescErrOs;          /* ��������������(Os) */
    unsigned int u32NetTxDescErrMsp;         /* ��������������(Msp) */

    unsigned int u32NetFatalBusErrInt;      /* ���ߴ���*/
}BSP_OM_NET_S;

/*om use end*/

/*taf used*/
enum SECURE_ENABLE_STATUS_I
{
    SECURE_DISABLE = 0,
    SECURE_ENABLE = 1
};

static inline int DRV_FILE_GET_DISKSPACE(const char *path,unsigned int *DskSpc,unsigned int *UsdSpc,  unsigned int *VldSpc)
{
    return 0;
}

#if 1  /*drv_sec.h*/
/*************************************************
 �� �� ��       : DRV_SECURE_SUPPORT
 ��������   : ��ǰ�汾�Ƿ�֧�ְ�ȫ����
 �������   : unsigned char *pData
 �������   : unsigned char *pData
 �� �� ֵ      : OK/ERROR
*************************************************/
extern int DRV_SECURE_SUPPORT(unsigned char *pu8Data);

/*************************************************
 �� �� ��       : DRV_SECURE_ALREADY_USE
 ��������   : ��ѯ��ǰ�汾�Ƿ��Ѿ����ð�ȫ����
 �������   : unsigned char *pData
 �������   : unsigned char *pData
 �� �� ֵ      : OK/ERROR
*************************************************/
extern int DRV_SECURE_ALREADY_USE(unsigned char *pu8Data);

/*************************************************
 �� �� ��       : DRV_START_SECURE
 ��������   : ���ð�ȫ����
 �������   :
 �������   :
 �� �� ֵ      : OK/ERROR
*************************************************/
extern int DRV_START_SECURE(void);

/*****************************************************************************
* �� �� ��  : mdrv_crypto_check_sec
*
* ��������  : ��ȫ�����ж�
*
* �������  : void
* �������  :
*
* �� �� ֵ  : 1����ȫ����0���ǰ�ȫ����
*
* ����˵��  :
*
*****************************************************************************/
unsigned int mdrv_crypto_check_sec (void);
#endif

#if 1  /*drv_pmu.h*/
/*****************************************************************************
 �� �� ��  : DRV_GET_PMU_STATE
 ��������  : ��ȡPMUģ�鿪����ʽ�����״̬����ص����������λ״̬��
 �������  : �ޡ�
 �������  : Pmu_State :������ʽ�����״̬����ص����������λ״̬��
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ע��������ýӿڽ���PS��PC������ʹ�ã�Ŀǰû��Ӧ�ã��ݱ�����
*****************************************************************************/
extern unsigned int DRV_GET_PMU_STATE(void*  Pmu_State);

/*****************************************************************************
��������   BSP_USB_PortTypeValidCheck
��������:  �ṩ���ϲ��ѯ�豸�˿���̬���úϷ��Խӿ�
           1���˿�Ϊ��֧�����ͣ�2������PCUI�ڣ�3�����ظ��˿ڣ�4���˵���������16��
           5����һ���豸��ΪMASS��
��������� pucPortType  �˿���̬����
           ulPortNum    �˿���̬����
����ֵ��   0:    �˿���̬�Ϸ�
           �������˿���̬�Ƿ�
*****************************************************************************/
unsigned int BSP_USB_PortTypeValidCheck(unsigned char *pucPortType, unsigned int ulPortNum);
#define DRV_USB_PORT_TYPE_VALID_CHECK(pucPortType, ulPortNum)  \
                    BSP_USB_PortTypeValidCheck(pucPortType, ulPortNum)

/*****************************************************************************
��������   BSP_USB_GetAvailabePortType
��������:  �ṩ���ϲ��ѯ��ǰ�豸֧�ֶ˿���̬�б��ӿ�
��������� ulPortMax    Э��ջ֧�����˿���̬����
�������:  pucPortType  ֧�ֵĶ˿���̬�б�
           pulPortNum   ֧�ֵĶ˿���̬����
����ֵ��   0:    ��ȡ�˿���̬�б��ɹ�
           ��������ȡ�˿���̬�б�ʧ��
*****************************************************************************/
unsigned int BSP_USB_GetAvailabePortType(unsigned char *pucPortType,
                            unsigned int *pulPortNum, unsigned int ulPortMax);
#define DRV_USB_GET_AVAILABLE_PORT_TYPE(pucPortType, pulPortNum, ulPortMax)  \
                BSP_USB_GetAvailabePortType(pucPortType, pulPortNum, ulPortMax)
#endif

#if 1  /*drv_oled.h*/

/*****************************************************************************
*  Function:  DRV_OLED_CLEAR_WHOLE_SCREEN
*  Description: oled clear *
*  Called By:AP
*  Table Accessed:
*  Table Updated:
*  Input:
*         N/A
*  Output:
*         N/A
*  Return:
*         N/A
*****************************************************************************/
extern void DRV_OLED_CLEAR_WHOLE_SCREEN(void);

/*****************************************************************************
*  Function:  DRV_OLED_UPDATE_STATE_DISPLAY
*  Description: oled display right or not right  *
*  Called By:AP
*  Table Accessed:
*  Table Updated:
*  Input:
*         N/A
*  Output:
*         N/A
*  Return:
*         N/A
*****************************************************************************/
extern  void DRV_OLED_UPDATE_STATE_DISPLAY(int UpdateStatus);

/*****************************************************************************
 �� �� ��  : DRV_OLED_UPDATE_DISPLAY
 ��������  : SD����Ƭ�汾������OLED��ʾ
 �������  : BOOL UpdateStatus
 �������  : �ޡ�
 ����ֵ��   ��
 ����:  �����ɹ���NV������ɺ���øú�����
        ����ʧ�ܺ���øú������
*****************************************************************************/
extern void DRV_OLED_UPDATE_DISPLAY(int UpdateStatus);

/*****************************************************************************
 �� �� ��  : DRV_OLED_STRING_DISPLAY
 ��������  : oled�ַ�����ʾ����
 �������  :
 *           Ҫ��ʾ�ַ�������ʼ����
 *           Ҫ��ʾ���ַ���
 �������  : none
 �� �� ֵ  : void
*****************************************************************************/
extern void DRV_OLED_STRING_DISPLAY(unsigned char ucX, unsigned char ucY, unsigned char *pucStr);

#endif

#if 1  /*drv_nve.h*/
#define BSP_NVE_NAME_LENGTH          8       /*NV name maximum length*/
#define BSP_NVE_NV_DATA_SIZE        104     /*NV data maximum length*/

#define BSP_NV_READ                 1       /*NV read  operation*/
#define BSP_NV_WRITE                0       /*NV write operation*/

typedef struct nve_info_stru {
	unsigned int nv_operation;              /*0-write,1-read*/
	unsigned int nv_number;                 /*NV number you want to visit*/
	char nv_name[BSP_NVE_NAME_LENGTH];
	unsigned int valid_size;
	unsigned char nv_data[BSP_NVE_NV_DATA_SIZE];
}NVE_INFO_S;

/*****************************************************************************
* �� �� ��  : DRV_NVE_ACCESS
*
* ��������  : ����LINUX NVE
*
* �������  : nve  ID
*
* �������  : ��
*
* �� �� ֵ  : OK& ERROR
*
* �޸ļ�¼ :  2013��6��27��  v1.00  yuanqinshun  ����
*****************************************************************************/
int DRV_NVE_ACCESS(NVE_INFO_S *nve);
#endif

#if 1  /*drv_nv.h*/
/*****************************************************************************
 �� �� ��  : DRV_COPY_NVUSE_TO_NVBACKUP
 ��������  : ��NV��ʹ����������������
 �������  : ��
 �������  : ��
 �� �� ֵ  : int��0�ɹ�������ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2012��7��21��
    ��    ��   : ��� y00186965
    �޸�����   : �����ɺ���

*****************************************************************************/
extern int DRV_COPY_NVUSE_TO_NVBACKUP ( void );


/*****************************************************************************
 �� �� ��  : DRV_COPY_NVUPGRADE_TO_NVUSE
 ��������  : ��NV��������������ʹ����
 �������  : ��
 �������  : ��
 �� �� ֵ  : int��0�ɹ�������ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2012��7��21��
    ��    ��   : ��� y00186965
    �޸�����   : �����ɺ���

*****************************************************************************/
extern int DRV_COPY_NVUPGRADE_TO_NVUSE( void );
#endif

typedef enum tagMSP_PROC_ID_E
{
    OM_REQUEST_PROC = 0,
    OM_ADDSNTIME_PROC = 1,
    OM_PRINTF_WITH_MODULE = 2,
    OM_PRINTF = 3,

    OM_PRINTF_GET_MODULE_IDLEV = 4,
/*BEGIN add by g00175336 for mmi 2012-01-18*/
    OM_READ_NV_PROC = 5,
    OM_WRITE_NV_PROC = 6,
/*END add by g00175336 for mmi 2012-01-18*/
    OM_MNTN_ERRLOG = 7, /*Added by c00204787 for errorlog,20120211*/

    MSP_PROC_REG_ID_MAX
}MSP_PROC_ID_E;

typedef void (*BSP_MspProc)(void);
#define EXCH_CB_NAME_SIZE           (32)
typedef struct
{
    char   aucName[EXCH_CB_NAME_SIZE];
    unsigned char *pucData;
    unsigned int  ulDataLen;
}cb_buf_t;

typedef int  (*exchCBReg)(cb_buf_t *);

/*liuyi++ �ýӿ����ĵ���δ���� ++*/
/*******************************************************************************
 �� �� ��: DRV_EXCH_CUST_FUNC_REG
 ��������: ��������ģ���ṩ����ӿڣ����ϲ�ע��ص������ڸ�λʱ�����ϲ�ָ����Ϣ
 �������: cb �ϲ�ָ���ص��ӿ�
 �������: ��
 �� �� ֵ: 0-���ӳɹ�������-����ʧ��
 ˵��    :  pucData :ָ����¼��Ϣ��ַ����ע�ⲻҪָ��ջ����ʱ���������ѿռ����ݣ�
                     ʹ�����Լ�ά���������ռ�ʹ���꣬�Լ��ͷš�����ȫ�ֱ�����
            aucName :����
            ulDataLen: ���ݳ���

����ֵ�б�:
    CUST_REG_OK = 0,                        ���ӳɹ�
    CUST_REG_PARM_NULL,                     ���Ϊ��
    CUST_REG_EXCH_NOT_READY,                EXCHģ����δ��ʼ��
    CUST_REG_EXCH_REBOOT_CONTEXT,           �Ѿ����븴λ����
    CUST_REG_EXCH_QUEUE_NOT_INIT,           ע�������δ��ʼ��
    CUST_REG_EXCH_QUEUE_FULL,               ��������
    CUST_REG_EXCH_ALREADY_IN_QUEUE,         �ظ�ע��
    CUST_REG_EXCH_MUTEX_REJECT,             MUTEX REJECT
    CUST_REG_EXCH_QUEUE_IN_ERR              �����ʧ��

��ͨ������ֵ��λ�������

*******************************************************************************/
extern int DRV_EXCH_CUST_FUNC_REG(exchCBReg cb);
/*liuyi++ �ýӿ����ĵ���δ���� --*/

/*liuyi++ �ýӿ����ĵ���δ����*/
/*****************************************************************************
* �� �� ��  : DRV_MSP_PROC_REG
*
* ��������  : DRV�ṩ��OM��ע�ắ��
*
* �������  : MSP_PROC_ID_E eFuncID, BSP_MspProc pFunc
* �������  : NA
*
* �� �� ֵ  : NA
*
* ����˵��  : ��ά�ɲ�ӿں���
*
*****************************************************************************/
extern void DRV_MSP_PROC_REG(MSP_PROC_ID_E eFuncID, BSP_MspProc pFunc);
/*liuyi++ �ýӿ����ĵ���δ����--*/
#else
/*****************************************************************************
 * �� �� ��  : BSP_MALLOC
 *
 * ��������  : BSP ��̬�ڴ����
 *
 * �������  : sz: ����Ĵ�С(byte)
 *             flags: �ڴ�����(�ݲ�ʹ��,Ԥ��)
 * �������  : ��
 * �� �� ֵ  : ����������ڴ�ָ��
 *****************************************************************************/
static inline void* BSP_MALLOC(unsigned int u32Size, MEM_POOL_TYPE enFlags) {
    return 0;
}

/*****************************************************************************
 * �� �� ��  : BSP_FREE
 *
 * ��������  : BSP ��̬�ڴ��ͷ�
 *
 * �������  : ptr: ��̬�ڴ�ָ��
 * �������  : ��
 * �� �� ֵ  : ��
 *****************************************************************************/
static inline void  BSP_FREE(void* pMem) {
}

/*****************************************************************
Function: free_mem_size_get
Description:
    get mem size
Input:
    N/A
Output:
    N/A
Return:
    free mem size
*******************************************************************/
static inline unsigned int FREE_MEM_SIZE_GET(void)
{
    return 0;
}

static inline int BSP_DLOAD_GetDloadType(void)
{
    return 1;
}

/*liuyi++ only for nas marenkun temp*/

/*****************************************************************************
 Function   : BSP_PWC_SetTimer4WakeSrc
 Description: ����timer4��Ϊ����Դ
 Input      :
 Return     : void
 Other      :
*****************************************************************************/
static inline void BSP_PWC_SetTimer4WakeSrc(void)
{
    return;
}


/*****************************************************************************
 Function   : BSP_PWC_DelTimer4WakeSrc
 Description: ����timer4����Ϊ����Դ
 Input      :
            :
 Return     : void
 Other      :
*****************************************************************************/
static inline void BSP_PWC_DelTimer4WakeSrc(void)
{
    return;
}

/*****************************************************************************
 �� �� ��  : DRV_PWRCTRL_SLEEPVOTE_LOCK
 ��������  : �����ֹ˯��ͶƱ�ӿڡ�
 �������  : enClientId:PWC_CLIENT_ID_E
 �������  : None
 �� �� ֵ  : PWC_PARA_INVALID/PWC_SUCCESS
*****************************************************************************/
static inline unsigned int  DRV_PWRCTRL_SLEEPVOTE_LOCK(PWC_CLIENT_ID_E enClientId)
{
    return 0;
}

/*****************************************************************************
 �� �� ��  : DRV_PWRCTRL_SLEEPVOTE_UNLOCK
 ��������  : ��������˯��ͶƱ�ӿڡ�
 �������  : enClientId:PWC_CLIENT_ID_E
 �������  : None
 �� �� ֵ  : PWC_PARA_INVALID/PWC_SUCCESS
*****************************************************************************/
static inline unsigned int  DRV_PWRCTRL_SLEEPVOTE_UNLOCK(PWC_CLIENT_ID_E enClientId)
{
    return 0;
}

/*****************************************************************************
 �� �� ��  : BSP_PWRCTRL_StandbyStateCcpu/BSP_PWRCTRL_StandbyStateAcpu
 ��������  : AT^PSTANDBY
 �������  :
 �������  :
 ����ֵ��
*****************************************************************************/
static inline unsigned int DRV_PWRCTRL_STANDBYSTATEACPU(unsigned int ulStandbyTime, unsigned int ulSwitchtime)
{
    return 0;
}

/*dload begin*/

#if 1
/* Begin: DRVΪOAMʵ��NV���ݻָ����̶��ṩ�Ľӿ� */

/********************************************************************************************************
 �� �� ��  : DRV_UPGRADE_NV_SET_RESULT
 ��������  : OAMͨ���ýӿڸ��ߵ���NV�����ɹ��˻���ʧ����
 �������  : status��0      ��ʾNV�����ɹ�
                     ����ֵ ��ʾNV����ʧ��
 �������  :
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ����ע��  : V3R3 SFT �� V9 BBIT ��׮
             V9 SFT  ʵ��
                ���NV�����ɹ���������fastboot�������óɹ���ʶ
                ���NV����ʧ�ܣ�������fastboot��������ʧ�ܱ�ʶ

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �����ɺ���
********************************************************************************************************/
static inline int DRV_UPGRADE_NV_SET_RESULT(int status)
{
    return 0;
}

/********************************************************************************************************
 �� �� ��  : DRV_NV_FILE_SEC_CHECK
 ��������  : ���ļ�����У��
 �������  : pnvFilePath����У���ļ���·��
 �������  :
 �� �� ֵ  : 0 :    �ļ�У����ȷ
             -1��   �ļ�У��ʧ��
 ����ע��  : Ŀǰ�������߱���ֱ�Ӵ�׮���Ժ�ʵ��

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �����ɺ���
********************************************************************************************************/
static inline int DRV_NV_FILE_SEC_CHECK(char * pnvFilePath)
{
    return 0;
}

/********************************************************************************************************
 �� �� ��  : BSP_DLOAD_NVBackupRead
 ��������  : ��Flash�е�NV�������ȡ���ݣ�ʵ��NV��Ļָ����ܡ�
 �������  : len����NV�������ʼ����ʼ����Ҫ��ȡ��NV��ȣ��ֽ�������
 �������  : pRamAddr��Ŀ��RAM��ַ�����ڴ�Ŷ�����NV�����ݡ�
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ����ע��  : V3R3 SFT �� V9 BBIT ����ԭ��
             V9 SFT  ������nand�ӿ��޸�Ϊ����emmc�ӿڶ�дNV������

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �޸ĺ���
********************************************************************************************************/
static inline int DRV_NVBACKUP_READ(unsigned char *pRamAddr, unsigned int len)
{
    return 0;
}

/********************************************************************************************************
 �� �� ��  : BSP_DLOAD_NVBackupWrite
 ��������  : ���ض�����д��Flash�е�NV�������ʵ��NV��ı��ݹ��ܡ�
 �������  : pRamAddr��ԴRAM��ַ�����ڴ����Ҫд���NV�����ݡ�
             len����NV�������ʼ����ʼ����Ҫд���NV��ȣ��ֽ�������
 �������  : �ޡ�
 �� �� ֵ  : 0:  �����ɹ���
             -1������ʧ�ܡ�
 ����ע��  : V3R3 SFT �� V9 BBIT ����ԭ��
             V9 SFT  ������nand�ӿ��޸�Ϊ����emmc�ӿڶ�дNV������

 �޸���ʷ  :
 1.��    ��   : 2012��12��6��
   ��    ��   : ��ϲ 00220237
   �޸�����   : �޸ĺ���
********************************************************************************************************/

static inline int DRV_NVBACKUP_WRITE(unsigned char *pRamAddr, unsigned int len)
{
    return 0;
}





#endif


#define UART_IOCTL_GET_RD_BUFF       0x7F001011
#define UART_IOCTL_SET_READ_CB       0x7F001001

typedef enum
{
    WWAN_WCDMA    = 1,/*WCDMAģʽ*/
    WWAN_CDMA    = 2  /*CDMAģʽ*/
}NDIS_WWAN_MODE;

/*�ܽ��źŵĶ���*/
#define IO_CTRL_FC                      (0x02)
#define IO_CTRL_DSR                     (0x20)
#define IO_CTRL_DTR                     (0x01)
#define IO_CTRL_RFR                     (0x04)
#define IO_CTRL_CTS                     (0x10)
#define IO_CTRL_RI                      (0x40)
#define IO_CTRL_DCD                     (0x80)

#define DRV_GPIO_USB_SWITCH     200
#define DRV_GPIO_HIGH           1
#define DRV_GPIO_LOW            0

#define AT_USB_SWITCH_SET_VBUS_INVALID          2

/* �豸ö�����˿ڸ��� */
#define DYNAMIC_PID_MAX_PORT_NUM        17

/* NV��50091�ṹ�������豸��Ҫö�ٵĶ˿���̬ */
typedef struct
{
    unsigned int ulStatus;
    unsigned char aucFirstPortStyle[DYNAMIC_PID_MAX_PORT_NUM];  /* �豸�л�ǰ�˿���̬ */
    unsigned char aucRewindPortStyle[DYNAMIC_PID_MAX_PORT_NUM]; /* �豸�л���˿���̬ */
    unsigned char reserved[22];
}DRV_DYNAMIC_PID_TYPE_STRU;

typedef void(* USB_NET_DEV_SWITCH_GATEWAY)(void);

static inline int DRV_OS_STATUS_SWITCH(int enable)
{
    return 0;
}

static inline int DRV_GET_LINUXSYSTYPE(void)
{
    return -1;
}

static inline DRV_SET_PORT_QUIRY(DRV_DYNAMIC_PID_TYPE_STRU *pstDynamicPidType)
{
    return 0;
}

static inline int DRV_USB_NAS_SWITCH_GATEWAY_REGFUNC(USB_NET_DEV_SWITCH_GATEWAY switchGwMode)
{
    return 0;
}

static inline unsigned int DRV_GET_U2DIAG_DEFVALUE(void)
{
    return 0;
}

static inline int DRV_GET_DIAG_MODE_VALUE(unsigned char *pucDialmode,
                                          unsigned char *pucCdcSpec)
{
    return 1;
}

static inline int DRV_USB_PHY_SWITCH_SET(unsigned char ucValue)
{
    return -1;
}

static inline int DRV_USB_PORT_TYPE_VALID_CHECK(unsigned char *pucPortType,
                                                unsigned int ulPortNum)
{
    return 0;
}

static inline int DRV_UDIAG_VALUE_CHECK(unsigned int DiagValue)
{
    return 1;
}

static inline int DRV_U2DIAG_VALUE_CHECK(unsigned int DiagValue)
{
    return 1;
}

static inline unsigned char DRV_GET_PORT_MODE(char*PsBuffer, unsigned int*Length)
{
    return 1;
}

static inline int DRV_SET_PID(unsigned char u2diagValue)
{
    return 1;
}

static inline int DRV_GPIO_SET(unsigned char ucGroup, unsigned char ucPin, unsigned char ucValue)
{
    return -1;
}

static inline void DRV_AT_SETAPPDAILMODE(unsigned int ulStatus)
{
    return;
}
/*************************************************
 �� �� ��   : BSP_MNTN_ProductTypeGet
 ��������   : ���ص�ǰ��Ʒ����
 �������   : ��
 �������   : ��
 �� �� ֵ   :0:STICK
             1:MOD
             2:E5
             3:CPE
*************************************************/
static inline unsigned int BSP_MNTN_ProductTypeGet(void)
{
	return 0;
}
#define DRV_PRODUCT_TYPE_GET()   BSP_MNTN_ProductTypeGet()

static inline int DRV_SDMMC_GET_STATUS(void)
{
    return 0;
}

/*om use begin*/
/*VERSIONINFO_I���ݽṹ�а汾�ַ��������Ч�ַ�����*/
#define VER_MAX_LENGTH                  30

typedef struct
{
    unsigned char CompId;              /* ����ţ��μ�COMP_TYPE */
    unsigned char CompVer[VER_MAX_LENGTH+1];         /* ���汾���� 30 �ַ�+ \0 */
}VERSIONINFO_I;

typedef enum{
    ADDRTYPE8BIT,
    ADDRTYPE16BIT,
    ADDRTYPE32BIT
}ENADDRTYPE;

typedef struct tagBSP_OM_NET_S
{
    unsigned int u32NetRxStatOverFlow;       /* ����FIFO���ͳ�Ƽ��� */
    unsigned int u32NetRxStatPktErr;         /* �����ܴ������� */
    unsigned int u32NetRxStatCrcErr;         /* ����CRC�������� */
    unsigned int u32NetRxStatLenErr;         /* ������Ч���Ȱ����� */
    unsigned int u32NetRxNoBufInt;           /* ����û��BUFFER�жϼ��� */
    unsigned int u32NetRxStopInt;            /* ����ֹͣ�жϼ��� */
    unsigned int u32NetRxDescErr;            /* �������������� */

    unsigned int u32NetTxStatUnderFlow;      /* ����FIFO����ͳ�Ƽ��� */
    unsigned int u32NetTxUnderFlowInt;       /* ����FIFO�����жϼ��� */
    unsigned int u32NetTxStopInt;            /* ����ֹͣ�жϼ��� */
    unsigned int u32NetTxDescErrPs;          /* ��������������(Ps) */
    unsigned int u32NetTxDescErrOs;          /* ��������������(Os) */
    unsigned int u32NetTxDescErrMsp;         /* ��������������(Msp) */

    unsigned int u32NetFatalBusErrInt;      /* ���ߴ���*/
}BSP_OM_NET_S;

/*om use end*/

/*taf used*/
enum SECURE_ENABLE_STATUS_I
{
    SECURE_DISABLE = 0,
    SECURE_ENABLE = 1
};

static inline int DRV_FILE_GET_DISKSPACE(const char *path,unsigned int *DskSpc,unsigned int *UsdSpc,  unsigned int *VldSpc)
{
    return 0;
}
/*liuyi--*/

#if 1  /*drv_timer.h*/
extern unsigned int omTimerGet(void);
#define DRV_GET_SLICE() omTimerGet()
#endif

#if 1  /*drv_oled.h*/

/*****************************************************************************
*  Function:  DRV_OLED_CLEAR_WHOLE_SCREEN
*  Description: oled clear *
*  Called By:AP
*  Table Accessed:
*  Table Updated:
*  Input:
*         N/A
*  Output:
*         N/A
*  Return:
*         N/A
*****************************************************************************/
extern void DRV_OLED_CLEAR_WHOLE_SCREEN(void);

/*****************************************************************************
*  Function:  DRV_OLED_UPDATE_STATE_DISPLAY
*  Description: oled display right or not right  *
*  Called By:AP
*  Table Accessed:
*  Table Updated:
*  Input:
*         N/A
*  Output:
*         N/A
*  Return:
*         N/A
*****************************************************************************/
extern  void DRV_OLED_UPDATE_STATE_DISPLAY(int UpdateStatus);

/*****************************************************************************
 �� �� ��  : DRV_OLED_UPDATE_DISPLAY
 ��������  : SD����Ƭ�汾������OLED��ʾ
 �������  : BOOL UpdateStatus
 �������  : �ޡ�
 ����ֵ��   ��
 ����:  �����ɹ���NV������ɺ���øú�����
        ����ʧ�ܺ���øú������
*****************************************************************************/
extern void DRV_OLED_UPDATE_DISPLAY(int UpdateStatus);

/*****************************************************************************
 �� �� ��  : DRV_OLED_STRING_DISPLAY
 ��������  : oled�ַ�����ʾ����
 �������  :
 *           Ҫ��ʾ�ַ�������ʼ����
 *           Ҫ��ʾ���ַ���
 �������  : none
 �� �� ֵ  : void
*****************************************************************************/
extern void DRV_OLED_STRING_DISPLAY(unsigned char ucX, unsigned char ucY, unsigned char *pucStr);

#endif

#if 1  /*drv_nve.h*/
#define BSP_NVE_NAME_LENGTH          8       /*NV name maximum length*/
#define BSP_NVE_NV_DATA_SIZE        104     /*NV data maximum length*/

#define BSP_NV_READ                 1       /*NV read  operation*/
#define BSP_NV_WRITE                0       /*NV write operation*/

typedef struct nve_info_stru {
	unsigned int nv_operation;              /*0-write,1-read*/
	unsigned int nv_number;                 /*NV number you want to visit*/
	char nv_name[BSP_NVE_NAME_LENGTH];
	unsigned int valid_size;
	unsigned char nv_data[BSP_NVE_NV_DATA_SIZE];
}NVE_INFO_S;

/*****************************************************************************
* �� �� ��  : DRV_NVE_ACCESS
*
* ��������  : ����LINUX NVE
*
* �������  : nve  ID
*
* �������  : ��
*
* �� �� ֵ  : OK& ERROR
*
* �޸ļ�¼ :  2013��6��27��  v1.00  yuanqinshun  ����
*****************************************************************************/
int DRV_NVE_ACCESS(NVE_INFO_S *nve);
#endif

#if 1  /*drv_nv.h*/
/*****************************************************************************
 �� �� ��  : DRV_COPY_NVUSE_TO_NVBACKUP
 ��������  : ��NV��ʹ����������������
 �������  : ��
 �������  : ��
 �� �� ֵ  : int��0�ɹ�������ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2012��7��21��
    ��    ��   : ��� y00186965
    �޸�����   : �����ɺ���

*****************************************************************************/
extern int DRV_COPY_NVUSE_TO_NVBACKUP ( void );


/*****************************************************************************
 �� �� ��  : DRV_COPY_NVUPGRADE_TO_NVUSE
 ��������  : ��NV��������������ʹ����
 �������  : ��
 �������  : ��
 �� �� ֵ  : int��0�ɹ�������ʧ��
 ���ú���  :
 ��������  :

 �޸���ʷ      :
  1.��    ��   : 2012��7��21��
    ��    ��   : ��� y00186965
    �޸�����   : �����ɺ���

*****************************************************************************/
extern int DRV_COPY_NVUPGRADE_TO_NVUSE( void );
#endif

typedef enum tagMSP_PROC_ID_E
{
    OM_REQUEST_PROC = 0,
    OM_ADDSNTIME_PROC = 1,
    OM_PRINTF_WITH_MODULE = 2,
    OM_PRINTF = 3,

    OM_PRINTF_GET_MODULE_IDLEV = 4,
/*BEGIN add by g00175336 for mmi 2012-01-18*/
    OM_READ_NV_PROC = 5,
    OM_WRITE_NV_PROC = 6,
/*END add by g00175336 for mmi 2012-01-18*/
    OM_MNTN_ERRLOG = 7, /*Added by c00204787 for errorlog,20120211*/

    MSP_PROC_REG_ID_MAX
}MSP_PROC_ID_E;

typedef void (*BSP_MspProc)(void);
#define EXCH_CB_NAME_SIZE           (32)
typedef struct
{
    char   aucName[EXCH_CB_NAME_SIZE];
    unsigned char *pucData;
    unsigned int  ulDataLen;
}cb_buf_t;

typedef int  (*exchCBReg)(cb_buf_t *);

/*liuyi++ �ýӿ����ĵ���δ���� ++*/
/*******************************************************************************
 �� �� ��: DRV_EXCH_CUST_FUNC_REG
 ��������: ��������ģ���ṩ����ӿڣ����ϲ�ע��ص������ڸ�λʱ�����ϲ�ָ����Ϣ
 �������: cb �ϲ�ָ���ص��ӿ�
 �������: ��
 �� �� ֵ: 0-���ӳɹ�������-����ʧ��
 ˵��    :  pucData :ָ����¼��Ϣ��ַ����ע�ⲻҪָ��ջ����ʱ���������ѿռ����ݣ�
                     ʹ�����Լ�ά���������ռ�ʹ���꣬�Լ��ͷš�����ȫ�ֱ�����
            aucName :����
            ulDataLen: ���ݳ���

����ֵ�б�:
    CUST_REG_OK = 0,                        ���ӳɹ�
    CUST_REG_PARM_NULL,                     ���Ϊ��
    CUST_REG_EXCH_NOT_READY,                EXCHģ����δ��ʼ��
    CUST_REG_EXCH_REBOOT_CONTEXT,           �Ѿ����븴λ����
    CUST_REG_EXCH_QUEUE_NOT_INIT,           ע�������δ��ʼ��
    CUST_REG_EXCH_QUEUE_FULL,               ��������
    CUST_REG_EXCH_ALREADY_IN_QUEUE,         �ظ�ע��
    CUST_REG_EXCH_MUTEX_REJECT,             MUTEX REJECT
    CUST_REG_EXCH_QUEUE_IN_ERR              �����ʧ��

��ͨ������ֵ��λ�������

*******************************************************************************/
extern int DRV_EXCH_CUST_FUNC_REG(exchCBReg cb);
/*liuyi++ �ýӿ����ĵ���δ���� --*/

/*liuyi++ �ýӿ����ĵ���δ����*/
/*****************************************************************************
* �� �� ��  : DRV_MSP_PROC_REG
*
* ��������  : DRV�ṩ��OM��ע�ắ��
*
* �������  : MSP_PROC_ID_E eFuncID, BSP_MspProc pFunc
* �������  : NA
*
* �� �� ֵ  : NA
*
* ����˵��  : ��ά�ɲ�ӿں���
*
*****************************************************************************/
#define DRV_MSP_PROC_REG(eFuncID, pFunc)
/*liuyi++ �ýӿ����ĵ���δ����--*/


/************************* �������ýṹ��(����ɾ��) ***************************/

/*TCP/IPЭ��ջ��ά�ɲⲶ�����Ϣ��ʶ*/
enum IPS_MNTN_TRACE_IP_MSG_TYPE_ENUM
{
    /* IP ���ݰ���ά�ɲ��ϱ� */
    ID_IPS_TRACE_IP_ADS_UL                  = 0xD030,
    ID_IPS_TRACE_IP_ADS_DL                  = 0xD031,
    ID_IPS_TRACE_IP_USB_UL                  = 0xD032,
    ID_IPS_TRACE_IP_USB_DL                  = 0xD033,

    ID_IPS_TRACE_IP_MSG_TYPE_BUTT
};

static inline unsigned long USB_ETH_DrvSetHostAssembleParam(unsigned long ulHostOutTimeout)
{
    return 0;
}

static inline unsigned long USB_ETH_DrvSetDeviceAssembleParam(
    unsigned long ulEthTxMinNum,
    unsigned long ulEthTxTimeout,
    unsigned long ulEthRxMinNum,
    unsigned long ulEthRxTimeout)
{
    return 0;
}

static inline unsigned int BSP_USB_GetAvailabePortType(unsigned char *pucPortType,
                            unsigned long *pulPortNum, unsigned long ulPortMax)
{
    return 0;
}
#define DRV_USB_GET_AVAILABLE_PORT_TYPE(pucPortType, pulPortNum, ulPortMax)  \
                BSP_USB_GetAvailabePortType(pucPortType, pulPortNum, ulPortMax)


static inline int USB_otg_switch_get(unsigned char *pucValue)
{
    return 0;
}
#define DRV_USB_PHY_SWITCH_GET(value) USB_otg_switch_get(value)
#define AT_USB_SWITCH_SET_VBUS_VALID            1
#define AT_USB_SWITCH_SET_VBUS_INVALID          2
#define USB_SWITCH_ON       1
#define USB_SWITCH_OFF      0


typedef void (*HSIC_UDI_ENABLE_CB_T)(void);
static inline unsigned int BSP_HSIC_RegUdiEnableCB(HSIC_UDI_ENABLE_CB_T pFunc)
{
    return 0;
}
#define DRV_HSIC_REGUDI_ENABLECB(x) BSP_HSIC_RegUdiEnableCB(x)

typedef void (*HSIC_UDI_DISABLE_CB_T)(void);
static inline unsigned int BSP_HSIC_RegUdiDisableCB(HSIC_UDI_DISABLE_CB_T pFunc)
{
    return 0;
}
#define DRV_HSIC_REGUDI_DISABLECB(x) BSP_HSIC_RegUdiDisableCB(x)

static inline unsigned int BSP_GetHsicEnumStatus(void)
{
    return 0;
}
#define DRV_GET_HSIC_ENUM_STATUS() BSP_GetHsicEnumStatus()


/************************* �������ýṹ�� END *********************************/

//struct sk_buff;
typedef void (*USB_IPS_MNTN_TRACE_CB_T)(struct sk_buff *skb,unsigned short usType);
static inline unsigned int DRV_USB_REG_IPS_TRACECB(USB_IPS_MNTN_TRACE_CB_T pFunc) {
    return 0;
}

unsigned int DRV_UTRACE_START(unsigned char * pData);
unsigned int DRV_UTRACE_TERMINATE(unsigned char * pData);
#endif

#ifdef __cplusplus
}
#endif
#endif