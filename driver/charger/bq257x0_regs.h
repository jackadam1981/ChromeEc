/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI bq257x0 battery charger driver.
 */

#ifndef __CROS_EC_BQ257X0_REGS_H
#define __CROS_EC_BQ257X0_REGS_H

#include "bq25710.h"

/* ChargeOption1 Register (0x30) */
#define BQ257X0_CHARGE_OPTION_1_CMP_REF_SHIFT	7
#define BQ257X0_CHARGE_OPTION_1_CMP_REF_BITS	1
#define BQ257X0_CHARGE_OPTION_1_CMP_REF__2P3	0
#define BQ257X0_CHARGE_OPTION_1_CMP_REF__1P2	1


#define BQ25710_CHARGE_OPTION_1_EN_PSYS_SHIFT		12
#define BQ25710_CHARGE_OPTION_1_EN_PSYS_BITS		1
#define xBQ25710_CHARGE_OPTION_1_EN_PSYS__disable	0
#define xBQ25710_CHARGE_OPTION_1_EN_PSYS__enable		1
#define BQ25720_CHARGE_OPTION_1_PSYS_CONFIG_SHIFT	12
#define BQ25720_CHARGE_OPTION_1_PSYS_CONFIG_BITS	2
#define BQ25720_CHARGE_OPTION_1_PSYS_CONFIG__PBUS_PBAT	0
#define BQ25720_CHARGE_OPTION_1_PSYS_CONFIG__OFF	3

/* ChargeOption2 Register (0x31) */
#define BQ25710_CHARGE_OPTION_2_BATOC_VTH_SHIFT		0
#define BQ25710_CHARGE_OPTION_2_BATOC_VTH_BITS		1
#define BQ25710_CHARGE_OPTION_2_BATOC_VTH__1P50		0
#define BQ25710_CHARGE_OPTION_2_BATOC_VTH__2P00		1

#define BQ25720_CHARGE_OPTION_2_BATOC_VTH_SHIFT		0
#define BQ25720_CHARGE_OPTION_2_BATOC_VTH_BITS		1
#define BQ25720_CHARGE_OPTION_2_BATOC_VTH__1P33		0
#define BQ25720_CHARGE_OPTION_2_BATOC_VTH__2P00		1

#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH_SHIFT		2
#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH_BITS		1
#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH__1P33		0
#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH__2P00		1

#define BQ257X0_CHARGE_OPTION_2_EN_ACOC_SHIFT		3
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC_BITS		1
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC__DISABLE	0
#define BQ257X0_CHARGE_OPTION_2_EN_ACOC__ENABLE		1


#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH_SHIFT		2
#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH_BITS		1
#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH__1P33		0
#define BQ257X0_CHARGE_OPTION_2_ACOC_VTH__2P00		1

/* ChargeOption3 Register (0x32) */
#define BQ257X0_CHARGE_OPTION_3_IL_AVG_SHIFT	3
#define BQ257X0_CHARGE_OPTION_3_IL_AVG_BITS	2
#define BQ25720_CHARGE_OPTION_3_IL_AVG__10A	1

/* ChargeOption4 Register (0x36) */
#define BQ25720_CHARGE_OPTION_4_VSYS_UVP_SHIFT	13
#define BQ25720_CHARGE_OPTION_4_VSYS_UVP_BITS	3
#define BQ25720_CHARGE_OPTION_4_VSYS_UVP__4P0	2

#define BQ25720_CHARGE_OPTION_4_IDCHG_DEG2_SHIFT	6
#define BQ25720_CHARGE_OPTION_4_IDCHG_DEG2_BITS		2
#define BQ25720_CHARGE_OPTION_4_IDCHG_DEG2__12MS	3

/* ProchotOption1 Register (0x34) */

#define BQ257X0_PROCHOT_OPTION_1_PP_ACOK_SHIFT		0
#define BQ257X0_PROCHOT_OPTION_1_PP_ACOK_BITS		1
#define BQ257X0_PROCHOT_OPTION_1_PP_ACOK__DISABLE	0
#define BQ257X0_PROCHOT_OPTION_1_PP_ACOK__ENABLE	1

#define BQ257X0_PROCHOT_OPTION_1_PP_BATPRES_SHIFT	1
#define BQ257X0_PROCHOT_OPTION_1_PP_BATPRES_BITS	1
#define BQ257X0_PROCHOT_OPTION_1_PP_BATPRES__DISABLE	0
#define BQ257X0_PROCHOT_OPTION_1_PP_BATPRES__ENABLE	1

#define BQ257X0_PROCHOT_OPTION_1_PP_INOM_SHIFT		4
#define BQ257X0_PROCHOT_OPTION_1_PP_INOM_BITS		1
#define BQ257X0_PROCHOT_OPTION_1_PP_INOM__DISABLE	0
#define BQ257X0_PROCHOT_OPTION_1_PP_INOM__ENABLE	1

/* ChargeCurrent Register */
#define BQ257X0_CHARGE_CURRENT_CHARGE_CURRENT_SHIFT	6
#define BQ257X0_CHARGE_CURRENT_CHARGE_CURRENT_BITS	7

#define BQ257X0_CHARGE_CURRENT_MASK	BQ257X0_MASK_(BQ257X0, \
						CHARGE_CURRENT, \
						CHARGE_CURRENT)

#define BQ257X0_MASK_(_chip, _reg, _field)				\
	GENMASK(							\
		(_chip##_##_reg##_##_field##_SHIFT +			\
		 _chip##_##_reg##_##_field##_BITS - 1),			\
		_chip##_##_reg##_##_field##_SHIFT)

#define BQ257X0_MASK(_reg, _field)	BQ257X0_MASK_(BQ257X0, _reg, _field)
#define BQ25710_MASK(_reg, _field)	BQ257X0_MASK_(BQ25710, _reg, _field)
#define BQ25720_MASK(_reg, _field)	BQ257X0_MASK_(BQ25720, _reg, _field)

#define GET_BQ25720(_reg, _field, _x)				\
	(((_x) >> BQ25720_##_reg##_##_field##_SHIFT) &		\
	 GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0))

#define SET_BQ25710(_reg, _field, _v, _x)				\
	(((_x) & ~BQ25710_MASK(_reg, _field)) |				\
	 (((_v) &							\
	   GENMASK(BQ25710_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25710_##_reg##_##_field##_SHIFT))

#define SET_BQ25720(_reg, _field, _v, _x)				\
	(((_x) & ~BQ25720_MASK(_reg, _field)) |				\
	 (((_v) &							\
	   GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25720_##_reg##_##_field##_SHIFT))

#define SET_BQ257X0(_reg, _field, _v, _x)				\
	(((_x) & ~BQ257X0_MASK(_reg, _field)) |				\
	 (((_v) &							\
	   GENMASK(BQ257X0_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ257X0_##_reg##_##_field##_SHIFT))

#define SET_BQ257X0_BY_NAME(_reg, _field, _e, _x)			\
	(((_x) & ~BQ257X0_MASK(_reg, _field)) |				\
	 ((BQ257X0_##_reg##_##_field##__##_e &				\
	   GENMASK(BQ257X0_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ257X0_##_reg##_##_field##_SHIFT))

#define SET_BQ25710_BY_NAME(_reg, _field, _e, _x)			\
	(((_x) & ~BQ25710_MASK(_reg, _field)) |				\
	 ((BQ25710_##_reg##_##_field##__##_e &				\
	   GENMASK(BQ25710_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25710_##_reg##_##_field##_SHIFT))

#define SET_BQ25720_BY_NAME(_reg, _field, _e, _x)			\
	(((_x) & ~BQ25720_MASK(_reg, _field)) |				\
	 ((BQ25720_##_reg##_##_field##__##_e &				\
	   GENMASK(BQ25720_##_reg##_##_field##_BITS - 1, 0)) <<		\
	  BQ25720_##_reg##_##_field##_SHIFT))

#define SET_CO1_BY_NAME(_f, _e, _x)	SET_BQ257X0_BY_NAME(CHARGE_OPTION_1, \
							    _f, _e, (_x))

#define SET_CO2(_field, _v, _x)		SET_BQ257X0(CHARGE_OPTION_2, \
							    _field, _v, (_x))
#define SET_CO2_BY_NAME(_field, _e, _x)	SET_BQ257X0_BY_NAME(CHARGE_OPTION_2, \
							    _field, _e, (_x))

#define SET_CO3(_field, _v, _x)		SET_BQ257X0(CHARGE_OPTION_3, \
							    _field, _v, (_x))

#define SET_CO4(_f, _v, _x)		SET_BQ25720(CHARGE_OPTION_4, _f, _v, (_x))
#define SET_CO4_BY_NAME(_f, _e, _x)	SET_BQ25720_BY_NAME(CHARGE_OPTION_4, _f, _e, (_x))

#define SET_PO1_BY_NAME(_field, _e, _x)	SET_BQ257X0_BY_NAME(PROCHOT_OPTION_1, \
							    _field, _e, (_x))


#endif /* __CROS_EC_BQ257X0_REGS_H */
