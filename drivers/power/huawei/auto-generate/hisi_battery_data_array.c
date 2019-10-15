/* This file should be auto-generated by tool, please don't manully modify it.*/
#include "hisi_battery_data_default.c"
#include "hisi_battery_data_sony_2000.c"
#include "hisi_battery_data_atl_3900.c"
#include "hisi_battery_data_lishen_2000.c"
#include "hisi_battery_data_lishen_2460.c"
#include "hisi_battery_data_guangyu_4000.c"
#include "hisi_battery_data_ATL_3000.c"
#include "hisi_battery_data_GY_3000.c"
#include "hisi_battery_data_LG_3000.c"
/*BEGIN PN:DTS2014092911047 Add by l84015384 20140930*/
#include "hisi_battery_data_Scud_2300.c"
#include "hisi_battery_data_Sunwoda_2300.c"
/*BEGIN PN:DTS2014102107088 Add by l84015384 20141021*/
#include "hisi_battery_data_Default_2300.c"
#include "hisi_battery_data_Default_3000.c"
/*END PN:DTS2014102107088 Add by l84015384 20141021*/
/*END PN:DTS2014092911047 Add by l84015384 20140930*/
/*Alice*/
#include "hisi_battery_data_Scud_2200.c"
#include "hisi_battery_data_SUNWODA_2200.c"
#include "hisi_battery_data_SUNWODA_2200_default.c"
/*CherryMini*/
#include "hisi_battery_data_SCUD_SDI_2550.c"
#include "hisi_battery_data_SCUD_SDI_200_2550.c"
#include "hisi_battery_data_GY_2550.c"
#include "hisi_battery_data_SUNWODA_2550.c"
#include "hisi_battery_data_SUNWODA_2550_default.c"
/*Carmel*/
#include "hisi_battery_data_CARMEL_ATL_3000.c"
#include "hisi_battery_data_CARMEL_SONY_3000.c"
#include "hisi_battery_data_CARMEL_LG_3000.c"
#include "hisi_battery_data_CARMEL_Default_3000.c"
#include "hisi_battery_data.h"
#if 0
static struct hisi_smartstar_coul_battery_data *bat_data_array_0[BAT_DATA_ARRAR_SIZE] = {
    &sony_2000_battery_data,    // must start with default data
    &lishen_2000_battery_data,
    NULL,    // must end with 0
};

static struct hisi_smartstar_coul_battery_data *bat_data_array_1[BAT_DATA_ARRAR_SIZE] = {
    &atl_3900_battery_data,    // must start with default data
    &guangyu_4000_battery_data,
    NULL,    // must end with 0
};

static struct hisi_smartstar_coul_battery_data *bat_data_array_2[BAT_DATA_ARRAR_SIZE] = {
    &lishen_2460_battery_data,    // must start with default data
    0,    // must end with 0
};
#endif 

/*CherryPlus*/
static struct hisi_smartstar_coul_battery_data *bat_data_array_0[BAT_DATA_ARRAR_SIZE] = {
    /*BEGIN PN:DTS2014102107088 Add by l84015384 20141021*/
    &Default_3000_battery_data,// must start with default data
    /*END PN:DTS2014102107088 Add by l84015384 20141021*/
    &ATL_3000_battery_data,
    &GY_3000_battery_data,
    &LG_3000_battery_data,
    NULL,    // must end with 0
};

/*CherryMini*/
static struct hisi_smartstar_coul_battery_data *bat_data_array_1[BAT_DATA_ARRAR_SIZE] = {
    &SUNWODA_2550_battery_data_default, // must start with default data
    &SUNWODA_2550_battery_data,
    &GY_2550_battery_data,
    &SCUD_SDI_200_2550_battery_data,
    &SCUD_SDI_2550_battery_data,
    NULL, // must end with 0
};

/*Alice*/
static struct hisi_smartstar_coul_battery_data *bat_data_array_2[BAT_DATA_ARRAR_SIZE] = {
    &SUNWODA_2200_battery_data_default, // must start with default data
    &SUNWODA_2200_battery_data,
    &Scud_2200_battery_data,
    NULL, // must end with 0
};

/*Carmel*/
static struct hisi_smartstar_coul_battery_data *bat_data_array_3[BAT_DATA_ARRAR_SIZE] = {
    &CARMEL_Default_3000_battery_data, // must start with default data
    &CARMEL_SONY_3000_battery_data,
    &CARMEL_ATL_3000_battery_data,
    &CARMEL_LG_3000_battery_data,
    NULL, // must end with 0
};
static p_bat_data_array bat_data_array_all[] = {
    &bat_data_array_0, // SS_PRODUCT_INDEX_0  
    &bat_data_array_1, // SS_PRODUCT_INDEX_1
    &bat_data_array_2, // SS_PRODUCT_INDEX_2
    &bat_data_array_3, // SS_PRODUCT_INDEX_3
};

