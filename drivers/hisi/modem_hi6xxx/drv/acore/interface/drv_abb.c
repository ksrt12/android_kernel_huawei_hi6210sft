/******************************************************************************

                  版权所有 (C), 2001-2011, 华为技术有限公司

 ******************************************************************************
  文 件 名   : drv_abb.c
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


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/*****************************************************************************
  2 函数声明
*****************************************************************************/
#if defined(BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)
extern BSP_S32 BSP_ABB_RegGet( BSP_U32 regAddr, BSP_U32 * getData );
extern BSP_S32 BSP_ABB_RegSet( BSP_U32 regAddr, BSP_U32 setData );
#endif

/*****************************************************************************
  3 函数实现
*****************************************************************************/

/*****************************************************************************
 函 数 名  : DRV_ABB_REG_GET
 功能描述  : get ABB register data
 输入参数  : regAddr --- 待读寄存器地址,ABB地址位宽为9bit,32bit留作扩展
 输出参数  : *getData --- 存放寄存器值,ABB数据位宽为8bit,32bit留作扩展
 返 回 值  : 0    - successful
             -1 - unsuccessful

 修改历史      :
  1.日    期   : 2012年6月19日
    作    者   : 杨诚 y00186965
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_ABB_REG_GET(unsigned int regAddr, unsigned int * getData)
{
#if defined (BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)
    return BSP_ABB_RegGet(regAddr, getData);
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}

/*****************************************************************************
 函 数 名  : DRV_ABB_REG_SET
 功能描述  : set ABB register data
 输入参数  : regAddr --- 待设置寄存器地址,ABB地址位宽为9bit,32bit留作扩展
             setData --- 设置的值,ABB数据位宽为8bit,32bit留作扩展
 输出参数  : 无
 返 回 值  : 0    - successful
             -1   - unsuccessful

 修改历史      :
  1.日    期   : 2012年6月19日
    作    者   : 杨诚 y00186965
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_ABB_REG_SET(unsigned int regAddr, unsigned int setData)
{
#if defined (BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)
    return BSP_ABB_RegSet(regAddr, setData);
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}

/*****************************************************************************
 函 数 名  : DRV_AUX_ABB_REG_SET
 功能描述  : set AUX ABB register data
 输入参数  : regAddr --- 待设置寄存器地址,ABB地址位宽为9bit,32bit留作扩展
             setData --- 设置的值,ABB数据位宽为8bit,32bit留作扩展
 输出参数  : 无
 返 回 值  : 0    - successful
             -1   - unsuccessful

 修改历史      :
  1.日    期   : 2012年6月19日
    作    者   : 杨诚 y00186965
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_AUX_ABB_REG_SET(unsigned int regAddr, unsigned int setData)
{
#if defined (BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)
    return BSP_OK;  /* temp tub，for RF6361 */
#endif

#if (defined BSP_CORE_APP)
    return 0;   /* 打桩 */
#endif
}

/*****************************************************************************
 函 数 名  : DRV_AUX_ABB_REG_GET
 功能描述  : get AUX ABB register data
 输入参数  : regAddr --- 待读寄存器地址,ABB地址位宽为9bit,32bit留作扩展
 输出参数  : *getData --- 存放寄存器值,ABB数据位宽为8bit,32bit留作扩展
 返 回 值  : 0    - successful
             -1 - unsuccessful

 修改历史      :
  1.日    期   : 2012年6月19日
    作    者   : 杨诚 y00186965
    修改内容   : 新生成函数

*****************************************************************************/
int DRV_AUX_ABB_REG_GET(unsigned int regAddr, unsigned int * getData)
{
#if defined (BSP_CORE_MODEM) || defined(PRODUCT_CFG_CORE_TYPE_MODEM)
    return BSP_OK;  /* temp tub，for RF6361 */
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

