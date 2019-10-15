
/* DO NOT EDIT - Generated automatically by build.py */

static struct single_row_lut LG_3000_fcc_temp = {
	.x		= {-20, -10, 0, 25, 40, 60},
	.y		= {3093, 3107, 3105, 3170, 3139, 3067},
	.cols	= 6
};

static struct single_row_lut LG_3000_fcc_sf = {
	.x		= {0, 100, 200, 300, 400, 500},
	.y		= {100, 96, 94, 92, 90, 88},
	.cols	= 6
};

static struct sf_lut LG_3000_pc_sf = {
	.rows = 1,
	.cols = 1,
	.row_entries = {0},
	.percent = {100},
	.sf = {
		{100}
	}
};


static struct sf_lut LG_3000_rbatt_sf = {
	.rows = 28,
	.cols = 6,
	.row_entries = {-20, -10, 0, 25, 40, 60},
	.percent = {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
	.sf = {
		{2150, 1340, 721, 149, 116, 105, },
		{2147, 1338, 720, 149, 116, 105, },
		{1947, 1200, 694, 155, 120, 107, },
		{1863, 1117, 674, 161, 124, 111, },
		{1844, 1107, 672, 167, 130, 114, },
		{1812, 1106, 678, 175, 135, 117, },
		{1823, 1095, 677, 185, 140, 120, },
		{1844, 1111, 673, 199, 148, 125, },
		{1880, 1123, 670, 199, 154, 130, },
		{1930, 1150, 678, 169, 135, 116, },
		{1997, 1193, 708, 154, 121, 109, },
		{2076, 1251, 756, 158, 126, 112, },
		{2160, 1321, 818, 165, 133, 116, },
		{2248, 1400, 889, 172, 137, 120, },
		{2345, 1483, 963, 182, 135, 119, },
		{2459, 1570, 1044, 188, 131, 112, },
		{2618, 1661, 1125, 188, 132, 111, },
		{2865, 1787, 1224, 185, 134, 112, },
		{6112, 2810, 1460, 197, 140, 118, },
		{6697, 3079, 1592, 202, 141, 117, },
		{7275, 3352, 1729, 206, 143, 117, },
		{7819, 3647, 1884, 213, 144, 119, },
		{8352, 3959, 2048, 218, 144, 120, },
		{8890, 4232, 2199, 221, 148, 123, },
		{9495, 4608, 2386, 238, 156, 128, },
		{10120, 5012, 2622, 263, 162, 125, },
		{10828, 5562, 2991, 299, 167, 125, },
		{11962, 6368, 3532, 500, 186, 134, },
	}                                 
};                                        

static struct pc_temp_ocv_lut LG_3000_pc_temp_ocv = {
	.rows = 29,
	.cols = 6,
	.temp = {-20, -10, 0, 25, 40, 60},
	.percent = {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35, 30, 25, 20, 15, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
	.ocv = {
		{4329, 4327, 4324, 4349, 4314, 4308},
		{4239, 4246, 4248, 4288, 4254, 4250},
		{4170, 4181, 4187, 4229, 4198, 4195},
		{4111, 4123, 4132, 4173, 4144, 4141},
		{4054, 4073, 4081, 4118, 4093, 4091},
		{3984, 4017, 4030, 4067, 4045, 4042},
		{3935, 3958, 3978, 4017, 3998, 3998},
		{3893, 3915, 3930, 3971, 3959, 3958},
		{3860, 3875, 3887, 3927, 3918, 3918},
		{3835, 3844, 3851, 3880, 3871, 3870},
		{3817, 3821, 3824, 3844, 3837, 3838},
		{3802, 3805, 3806, 3819, 3814, 3815},
		{3787, 3792, 3792, 3799, 3796, 3795},
		{3774, 3780, 3780, 3782, 3780, 3779},
		{3759, 3766, 3767, 3770, 3764, 3760},
		{3742, 3751, 3753, 3756, 3747, 3736},
		{3724, 3729, 3732, 3733, 3722, 3711},
		{3700, 3704, 3705, 3692, 3691, 3679},
		{3669, 3679, 3677, 3679, 3671, 3662},
		{3659, 3672, 3671, 3677, 3670, 3660},
		{3648, 3663, 3664, 3673, 3667, 3657},
		{3634, 3651, 3655, 3668, 3662, 3652},
		{3615, 3636, 3642, 3656, 3651, 3639},
		{3590, 3611, 3621, 3626, 3623, 3610},
		{3560, 3578, 3587, 3583, 3583, 3571},
		{3518, 3533, 3536, 3527, 3532, 3520},
		{3458, 3468, 3464, 3454, 3463, 3451},
		{3377, 3375, 3358, 3358, 3365, 3354},
		{3200, 3200, 3200, 3200, 3200, 3200},
	}
};

static struct hisi_smartstar_coul_battery_data LG_3000_battery_data = {
	.id_voltage_min = 1450,
	.id_voltage_max = 1650,
	.fcc = 3000,
	.fcc_temp_lut  = &LG_3000_fcc_temp,
	.fcc_sf_lut = &LG_3000_fcc_sf,
	.pc_temp_ocv_lut = &LG_3000_pc_temp_ocv,
	.pc_sf_lut = &LG_3000_pc_sf,
	.rbatt_sf_lut = &LG_3000_rbatt_sf,
	.default_rbatt_mohm = 100,
    /*BEGIN PN:DTS2014082804634 Modified by l00189337 20140918*/
	.max_currentmA = 1100,
    .max_voltagemV = 4352,
    .max_cin_currentmA = 1200,
    .terminal_currentmA = 150,
    /*END PN:DTS2014082804634 Modified by l00189337 20140918*/
    .charge_in_temp_5 = 3,
    .charge_in_temp_10 = 3,
    .batt_brand = "SUNWODA",
};
