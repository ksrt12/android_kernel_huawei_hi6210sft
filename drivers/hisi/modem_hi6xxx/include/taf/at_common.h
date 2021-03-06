/*****************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

******************************************************************************
  文 件 名   : at_common.h
  版 本 号   : V1.0
  作    者   : 崔军强/64416
  生成日期   : 2011年4月9日
  功能描述   : AT的通用头文件接口
  函数列表   :
  修改历史   :
  1.日    期 : 2011年4月9日
    作    者 : 崔军强/64416
    修改内容 : 创建文件

*****************************************************************************/

#ifndef __AT_COMMON_H__
#define __AT_COMMON_H__

#include "vos.h"


#ifdef __cplusplus
extern "C"
{
#endif

/* Added by f00179208 for V9R1 split AT head file, 2012-10-19 begin */
extern VOS_VOID* At_HeapAllocD(VOS_UINT32 ulSize);
extern VOS_VOID At_HeapFreeD(VOS_VOID *pAddr);

#define AT_MALLOC(ulSize)          At_HeapAllocD(ulSize)
#define AT_FREE(pAddr)             At_HeapFreeD(pAddr)
/* Added by f00179208 for V9R1 split AT head file, 2012-10-19 end */


/*DTS2012011201043 w00182550 start in 2012-01-02*/
#define AT_DISABLE_CSIM                       (0)

typedef struct
{
    VOS_UINT32 ulEnableCSIM;
}NVIM_ENALBE_CSIM_STRU;

/*DTS2012011201043 w00182550 end in 2012-01-02*/

typedef struct
{
    VOS_UINT16      ucIndex;        /* AT通道ID */
    VOS_UINT16      aucRsv;
    VOS_UINT32      ulMsgId;        /* 原语ID */
    VOS_UINT16      aucData[4];     /* 原语内容 */
}TDS_AT_PS_MSG_INFO_STRU;


/*****************************************************************************
 Prototype       : TDS_PsAppSendMsg
 Description     : PS调用APP的接口，用于从DSP接收TDS维修原语(AT实现，PS调用)
 Input           : VOS_UINT32 ulLen 消息长度(数据长度不大于48字节)
                   TDS_AT_PS_MSG_INFO_STRU * pstTdsAppPsMsg 消息内容
 Output          : None.
 Return Value    : 成功:0,失败:其他值
 History         : ---
 *****************************************************************************/
extern VOS_UINT32 TDS_PsAtSendMsg(VOS_UINT32 ulLen, TDS_AT_PS_MSG_INFO_STRU * pstTdsAppPsMsg);
#ifdef __cplusplus
}
#endif


#endif /*__MSP_AT_H__*/

