/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : drv_debug_led.c
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
#include "stdio.h"
#ifdef __OS_RTOSCK__
#else
#include <vxWorks.h>
#include <private/vmLibP.h>
#include <cacheLib.h>
#include "arm_pbxa9.h"
#include <cacheLib.h>
#endif
#include "BSP_TIMER.h"
#include "sys_config.h"
#include "soc_baseaddr_interface.h"
#include "string.h"
#endif

#include "soc_baseaddr_interface.h"
#include "soc_timer_interface.h"
#if defined (CHIP_BB_HI6210)
#include "soc_peri_sctrl_interface.h"
#elif defined (CHIP_BB_HI6620)
#include "soc_sctrl_interface.h"
#endif
#include <soc_ao_sctrl_interface.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 函数声明
*****************************************************************************/
#if 0/* defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)*/
extern BSP_S32 BSP_DBLED_ON( DBLED_ID_E ledId );
extern BSP_S32 BSP_DBLED_OFF( DBLED_ID_E ledId );
extern int BSP_SYSCTRL_REG_SET(unsigned int regAddr, unsigned int setData);
extern int BSP_SYSCTRL_REG_GET(unsigned int regAddr, unsigned int * getData);
#endif

/*****************************************************************************
  3 函数实现
*****************************************************************************/

/*****************************************************************************
 函 数 名  : DRV_DBLED_ON
 功能描述  : 调试灯灯点亮处理函数
 输入参数  : ledId:点灯ID,取值0~5
 输出参数  : 无
 返 回 值  : int
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2012年7月25日
    作    者   : 荆耀辉 00179452
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_DBLED_ON( unsigned int ledId )
{
#if 0/*defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)*/
    return BSP_DBLED_ON((DBLED_ID_E)ledId);
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}

/*****************************************************************************
 函 数 名  : DRV_DBLED_OFF
 功能描述  : 调试灯熄灭处理函数
 输入参数  : ledId:点灯ID,取值0~5
 输出参数  : 无
 返 回 值  : int
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2012年7月25日
    作    者   : 荆耀辉 00179452
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_DBLED_OFF( unsigned int ledId )
{
#if 0/*defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)*/
    return BSP_DBLED_OFF((DBLED_ID_E)ledId);
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}

/*****************************************************************************
 函 数 名  : DRV_SYSCTRL_REG_SET
 功能描述  : DRV提供统一的sysctrl写接口函数
 输入参数  : regAddr：SC寄存器偏移地址；
             setData: SC寄存器值。
 输出参数  : 无
 返 回 值  : int
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2012年7月25日
    作    者   : 荆耀辉 00179452
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_SYSCTRL_REG_SET(unsigned int regAddr, unsigned int setData)
{
#if 0/*defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)*/
    return BSP_SYSCTRL_REG_SET(regAddr, setData);
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}

/*****************************************************************************
 函 数 名  : DRV_SYSCTRL_REG_GET
 功能描述  : DRV提供统一的sysctrl读接口函数
 输入参数  : regAddr： SC寄存器偏移地址。
             getData：保存SC寄存器值的指针。
 输出参数  : 无
 返 回 值  : int
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2012年7月25日
    作    者   : 荆耀辉 00179452
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_SYSCTRL_REG_GET(unsigned int regAddr, unsigned int * getData)
{
#if 0/*defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)*/
    return BSP_SYSCTRL_REG_GET(regAddr, getData);
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}




#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

