/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : drv_charge.c
  版 本 号   : 初稿
  作    者   : c61362
  生成日期   : 2012年3月2日
  最近修改   :
  功能描述   : 底软给上层软件封装的接口层
  修改历史   :
  1.日    期   : 2012年3月2日
    作    者   : c61362
    修改内容   : 新建Drvinterface.c
    
  2.日    期   : 2013年2月19日
    作    者   : 蔡喜 220237
    修改内容   : 由Drvinterface.c拆分所得

******************************************************************************/

/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "BSP.h"
#if defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)
#include "sys_config.h"
#endif


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 函数声明
*****************************************************************************/

/*****************************************************************************
  3 函数实现
*****************************************************************************/
/*****************************************************************************
 函 数 名  : mdrv_misc_get_battery_state
 功能描述  :获取底层电池状态信息
 输入参数  :battery_state 电量信息
 输出参数  :battery_state 电量信息
 返回值：   0 操作成功
                         -1操作失败

*****************************************************************************/
int mdrv_misc_get_battery_state(BATT_STATE_S *battery_state)
{
    return 0;/*stub,sync from v7*/
}

/*****************************************************************************
 函 数 名  : mdrv_misc_get_charging_status
 功能描述  :查询目前是否正在充电中
 输入参数  :无
 输出参数  :无
 返回值：   0 未充电
                          1 充电中
*****************************************************************************/
int mdrv_misc_get_charging_status(void)
{
    return 0;/*stub,sync from v7*/
}

/*****************************************************************************
 函 数 名  : mdrv_misc_set_charge_state	
 功能描述  :使能或者禁止充电
 输入参数  :ulState      0:禁止充电
 						1:使能充电
 输出参数  :无
 返回值：    无
*****************************************************************************/
void mdrv_misc_set_charge_state(unsigned long ulState)
{
    return;/*stub,sync from v7*/
}

/*****************************************************************************
 函 数 名  : mdrv_misc_get_charge_state	
 功能描述  :查询
 输入参数  :
 输出参数  :无
 返回值：    无
*****************************************************************************/
int mdrv_misc_get_charge_state(void)
{
    return 0;/*stub,sync from v7*/
}

/*****************************************************************************
 函 数 名  : mdrv_misc_sply_battery
 功能描述  :查询
 输入参数  :
 输出参数  :无
 返回值：    无
*****************************************************************************/
int mdrv_misc_sply_battery(void)
{
    return 0;/*stub,sync from v7*/
}

/*****************************************************************************
 函 数 名  : mdrv_misc_get_cbc_state
 功能描述  : 返回电池状态和电量
 输入参数  :pusBcs 0:电池在供电 1:与电池连接电池未供电 2:没有与电池连接
                          pucBcl  0:电量不足或没有与电池连接
 输出参数  : pusBcs 0:电池在供电 1:与电池连接电池未供电 2:没有与电池连接
                          pucBcl  0:电量不足或没有与电池连接
 返回值：   0 操作成功
                         -1操作失败

*****************************************************************************/
int mdrv_misc_get_cbc_state(unsigned char *pusBcs,unsigned char *pucBcl)
{
    return 0;/*stub,sync from v7*/
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

