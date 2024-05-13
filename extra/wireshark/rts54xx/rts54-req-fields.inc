-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

local cmds_pf = {
  seq_num  = ProtoField.uint16("rts54.seq_num",  "seq_num",  base.DEC, nil),
  port_num  = ProtoField.uint8("rts54.port_num",  "port_num",  base.DEC, nil),

  direction  = ProtoField.uint8("I2C-direction", "direction", nil, {
    [0] = "in",
    [1] = "out",
    [54] = "out",
  }),

  chip_type  = ProtoField.uint8("Chip-Type", "chip_type", nil, {
    [0x54] = "RTS54XX",
  }),

  cmd     = ProtoField.uint8("rts54.cmd",     "cmd",     base.HEX, {
    [0x01] = "PREFIX",
    [0x08] = "PREFIX",
    [0x09] = "GET_RTK_STATUS",
    [0x0a] = "ACK_CC_CI",
    [0x0e] = "PREFIX",
    [0x20] = "PREFIX",
    [0x3a] = "GET_IC_STATUS",
  }),
  wr_len  = ProtoField.uint8("rts54.wr_len",  "wr_len",  base.DEC, nil),
--  sub_cmd = ProtoField.uint8("rts54.sub_cmd", "sub_cmd", base.HEX, nil),
  payload = ProtoField.bytes("rts54.payload", "payload_bytes"),
}

rts54_proto = Proto("rts54xx", "RTS54XX")
rts54_proto.fields = cmds_pf

local rdo_pf = {
  data32 = ProtoField.uint32("rts54.rdo_data", "RDO Data", base.HEX),

  ProtoField.uint32("rts54.rdo_moc", "Max Op Current", base.UNIT_STRING, {" * 10 mA"}, 0x000003ff),
  ProtoField.uint32("rts54.rdo_oc", "Op Current",      base.UNIT_STRING, {" * 10 mA"}, 0x000ffc00),
  ProtoField.uint32("rts54.rdo_reserved20", "reserved",                 base.HEX, nil, 0x00f00000),
  ProtoField.bool("rts54.rdo_usb", "No USB Support",                          32, nil, 0x01000000),
  ProtoField.bool("rts54.rdo_usb_comm", "USB Communication",                  32, nil, 0x02000000),
  ProtoField.bool("rts54.rdo_cap_mismatch", "Cap Mismatch",                   32, nil, 0x04000000),
  ProtoField.bool("rts54.rdo_give_back", "Give Back",                         32, nil, 0x08000000),
  ProtoField.uint32("rts54.rdo_obj_pos", "Obj Pos",                     base.DEC, nil, 0x70000000),
  ProtoField.bool("rts54.rdo_reserved31", "reserved",                         32, nil, 0x80000000),
}

Proto("rts54xx.rdo", "RDO").fields = rdo_pf

_gg_rdo_pf = rdo_pf

-- rts54xx cmd 0x01 (VENDOR_CMD_ENABLE)

local cmd_vce_pf = {
  sub_cmd  = ProtoField.uint8("rts54.sub_cmd",  "sub_cmd",  base.HEX, {
    [0xda] = "VENDOR_CMD_ENABLE",
  }),
  sub_cmd2 = ProtoField.uint8("rts54.sub_cmd2", "sub_cmd2", base.HEX, nil),
  sub_cmd3 = ProtoField.uint8("rts54.sub_cmd3", "sub_cmd3", base.HEX, nil),
}

-- rts54xx cmd 0x08

local cmd08_pf = {
  sub_cmd  = ProtoField.uint8("rts54.cmd08_sub_cmd",  "sub_cmd",  base.HEX, {
    [0x00] = "TCPM_RESET",
    [0x01] = "SET_NOTIFICATION_ENABLE",
    [0x03] = "SET_PDO",
    [0x04] = "SET_RDO",
    [0x44] = "AUTO_SET_RDO",
    [0x05] = "SET_TPC_RP",
    [0x19] = "SEND_VDM",
    [0x1a] = "SET_VDO",
    [0x1d] = "SET_TPC_CSD_OPERATION_MODE",
    [0x1f] = "SET_TPC_RECONNECT",
    [0x20] = "INIT_PD_AMS",
    [0x21] = "FORCE_SET_POWER_SWITCH",
    [0x23] = "SET_TPC_DISCONNECT",
    [0x24] = "ACK_POWER_CTRL_REQ",
    [0x26] = "SET_BYPASS_SVID",
    [0x27] = "SET_BBR_CTS",
    [0x28] = "SET_BC12",
    [0x2B] = "SET_SYS_PWR_STATE",
    [0x83] = "GET_PDO",
    [0x84] = "GET_RDO",
    [0x85] = "GET_TPC_RP",
    [0x99] = "GET_VDM",
    [0x9a] = "GET_VDO",
    [0x9d] = "GET_TPC_CSD_OPERATION_MODE",
    [0xa2] = "GET_AMS_STATUS",
    [0xf0] = "GET_CSD",
    [0xa6] = "GET_BYPASS_SVID",
    [0xa7] = "GET_CURRENT_PARTNER_SRC_PDO",
    [0xa8] = "GET_DATA_MESSAGE",
    [0xa9] = "GET_POWER_SWITCH_STATUS",
    [0xaa] = "GET_EPR_PDO",
  }),
  port_num = ProtoField.uint8("rts54.cmd08_port_num", "port_num", base.DEC),
--  reset_type = ProtoField.uint8("rts54.cmd08_reset_type", "reset_type", base.HEX),

  -- sub_cmd 0x00 TCPM_RESET
  tr_reset_type = ProtoField.uint8("rts54.cmd08_reset_type", "Reset Type",
		base.HEX, { [0] = "Soft", [1] = "Hard", }, 0x03),
  tr_reserved = ProtoField.uint8("rts54.cmd08_00_reserved", "Reserved",
		base.HEX, nil, 0xfc),

  data8 = ProtoField.uint8("rts54.cmd08_data", "data", base.HEX),
  data32 = ProtoField.uint32("rts54.cmd08_data", "data", base.HEX),

  set_tpc_rp_tc_rp = ProtoField.uint8("rts54.set_tpc_rp_tc_rp",
		"Type-C Rp", nil,
		{ [0] = "Reserved", [1] = "USB", [2] = "1.5A", [3] = "3.0A", }, 0x0c),

  set_tpc_rp_pd_rp = ProtoField.uint8("rts54.set_tpc_rp_pd_rp",
		"PD Rp", nil,
		{ [0] = "Reserved", [1] = "USB", [2] = "1.5A", [3] = "3.0A", }, 0x30),

}

-- cmd 08 01 SET_NOTIFICATION_ENABLE

local cmd_sne_data_bits = {
  "Command_Complete",
  "External_Supply_Change",
  "Power_Operation_Mode_Change",
  "Reserved",
  "Reserved",
  "Supported_Provider_Capabilities_Change",
  "Negotiated_Power_Level_Change",
  "PD_Reset_Complete",
  "Supported_CAM_change",
  "Battery_Charging_Status_Change",
  "Reserved",
  "Port_Partner_Changed",
  "Power_Direction_Changed",
  "Reserved",
  "Connect_Change",
  "Error",
  "IR_Drop",
  "Soft_Reset_Completed",
  "Error_Recovery_occurred",
  "PD_PIO_Status_Change",
  "Alternate_Flow_Change",
  "DP_Status_Change",
  "DFP_OCP_Change",
  "Port_operation_mode_Change",
  "Power_Control_Request",
  "VDM_Received",
  "Source_Sink_Cap_Received",
  "Data_Message_Received",
  "Reserved",
  "System_Miscellaneous_Change",
  "Reserved_and_shall_be_set_to_0",
  "PD_AMS_Change",
}
local cmd_sne_data_pf = pf_auto_bits32(cmd_sne_data_bits)

local cmd_sne_pf = {
--  data32 = ProtoField.uint32("rts54.cmd_sne_data32", "data32", base.HEX),
  table.unpack(cmd_sne_data_pf)
}

-- cmd 08 03 SET_PDO

local cmd_set_pdo_pf = {
  pdo_info = ProtoField.uint8("rts54.cmd_set_pdo_info", "spdo_info", base.HEX),
  spr_num =  ProtoField.uint8("rts54.cmd_set_pdo_spr_num", "spr_num",
		base.DEC, nil, 0x07),
  pdo_type = ProtoField.uint8("rts54.cmd_set_pdo_type", "pdo_type",
		base.DEC, { [0] = "Sink", [1] = "Source", }, 0x08),
  epr_num =  ProtoField.uint8("rts54.cmd_set_pdo_epr_num", "epr_num",
		base.DEC, nil, 0x70),
  reserved = ProtoField.uint8("rts54.cmd_set_pdo_reserved", "reserved",
		base.DEC, nil, 0x80),
  pdo =      ProtoField.uint32("rts54.cmd_set_pdo_data", "pdo", base.HEX),
}

-- cmd 08 1a SET_VDO

local cmd_set_vdo_pf = {
  vdo_info = ProtoField.uint8("rts54.cmd_set_vdo_info", "svdo_info", base.HEX),
  vdo_num =  ProtoField.uint8("rts54.cmd_set_vdo_num", "vdo_num",
		base.DEC, nil, 0x07),
  vdo_origin = ProtoField.uint8("rts54.cmd_set_vdo_origin", "vdo_origin",
		base.DEC, { [0] = "Original", [1] = "Port Partner", }, 0x08),
  reserved = ProtoField.uint8("rts54.cmd_set_vdo_reserved", "reserved",
		base.DEC, nil, 0xf0),

  vdo_type = ProtoField.uint8("rts54.cmd_set_vdo_type", "vdo_type", nil, {
    [0] =  "Reserved",
    [1] =  "ID HEADER VDO",
    [2] =  "Cert Stat VDO",
    [3] =  "Product VDO",
    [4] =  "Cable VDO",
    [5] =  "Alternate Mode Adapter VDO",
    [6] =  "SVID Response VDO1",
    [7] =  "SVID Response VDO2",
    [8] =  "SVID Response VDO3",
    [9] =  "SVID Response VDO4",
    [10] = "SVID Response VDO5",
    [11] = "SVID Response VDO6",
  }),
  vdo =      ProtoField.uint32("rts54.cmd_set_vdo_data", "vdo", base.HEX),
}

-- cmd 08 21 FORCE_SET_POWER_SWITCH

local cmd_force_set_power_switch_pf = {
  vsbin_en = ProtoField.uint8("rts54.cmd_fsps_vsbin_en", "vsbin_en", base.DEC, nil, 0x03),
  lp_en = ProtoField.uint8("rts54.cmd_fsps_lp_en",    "lp_en",    base.DEC, nil, 0x0c),
  reserved = ProtoField.uint8("rts54.cmd_fsps_reserved", "reserved", base.DEC, nil, 0x30),
  vsbin_en_c = ProtoField.bool("rts54.cmd_fsps_vsbin_en_control", "vsbin_en_c", 8, nil, 0x40),
  lp_en_c = ProtoField.bool("rts54.cmd_fsps_lp_en_control",     "lp_en_c",   8, nil, 0x80),
}

-- cmd 08 83 GET_PDO

local cmd_get_pdo_pf = {
  pdo_info = ProtoField.uint8("rts54.cmd_get_pdo_info", "pdo_info", base.HEX),
  pdo_source = ProtoField.uint8("rts54.cmd_get_pdo_source", "pdo_source",
		base.DEC, { [0] = "Sink", [1] = "Source", }, 0x01),
  pdo_pp = ProtoField.uint8("rts54.cmd_get_pdo_pp", "pdo_pp",
		base.DEC, { [0] = "TCPM", [1] = "Port Partner", }, 0x02),
  pdo_offset =  ProtoField.uint8("rts54.cmd_get_pdo_offset", "pdo_offset",
		base.DEC, nil, 0x1c),
  pdo_num =  ProtoField.uint8("rts54.cmd_get_pdo_num", "pdo_num",
		base.DEC, nil, 0xe0),

--  vsbin_en = ProtoField.uint8("rts54.cmd_fsps_vsbin_en", "vsbin_en", base.DEC, nil, 0x03),

}

-- cmd 0x09 GET_RTK_STATUS

local cmd09_pf = {
  offset   = ProtoField.uint8("rts54.cmd09_offset",  "offset",  base.DEC, nil),
  port_num = ProtoField.uint8("rts54.cmd09_port_num",  "port_num",  base.DEC, nil),
  sts_len  = ProtoField.uint8("rts54.cmd09_sts_len",  "sts_len",  base.DEC, nil),
}

-- cmd 0x0a (ACK_CC_CI)

local cmd0a_pf = {
  reserved = ProtoField.uint8("rts54.cmd0a_reserved",  "reserved",  base.HEX, nil),
  port_num = ProtoField.uint8("rts54.cmd0a_port_num",  "port_num",  base.DEC, nil),
  data32   = ProtoField.uint32("rts54.cmd0a_data32", "data32", base.HEX),
  command_complete = ProtoField.uint8("rts54.cmd0a_command_complete",  "command_complete",  base.HEX, nil),
}

-- rts54xx cmd 0x0e

local cmd0e_pf = {
  sub_cmd  = ProtoField.uint8("rts54.cmd0e_sub_cmd",  "sub_cmd",  base.HEX, {
    [0x01] = "PPM_RESET",
    [0x03] = "CONNECTOR_RESET",
    [0x06] = "GET_CAPABILITY",
    [0x07] = "GET_CONNECTOR_CAPABILITY",
    [0x08] = "SET_CCOM",
    [0x09] = "SET_UOR",
    [0x0b] = "SET_PDR",
    [0x0c] = "GET_ALTERNATE_MODE",
    [0x0d] = "GET_CONNECTOR_ALT_MODE_SUPPORTED",
    [0x0e] = "GET_CURRENT_CONNECTOR_ALT_MODE",
    [0x0f] = "SET_NEW_CONNECTOR_ALTERNATE_MODE",
    [0x10] = "UCSI_GET_PDOS",
    [0x11] = "GET_CABLE_PROPERTY",
    [0x12] = "UCSI_GET_CONNECTOR_STATUS",
    [0x13] = "UCSI_GET_ERROR_STATUS",
    [0x1c] = "UCSI_SET_SINK_PATH",
    [0x1d] = "UCSI_SET_PDOS",
    [0x1e] = "UCSI_READ_POWER_LEVEL",
    [0x21] = "UCSI_SET_USB",
    [0x22] = "UCSI_GET_LPM_PPM_INFO",
  }),

  reserved = ProtoField.uint8("rts54.cmd0e_reserved", "reserved", base.HEX, nil),

  -- sub_cmd 0x01 PPM_RESET
  pr_data_length = ProtoField.uint8("rts54.cmd0e_data_length", "data_length", base.HEX),

  -- sub_cmd 0x03 CONNECTION_RESET
  cr_connector_number = ProtoField.uint8("rts54.cmd0e_connector_number",
		"connector_number", nil, nil, 0x7f),
  cr_reset_type = ProtoField.uint8("rts54.cmd0e_reset_type",
		"reset_type", nil, { [0] = "Hard", [1] = "Data" }, 0x80),

  -- sub_cmd 0x08 SET_CCOM

  sccom_cc_operation_mode = ProtoField.uint16("rts54.cmd0e_cc_op_mode",
		"cc_op_mode", nil, nil, 0x0780),

  sccom_cc_operation_mode_rp = ProtoField.bool("rts54.cmd0e_cc_op_mode_rp",
		"Op Mode Rp", 16, nil, 0x0080),
  sccom_cc_operation_mode_rd = ProtoField.bool("rts54.cmd0e_cc_op_mode_rd",
		"Op Mode Rd", 16, nil, 0x0100),
  sccom_cc_operation_mode_drp = ProtoField.bool("rts54.cmd0e_cc_op_mode_drp",
		"Op Mode DRP", 16, nil, 0x0200),
  sccom_cc_operation_mode_z = ProtoField.bool("rts54.cmd0e_cc_op_mode_z",
		"Op Mode Disabled", 16, nil, 0x0400),

  -- sub_cmd 0x12 UCSI_GET_CONNECTOR_STATUS

}

-- rts54xx cmd 0x12

local cmd12_pf = {
  sub_cmd  = ProtoField.uint8("rts54.cmd12_sub_cmd",  "sub_cmd",  base.HEX, {
    [0x01] = "SET_FTS_STATUS",
    [0x02] = "GET_FTS_STATUS",
  }),
}

-- rts54xx cmd 0x20

local cmd20_pf = {
  sub_cmd  = ProtoField.uint8("rts54.cmd20_sub_cmd",  "sub_cmd",  base.HEX, {
    [0x00] = "SET_RETIMER_FW_UPDATE_MODE",
  }),
  port_num = ProtoField.uint8("rts54.cmd20_port_num", "port_num", base.DEC),
  fw_update_status = ProtoField.uint8("rts54.cmd20_fw_update_status", "FW_update_status", base.HEX),
}

-- rts54xx cmd 0x3a

local cmd3a_pf = {
  offset   = ProtoField.uint8("rts54.cmd3a_offset",   "offset",   base.DEC, nil),
  reserved = ProtoField.uint8("rts54.cmd3a_reserved", "reserved", base.HEX, nil),
  data_len = ProtoField.uint8("rts54.cmd3a_data_len", "data_len", base.DEC, nil),
}

local cmd_get_vdo_pf = {
  vdo_info = ProtoField.uint8("rts54.cmd_get_vdo_info", "vdo_info", base.HEX),
  vdo_num =  ProtoField.uint8("rts54.cmd_get_vdo_num", "vdo_num",
		base.DEC, nil, 0x07),
  vdo_origin = ProtoField.uint8("rts54.cmd_get_vdo_origin", "vdo_origin",
		base.DEC, {
			[0] = "Port",
			[1] = "SOP",
			[2] = "SOP'",
			[3] = "SOP''", }, 0x18),
  vdo_reserved = ProtoField.uint8("rts54.cmd_get_vdo_reserved", "vdo_reserved",
		base.DEC, nil, 0xe0),
  vdo_type = ProtoField.uint8("rts54.cmd_get_vdo_type", "vdo_type", base.HEX, {
    [0] =  "Reserved",
    [1] =  "ID HEADER VDO",
    [2] =  "Cert Stat VDO",
    [3] =  "Product VDO",
    [4] =  "Product Type VDO1",
    [5] =  "Product Type VDO2",
    [6] =  "Product Type VDO3",
    [7] =  "SVID Response VDO1",
    [8] =  "SVID Response VDO2",
    [9] =  "SVID Response VDO3",
    [10] = "SVID Response VDO4",
    [11] = "SVID Response VDO5",
    [12] = "SVID Response VDO6",
    [13] = "PD_VDO_DP_CAPS",
    [14] = "PD_VDO_DP_STATUS",
    [15] = "PD_VDO_DP_CFG",
  }),
}

local t = {
  cmds_pf = cmds_pf,
  rdo_pf = rdo_pf,
  cmd_vce_pf = cmd_vce_pf,
  cmd08_pf = cmd08_pf,
  cmd_sne_pf = cmd_sne_pf,
  cmd_sne_data_pf = cmd_sne_data_pf,
  cmd_set_pdo_pf = cmd_set_pdo_pf,
  cmd_set_vdo_pf = cmd_set_vdo_pf,
  cmd_force_set_power_switch_pf = cmd_force_set_power_switch_pf,
  cmd_get_pdo_pf = cmd_get_pdo_pf,
  cmd09_pf = cmd09_pf,
  cmd0a_pf = cmd0a_pf,
  cmd0e_pf = cmd0e_pf,
  cmd12_pf = cmd12_pf,
  cmd20_pf = cmd20_pf,
  cmd3a_pf = cmd3a_pf,
  cmd_get_vdo_pf = cmd_get_vdo_pf,

--  data32 = ProtoField.uint32("rts54.data", "data", base.HEX),
}

rts54_fields = t
