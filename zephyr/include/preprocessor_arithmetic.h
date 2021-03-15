/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if !defined(ZEPHYR_INCLUDE_PREPROCESSOR_ARITHMETIC_H_)
#define ZEPHYR_INCLUDE_PREPROCESSOR_ARITHMETIC_H_

/**
 * @def INC
 *
 * @brief output x + 1
 *
 * @param x range: 0 ~ 98
 */
#define INC(x) INC_ ## x

#define INC_0  01
#define INC_01 02
#define INC_02 03
#define INC_03 04
#define INC_04 05
#define INC_05 06
#define INC_06 07
#define INC_07 08
#define INC_08 09
#define INC_09 10
#define INC_10 11
#define INC_11 12
#define INC_12 13
#define INC_13 14
#define INC_14 15
#define INC_15 16
#define INC_16 17
#define INC_17 18
#define INC_18 19
#define INC_19 20
#define INC_20 21
#define INC_21 22
#define INC_22 23
#define INC_23 24
#define INC_24 25
#define INC_25 26
#define INC_26 27
#define INC_27 28
#define INC_28 29
#define INC_29 30
#define INC_30 31
#define INC_31 32
#define INC_32 33
#define INC_33 34
#define INC_34 35
#define INC_35 36
#define INC_36 37
#define INC_37 38
#define INC_38 39
#define INC_39 40
#define INC_40 41
#define INC_41 42
#define INC_42 43
#define INC_43 44
#define INC_44 45
#define INC_45 46
#define INC_46 47
#define INC_47 48
#define INC_48 49
#define INC_49 50
#define INC_50 51
#define INC_51 52
#define INC_52 53
#define INC_53 54
#define INC_54 55
#define INC_55 56
#define INC_56 57
#define INC_57 58
#define INC_58 59
#define INC_59 60
#define INC_60 61
#define INC_61 62
#define INC_62 63
#define INC_63 64
#define INC_64 65
#define INC_65 66
#define INC_66 67
#define INC_67 68
#define INC_68 69
#define INC_69 70
#define INC_70 71
#define INC_71 72
#define INC_72 73
#define INC_73 74
#define INC_74 75
#define INC_75 76
#define INC_76 77
#define INC_77 78
#define INC_78 79
#define INC_79 80
#define INC_80 81
#define INC_81 82
#define INC_82 83
#define INC_83 84
#define INC_84 85
#define INC_85 86
#define INC_86 87
#define INC_87 88
#define INC_88 89
#define INC_89 90
#define INC_90 91
#define INC_91 92
#define INC_92 93
#define INC_93 94
#define INC_94 95
#define INC_95 96
#define INC_96 97
#define INC_97 98
#define INC_98 99

#define INC_1  INC_01
#define INC_2  INC_02
#define INC_3  INC_03
#define INC_4  INC_04
#define INC_5  INC_05
#define INC_6  INC_06
#define INC_7  INC_07
#define INC_8  INC_08
#define INC_9  INC_09

/**
 * @def ADD
 *
 * @brief output x + y
 *
 * @param x range: 0 ~ 99
 * @param y range: 0 ~ 99
 *
 * Note: x + y should be in the following range:
 *   0 <= x + y <= 99
 */
#define ADD(x, y) ADD_ ##y(x)

#define ADD_0(x)  x
#define ADD_01(x) ADD_0(INC(x))
#define ADD_02(x) ADD_01(INC(x))
#define ADD_03(x) ADD_02(INC(x))
#define ADD_04(x) ADD_03(INC(x))
#define ADD_05(x) ADD_04(INC(x))
#define ADD_06(x) ADD_05(INC(x))
#define ADD_07(x) ADD_06(INC(x))
#define ADD_08(x) ADD_07(INC(x))
#define ADD_09(x) ADD_08(INC(x))
#define ADD_10(x) ADD_09(INC(x))
#define ADD_11(x) ADD_10(INC(x))
#define ADD_12(x) ADD_11(INC(x))
#define ADD_13(x) ADD_12(INC(x))
#define ADD_14(x) ADD_13(INC(x))
#define ADD_15(x) ADD_14(INC(x))
#define ADD_16(x) ADD_15(INC(x))
#define ADD_17(x) ADD_16(INC(x))
#define ADD_18(x) ADD_17(INC(x))
#define ADD_19(x) ADD_18(INC(x))
#define ADD_20(x) ADD_19(INC(x))
#define ADD_21(x) ADD_20(INC(x))
#define ADD_22(x) ADD_21(INC(x))
#define ADD_23(x) ADD_22(INC(x))
#define ADD_24(x) ADD_23(INC(x))
#define ADD_25(x) ADD_24(INC(x))
#define ADD_26(x) ADD_25(INC(x))
#define ADD_27(x) ADD_26(INC(x))
#define ADD_28(x) ADD_27(INC(x))
#define ADD_29(x) ADD_28(INC(x))
#define ADD_30(x) ADD_29(INC(x))
#define ADD_31(x) ADD_30(INC(x))
#define ADD_32(x) ADD_31(INC(x))
#define ADD_33(x) ADD_32(INC(x))
#define ADD_34(x) ADD_33(INC(x))
#define ADD_35(x) ADD_34(INC(x))
#define ADD_36(x) ADD_35(INC(x))
#define ADD_37(x) ADD_36(INC(x))
#define ADD_38(x) ADD_37(INC(x))
#define ADD_39(x) ADD_38(INC(x))
#define ADD_40(x) ADD_39(INC(x))
#define ADD_41(x) ADD_40(INC(x))
#define ADD_42(x) ADD_41(INC(x))
#define ADD_43(x) ADD_42(INC(x))
#define ADD_44(x) ADD_43(INC(x))
#define ADD_45(x) ADD_44(INC(x))
#define ADD_46(x) ADD_45(INC(x))
#define ADD_47(x) ADD_46(INC(x))
#define ADD_48(x) ADD_47(INC(x))
#define ADD_49(x) ADD_48(INC(x))
#define ADD_50(x) ADD_49(INC(x))
#define ADD_51(x) ADD_50(INC(x))
#define ADD_52(x) ADD_51(INC(x))
#define ADD_53(x) ADD_52(INC(x))
#define ADD_54(x) ADD_53(INC(x))
#define ADD_55(x) ADD_54(INC(x))
#define ADD_56(x) ADD_55(INC(x))
#define ADD_57(x) ADD_56(INC(x))
#define ADD_58(x) ADD_57(INC(x))
#define ADD_59(x) ADD_58(INC(x))
#define ADD_60(x) ADD_59(INC(x))
#define ADD_61(x) ADD_60(INC(x))
#define ADD_62(x) ADD_61(INC(x))
#define ADD_63(x) ADD_62(INC(x))
#define ADD_64(x) ADD_63(INC(x))
#define ADD_65(x) ADD_64(INC(x))
#define ADD_66(x) ADD_65(INC(x))
#define ADD_67(x) ADD_66(INC(x))
#define ADD_68(x) ADD_67(INC(x))
#define ADD_69(x) ADD_68(INC(x))
#define ADD_70(x) ADD_69(INC(x))
#define ADD_71(x) ADD_70(INC(x))
#define ADD_72(x) ADD_71(INC(x))
#define ADD_73(x) ADD_72(INC(x))
#define ADD_74(x) ADD_73(INC(x))
#define ADD_75(x) ADD_74(INC(x))
#define ADD_76(x) ADD_75(INC(x))
#define ADD_77(x) ADD_76(INC(x))
#define ADD_78(x) ADD_77(INC(x))
#define ADD_79(x) ADD_78(INC(x))
#define ADD_80(x) ADD_79(INC(x))
#define ADD_81(x) ADD_80(INC(x))
#define ADD_82(x) ADD_81(INC(x))
#define ADD_83(x) ADD_82(INC(x))
#define ADD_84(x) ADD_83(INC(x))
#define ADD_85(x) ADD_84(INC(x))
#define ADD_86(x) ADD_85(INC(x))
#define ADD_87(x) ADD_86(INC(x))
#define ADD_88(x) ADD_87(INC(x))
#define ADD_89(x) ADD_88(INC(x))
#define ADD_90(x) ADD_89(INC(x))
#define ADD_91(x) ADD_90(INC(x))
#define ADD_92(x) ADD_91(INC(x))
#define ADD_93(x) ADD_92(INC(x))
#define ADD_94(x) ADD_93(INC(x))
#define ADD_95(x) ADD_94(INC(x))
#define ADD_96(x) ADD_95(INC(x))
#define ADD_97(x) ADD_96(INC(x))
#define ADD_98(x) ADD_97(INC(x))
#define ADD_99(x) ADD_98(INC(x))

#define ADD_1(x) ADD_01(x)
#define ADD_2(x) ADD_02(x)
#define ADD_3(x) ADD_03(x)
#define ADD_4(x) ADD_04(x)
#define ADD_5(x) ADD_05(x)
#define ADD_6(x) ADD_06(x)
#define ADD_7(x) ADD_07(x)
#define ADD_8(x) ADD_08(x)
#define ADD_9(x) ADD_09(x)

/**
 * @def DEC
 *
 * @brief output x - 1
 *
 * @param x range: 1 ~ 99
 */
#define DEC(x) DEC_ ## x

#define DEC_99 98
#define DEC_98 97
#define DEC_97 96
#define DEC_96 95
#define DEC_95 94
#define DEC_94 93
#define DEC_93 92
#define DEC_92 91
#define DEC_91 90
#define DEC_90 89
#define DEC_89 88
#define DEC_88 87
#define DEC_87 86
#define DEC_86 85
#define DEC_85 84
#define DEC_84 83
#define DEC_83 82
#define DEC_82 81
#define DEC_81 80
#define DEC_80 79
#define DEC_79 78
#define DEC_78 77
#define DEC_77 76
#define DEC_76 75
#define DEC_75 74
#define DEC_74 73
#define DEC_73 72
#define DEC_72 71
#define DEC_71 70
#define DEC_70 69
#define DEC_69 68
#define DEC_68 67
#define DEC_67 66
#define DEC_66 65
#define DEC_65 64
#define DEC_64 63
#define DEC_63 62
#define DEC_62 61
#define DEC_61 60
#define DEC_60 59
#define DEC_59 58
#define DEC_58 57
#define DEC_57 56
#define DEC_56 55
#define DEC_55 54
#define DEC_54 53
#define DEC_53 52
#define DEC_52 51
#define DEC_51 50
#define DEC_50 49
#define DEC_49 48
#define DEC_48 47
#define DEC_47 46
#define DEC_46 45
#define DEC_45 44
#define DEC_44 43
#define DEC_43 42
#define DEC_42 41
#define DEC_41 40
#define DEC_40 39
#define DEC_39 38
#define DEC_38 37
#define DEC_37 36
#define DEC_36 35
#define DEC_35 34
#define DEC_34 33
#define DEC_33 32
#define DEC_32 31
#define DEC_31 30
#define DEC_30 29
#define DEC_29 28
#define DEC_28 27
#define DEC_27 26
#define DEC_26 25
#define DEC_25 24
#define DEC_24 23
#define DEC_23 22
#define DEC_22 21
#define DEC_21 20
#define DEC_20 19
#define DEC_19 18
#define DEC_18 17
#define DEC_17 16
#define DEC_16 15
#define DEC_15 14
#define DEC_14 13
#define DEC_13 12
#define DEC_12 11
#define DEC_11 10
#define DEC_10 09
#define DEC_09 08
#define DEC_08 07
#define DEC_07 06
#define DEC_06 05
#define DEC_05 04
#define DEC_04 03
#define DEC_03 02
#define DEC_02 01
#define DEC_01 0

#define DEC_9 DEC_09
#define DEC_8 DEC_08
#define DEC_7 DEC_07
#define DEC_6 DEC_06
#define DEC_5 DEC_05
#define DEC_4 DEC_04
#define DEC_3 DEC_03
#define DEC_2 DEC_02
#define DEC_1 DEC_01

/**
 * @def SUB
 *
 * @brief output x - y
 *
 * @param x range: 0 ~ 99
 * @param y range: 0 ~ 99
 *
 * Note: x - y should be in the following range:
 *   0 <= x + y <= 99
 */
#define SUB(x, y) SUB_ ##y(x)

#define SUB_0(x)  x
#define SUB_01(x) SUB_0(DEC(x))
#define SUB_02(x) SUB_01(DEC(x))
#define SUB_03(x) SUB_02(DEC(x))
#define SUB_04(x) SUB_03(DEC(x))
#define SUB_05(x) SUB_04(DEC(x))
#define SUB_06(x) SUB_05(DEC(x))
#define SUB_07(x) SUB_06(DEC(x))
#define SUB_08(x) SUB_07(DEC(x))
#define SUB_09(x) SUB_08(DEC(x))
#define SUB_10(x) SUB_09(DEC(x))
#define SUB_11(x) SUB_10(DEC(x))
#define SUB_12(x) SUB_11(DEC(x))
#define SUB_13(x) SUB_12(DEC(x))
#define SUB_14(x) SUB_13(DEC(x))
#define SUB_15(x) SUB_14(DEC(x))
#define SUB_16(x) SUB_15(DEC(x))
#define SUB_17(x) SUB_16(DEC(x))
#define SUB_18(x) SUB_17(DEC(x))
#define SUB_19(x) SUB_18(DEC(x))
#define SUB_20(x) SUB_19(DEC(x))
#define SUB_21(x) SUB_20(DEC(x))
#define SUB_22(x) SUB_21(DEC(x))
#define SUB_23(x) SUB_22(DEC(x))
#define SUB_24(x) SUB_23(DEC(x))
#define SUB_25(x) SUB_24(DEC(x))
#define SUB_26(x) SUB_25(DEC(x))
#define SUB_27(x) SUB_26(DEC(x))
#define SUB_28(x) SUB_27(DEC(x))
#define SUB_29(x) SUB_28(DEC(x))
#define SUB_30(x) SUB_29(DEC(x))
#define SUB_31(x) SUB_30(DEC(x))
#define SUB_32(x) SUB_31(DEC(x))
#define SUB_33(x) SUB_32(DEC(x))
#define SUB_34(x) SUB_33(DEC(x))
#define SUB_35(x) SUB_34(DEC(x))
#define SUB_36(x) SUB_35(DEC(x))
#define SUB_37(x) SUB_36(DEC(x))
#define SUB_38(x) SUB_37(DEC(x))
#define SUB_39(x) SUB_38(DEC(x))
#define SUB_40(x) SUB_39(DEC(x))
#define SUB_41(x) SUB_40(DEC(x))
#define SUB_42(x) SUB_41(DEC(x))
#define SUB_43(x) SUB_42(DEC(x))
#define SUB_44(x) SUB_43(DEC(x))
#define SUB_45(x) SUB_44(DEC(x))
#define SUB_46(x) SUB_45(DEC(x))
#define SUB_47(x) SUB_46(DEC(x))
#define SUB_48(x) SUB_47(DEC(x))
#define SUB_49(x) SUB_48(DEC(x))
#define SUB_50(x) SUB_49(DEC(x))
#define SUB_51(x) SUB_50(DEC(x))
#define SUB_52(x) SUB_51(DEC(x))
#define SUB_53(x) SUB_52(DEC(x))
#define SUB_54(x) SUB_53(DEC(x))
#define SUB_55(x) SUB_54(DEC(x))
#define SUB_56(x) SUB_55(DEC(x))
#define SUB_57(x) SUB_56(DEC(x))
#define SUB_58(x) SUB_57(DEC(x))
#define SUB_59(x) SUB_58(DEC(x))
#define SUB_60(x) SUB_59(DEC(x))
#define SUB_61(x) SUB_60(DEC(x))
#define SUB_62(x) SUB_61(DEC(x))
#define SUB_63(x) SUB_62(DEC(x))
#define SUB_64(x) SUB_63(DEC(x))
#define SUB_65(x) SUB_64(DEC(x))
#define SUB_66(x) SUB_65(DEC(x))
#define SUB_67(x) SUB_66(DEC(x))
#define SUB_68(x) SUB_67(DEC(x))
#define SUB_69(x) SUB_68(DEC(x))
#define SUB_70(x) SUB_69(DEC(x))
#define SUB_71(x) SUB_70(DEC(x))
#define SUB_72(x) SUB_71(DEC(x))
#define SUB_73(x) SUB_72(DEC(x))
#define SUB_74(x) SUB_73(DEC(x))
#define SUB_75(x) SUB_74(DEC(x))
#define SUB_76(x) SUB_75(DEC(x))
#define SUB_77(x) SUB_76(DEC(x))
#define SUB_78(x) SUB_77(DEC(x))
#define SUB_79(x) SUB_78(DEC(x))
#define SUB_80(x) SUB_79(DEC(x))
#define SUB_81(x) SUB_80(DEC(x))
#define SUB_82(x) SUB_81(DEC(x))
#define SUB_83(x) SUB_82(DEC(x))
#define SUB_84(x) SUB_83(DEC(x))
#define SUB_85(x) SUB_84(DEC(x))
#define SUB_86(x) SUB_85(DEC(x))
#define SUB_87(x) SUB_86(DEC(x))
#define SUB_88(x) SUB_87(DEC(x))
#define SUB_89(x) SUB_88(DEC(x))
#define SUB_90(x) SUB_89(DEC(x))
#define SUB_91(x) SUB_90(DEC(x))
#define SUB_92(x) SUB_91(DEC(x))
#define SUB_93(x) SUB_92(DEC(x))
#define SUB_94(x) SUB_93(DEC(x))
#define SUB_95(x) SUB_94(DEC(x))
#define SUB_96(x) SUB_95(DEC(x))
#define SUB_97(x) SUB_96(DEC(x))
#define SUB_98(x) SUB_97(DEC(x))
#define SUB_99(x) SUB_98(DEC(x))

#define SUB_1(x) SUB_01(x)
#define SUB_2(x) SUB_02(x)
#define SUB_3(x) SUB_03(x)
#define SUB_4(x) SUB_04(x)
#define SUB_5(x) SUB_05(x)
#define SUB_6(x) SUB_06(x)
#define SUB_7(x) SUB_07(x)
#define SUB_8(x) SUB_08(x)
#define SUB_9(x) SUB_09(x)

#endif
