-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

local resp_pf = {
  wr_len  = ProtoField.uint8("rts54r.wr_len",  "wr_len",  base.DEC, nil),
  payload = ProtoField.bytes("rts54r.payload", "payload_bytes"),

  gc_bm_attr = ProtoField.uint32("rts54r.bmAttributes", "bmAttributes", base.HEX, nil),

  gcp_bm_speed_supported = ProtoField.uint16("rts54r.bmSpeedSupported",
		"bmSpeedSupported", base.HEX, nil),
  gcp_b_current_capability = ProtoField.uint8("rts54r.bCurrentCapability",
		"bCurrentCapability", base.UNIT_STRING, {" * 50 mA"}),

  gcp_vbus_in_cable = ProtoField.uint8("rts54r.vbus_in_cable", "vbus_in_cable", base.DEC, nil, 0x01),
  gcp_cable_type = ProtoField.uint8("rts54r.cable_type", "cable_type", base.DEC, nil, 0x02),
  gcp_dir        = ProtoField.uint8("rts54r.dir",        "dir",        base.DEC, nil, 0x04),
  gcp_pet        = ProtoField.uint8("rts54r.pet",        "pet",        base.DEC, nil, 0x18),
  gcp_ms         = ProtoField.uint8("rts54r.ms",         "ms",         base.DEC, nil, 0x20),
  gcp_cpdr       = ProtoField.uint8("rts54r.cpdr",       "cpdr",       base.DEC, nil, 0xc0),


  gcp_latency  = ProtoField.uint8("rts54r.latency", "latency", base.DEC, nil, 0x0f),
  gcp_reserved = ProtoField.uint8("rts54r.reserved", "reserved", base.HEX, nil, 0xf0),

  gcc_operation_mode = ProtoField.uint32("rts54r.gcc_operation_mode", "gcc_o_m",
		base.HEX, nil, 0x000000ff),
  gcc_provider = ProtoField.bool("rts54r.gcc_provider", "gcc_p",
		32, nil, 0x00000100),
  gcc_consumer = ProtoField.bool("rts54r.gcc_consumer", "gcc_c",
		32, nil, 0x00000200),
  gcc_s_dfp = ProtoField.bool("rts54r.gcc_s_dfp", "gcc_s_dfp",
		32, nil, 0x00000400),
  gcc_s_ufp = ProtoField.bool("rts54r.gcc_s_ufp", "gcc_s_ufp",
		32, nil, 0x00000800),
  gcc_s_src = ProtoField.bool("rts54r.gcc_s_src", "gcc_s_src",
		32, nil, 0x00001000),
  gcc_s_snk = ProtoField.bool("rts54r.gcc_s_snk", "gcc_s_snk",
		32, nil, 0x00002000),
  gcc_eom = ProtoField.uint32("rts54r.gcc_eom", "gcc_eom",
		base.HEX, nil, 0x003fc000),
  gcc_mc = ProtoField.uint32("rts54r.gcc_mc", "gcc_mc",
		base.HEX, nil, 0x03c00000),
  gcc_rcp = ProtoField.bool("rts54r.gcc_rcp", "gcc_rcp",
		32, nil, 0x04000000),
  gcc_p_pd_r = ProtoField.uint32("rts54r.gcc_p_pd_r", "gcc_p_pd_r",
		base.HEX, nil, 0x18000000),
  gcc_res37 = ProtoField.uint32("rts54r.gcc_res", "gcc_res",
		base.HEX, nil, 0xe0000000),

  gis_rom_flash_code = ProtoField.uint8("rts54r.rom_flash_code", "rf_code", nil,
		{ [0] = "Rom", [1] = "Flash" }),
  gis_FW_main_version = ProtoField.uint8("FW_main_version", FWmversion,
		base.HEX, nil),
  gis_FW_sub1_version = ProtoField.uint8("FW_sub1_version", FWsub1version,
		base.HEX, nil),
  gis_FW_sub2_version = ProtoField.uint8("FW_sub2_version", FWsub2version,
		base.HEX, nil),
  gis_pd_status = ProtoField.uint8("PD_status", "PD_status", base.DEC,
		{ [0] = "PD not ready", [1] = "PD is ready", }, 0x01),
  gis_con_status = ProtoField.uint8("CON_status", "CON_status", base.DEC,
		{ [0] = "TypeC not connected", [1] = "TypeC is connected", }, 0x08),

  gis_v_p_id = ProtoField.none("V_P_ID"),
  gis_vid = ProtoField.uint16("VID", "VID", base.HEX, nil),
  gis_pid = ProtoField.uint16("PID", "PID", base.HEX, nil),

  gis_fbo = ProtoField.uint8("FBO", "FBO", base.HEX, nil),

  gis_pd_r_v = ProtoField.none("PD_R_V"),
  gis_pd_r_h = ProtoField.uint8("PD_R_H", "PD_R_H"),
  gis_pd_r_l = ProtoField.uint8("PD_R_L", "PD_R_L"),
  gis_pd_v_h = ProtoField.uint8("PD_V_H", "PD_V_H"),
  gis_pd_v_l = ProtoField.uint8("PD_V_L", "PD_V_L"),

  gvdo_vdo = ProtoField.bytes("rts54r.gvdo_vdo", "VDO"),

  gcs_cs_change = ProtoField.uint16("CS_Change_Mask", "Change", base.HEX),



  gcs_pom   = ProtoField.uint16("Port_Operation_Mode",    "Mode", base.DEC, {
	[1] = "Default Current",
	[2] = "BC",
	[3] = "PD",
	[4] = "1.5 A",
	[5] = "3 A",
	[6] = "5 A", } , 0x0007),
  gcs_co_st = ProtoField.bool("Connector_Status",      "Attached", 16, nil, 0x0008),
  gcs_pd    = ProtoField.uint16("Power_Direction",          "PD", base.DEC, {
	[0] = "Consumer",
	[1] = "Provider", }, 0x0010),
  gcs_cpf   = ProtoField.uint16("Connector_Partner_Flags", "CPF", base.DEC, {
	[0] = "USB",
	[1] = "Alt Mode",
	[2] = "USB4 gen 3",
	[3] = "USB4 gen 4", }, 0x1f70),
  gcs_cpt   = ProtoField.uint16("Connector_Partner_Type",  "CPT", base.DEC,
	{ [1] = "DFP",
	  [2] = "UFP",
	  [3] = "Powered Cable-UFP",
	  [4] = "Powered Cable+UFP",
	  [5] = "Debug Accessory",
	  [6] = "Audio Adapter", }, 0xe000),


  gcs_rdo = ProtoField.uint32("Request_Data_Object", "RDO", base.HEX),


  gcs_bccs = ProtoField.uint24("BCCS", "BCCS", base.DEC,
  		{ [0] = "Not charging",
		  [1] = "Nominal charging rate",
		  [2] = "Slow charging rate",
		  [3] = "Very slow charging rate", }, 0x03),
  gcs_pclr0 = ProtoField.uint24("PCLR_PBL", "PBL", base.DEC, nil, 0x04),
  gcs_pclr1 = ProtoField.uint24("PCLR_RPBL", "RPBL", base.DEC, nil, 0x08),
  gcs_pclr2 = ProtoField.uint24("PCLR_2", "PCLR_2", base.DEC, nil, 0x10),
  gcs_pclr3 = ProtoField.uint24("PCLR_3", "PCLR_3", base.DEC, nil, 0x20),
  gcs_pdv_l =   ProtoField.uint24("PD_Version_L", "PDVersion_L", base.DEC, nil, 0x00003fc0),
  gcs_pdv_h =   ProtoField.uint24("PD_Version_H", "PDVersion_H", base.DEC, nil, 0x003fc000),
  gcs_ori =   ProtoField.uint24("PD_Ori", "Ori", base.DEC,
		{ [0] = "Normal", [1] = "Flipped", }, 0x00400000),
  gcs_sps =   ProtoField.bool("Sink_Path_Status", "Sink", 24, nil, 0x00800000),


  gcs_rcps =   ProtoField.uint24("Reverse_Current_Protection_Status", "RCPS", base.DEC,
		{ [0] = "Safe", [1] = "Triggered", }, 0x01),
  gcs_prr =   ProtoField.bool("Power_Reading_Ready", "Power Reading Ready", 24, nil, 0x02),

  gcs_cs =    ProtoField.uint24("Current_Scale",   "CS",
		base.UNIT_STRING, {" * 5 mA"}, 0x0000001c),
  gcs_pc =    ProtoField.uint24("Peak_Current",    "PC",            base.DEC, nil, 0x001fffe0),

  gcs_ac =    ProtoField.uint24("Average_Current", "AC",            base.DEC, nil, 0x1fffe0),

  gcs_vs =    ProtoField.uint32("Voltage_Scale", "VS",
		base.UNIT_STRING, {" * 5 mV"}, 0x000001e0),
  gcs_vr =    ProtoField.uint32("Voltage_Reading", "VR",      base.DEC, nil, 0x01fffe00),
  gcs_r153 =  ProtoField.uint32("Reserved153", "Reserved153", base.HEX, nil, 0xfe000000),
}

local rts54_r_proto = Proto("rts54xx-r", "RTS54XX-r")
rts54_r_proto.fields = resp_pf

function decode_resp(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg(0, nil))
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))
  subtree:add(resp_pf.payload, resp_msg(1, -1))

  pktinfo.cols.info:set("Response")
end

-- 0x08 0x83 GET_PDO

function decode_resp_get_pdo(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg)
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))

  local i
  local npdo = (resp_msg:len() - 1) / 4
  for i = 1, npdo do
    subtree:add(resp_pf.payload, resp_msg(1 + 4 * (i - 1), 4))
  end

  pktinfo.cols.info:set("Response-GET-PDO (" .. npdo .. ")")
end

-- 0x08 0x84 GET_RDO

function decode_resp_get_rdo(tree, resp_msg, pktinfo)

  local t = tree:add(rts54_r_proto, resp_msg)
  t:add(resp_pf.wr_len, resp_msg(0, 1))
--  tree_add_fields32(t, rts_pf.rdo, resp_msg(1, 4))
  tree_add_fields32(t, rts54_fields.rdo_pf, resp_msg(1, 4))

  pktinfo.cols.info:set("Response-GET-RDO")
end

-- 0x08 0x9a GET_VDO

function decode_resp_get_vdo(tree, resp_msg, pktinfo, hints)
  local t2

  local t = tree:add(rts54_r_proto, resp_msg)
  t:add(resp_pf.wr_len, resp_msg(0, 1))

  local i
  local nvdo = (resp_msg:len() - 1) / 4
  for i = 1, nvdo do
    local vdo = resp_msg(1 + 4 * (i - 1), 4)

    if hints and hints[i] then
      local vdo_type = hints[i]
      if rts_vdo_n_pf[vdo_type] then
        tree_add_fields32(t, rts_vdo_n_pf[vdo_type], vdo)
      else
        t2 = t:add(resp_pf.gvdo_vdo, vdo)
        t2:append_text(" (type " .. vdo_type .. ")")
      end
    else
      t2 = t:add(resp_pf.gvdo_vdo, vdo)
      t2:append_text(" (unexpected)")
    end

  end

  pktinfo.cols.info:set("Response-Get-VDO")
end

-- 0x09 GET_RTK_STATUS

function decode_resp_get_rtk_status(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg)
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))
  subtree:add(resp_pf.payload, resp_msg(1))

  pktinfo.cols.info:set("Response-Get-RTK-Status")
end

-- 0x0e 0x06 GET_CAPABILITY

function decode_resp_get_capability(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg(0, nil))
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))
  subtree:add(resp_pf.gc_bm_attr, resp_msg(1, 4))
  subtree:add(resp_pf.payload, resp_msg(5))

  pktinfo.cols.info:set("Response-Get-Capabilities")
end

-- 0x0e 0x07 GET_CONNECTOR_CAPABILITY

function decode_resp_get_connector_capability(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg)
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))

  t2 = subtree:add(resp_pf.payload, resp_msg(1, 4))

  local w = resp_msg(1, 4):le_int()
  local f

  for _, pf in ipairs({
    resp_pf.gcc_operation_mode,
    resp_pf.gcc_provider,
    resp_pf.gcc_consumer,
    resp_pf.gcc_s_dfp,
    resp_pf.gcc_s_ufp,
    resp_pf.gcc_s_src,
    resp_pf.gcc_s_snk,
    resp_pf.gcc_eom,
    resp_pf.gcc_mc,
    resp_pf.gcc_rcp,
    resp_pf.gcc_p_pd_r,
    resp_pf.gcc_res37,
  }) do
    t2:add(pf, w)
  end

  pktinfo.cols.info:set("Response-Get-Connector-Capability")
end

-- 0x0e 0x11 GET_CABLE_PROPERTY

function decode_resp_get_cable_property(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg(0, nil))
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))
  subtree:add_le(resp_pf.gcp_bm_speed_supported, resp_msg(1, 2))
  subtree:add(resp_pf.gcp_b_current_capability, resp_msg(3, 1))

  t = subtree:add(resp_pf.payload, resp_msg(4, 1))
  t:add(resp_pf.gcp_vbus_in_cable, resp_msg(5, 1))
  t:add(resp_pf.gcp_cable_type,    resp_msg(5, 1))
  t:add(resp_pf.gcp_dir,           resp_msg(5, 1))
  t:add(resp_pf.gcp_pet,           resp_msg(5, 1))
  t:add(resp_pf.gcp_ms,            resp_msg(5, 1))
  t:add(resp_pf.gcp_cpdr,          resp_msg(5, 1))


  t = subtree:add(resp_pf.payload, resp_msg(5, 1))
  t:add(resp_pf.gcp_latency,  resp_msg(5, 1))
  t:add(resp_pf.gcp_reserved, resp_msg(5, 1))

  pktinfo.cols.info:set("Response-Get-Cable-Property")
end

-- 0x0e 0x12 UCSI_GET_CONNECTOR_STATUS

function decode_resp_ucsi_get_connector_status(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg(0, nil))
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))

  subtree:add(resp_pf.gcs_cs_change, resp_msg(1, 2))

  --subtree:add(resp_pf.gcs_pom, resp_msg(3, 2))
  t = subtree:add(resp_pf.payload, resp_msg(3, 2))
  t:add_le(resp_pf.gcs_pom,   resp_msg(3, 2))
  t:add_le(resp_pf.gcs_co_st, resp_msg(3, 2))
  t:add_le(resp_pf.gcs_pd,    resp_msg(3, 2))
  t:add_le(resp_pf.gcs_cpf,   resp_msg(3, 2))
  t:add_le(resp_pf.gcs_cpt,   resp_msg(3, 2))


  subtree:add(resp_pf.gcs_rdo, resp_msg(5, 4))

  t = subtree:add(resp_pf.payload, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_bccs,  resp_msg(9, 3))
  t:add_le(resp_pf.gcs_pclr0, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_pclr1, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_pclr2, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_pclr3, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_pdv_l, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_pdv_h, resp_msg(9, 3))
  t:add_le(resp_pf.gcs_ori,   resp_msg(9, 3))
  t:add_le(resp_pf.gcs_sps,   resp_msg(9, 3))

  t = subtree:add(resp_pf.payload, resp_msg(12, 3))

  t:add_le(resp_pf.gcs_rcps,  resp_msg(12, 3))
  t:add_le(resp_pf.gcs_prr,   resp_msg(12, 3))
  t:add_le(resp_pf.gcs_cs,    resp_msg(12, 3))
  t:add_le(resp_pf.gcs_pc,    resp_msg(12, 3))

  t = subtree:add(resp_pf.payload, resp_msg(14, 3))
  t:add_le(resp_pf.gcs_ac,         resp_msg(14, 3))

  t = subtree:add(resp_pf.payload, resp_msg(16, 4))
  t:add_le(resp_pf.gcs_vs,      resp_msg(16, 4))
  t:add_le(resp_pf.gcs_vr,      resp_msg(16, 4))
  t:add_le(resp_pf.gcs_r153,    resp_msg(16, 4))

  pktinfo.cols.info:set("Response-UCSI-Get-Connector-Status")
end

-- 0x0e 0x13 UCSI_GET_ERROR_STATUS

function decode_resp_ucsi_get_error_status(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg(0, nil))
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))
  subtree:add(resp_pf.payload, resp_msg(1, 2))

  pktinfo.cols.info:set("Response-UCSI-Get-Error-Status")
end

-- 0x3a GET_IC_STATUS

function decode_resp_get_ic_status(tree, resp_msg, pktinfo)

  local subtree = tree:add(rts54_r_proto, resp_msg(0, nil))
  subtree:add(resp_pf.wr_len, resp_msg(0, 1))
  subtree:add(resp_pf.gis_rom_flash_code, resp_msg(1, 1))
  subtree:add(resp_pf.payload, resp_msg(2, 2))
  subtree:add(resp_pf.gis_FW_main_version, resp_msg(4, 1))
  subtree:add(resp_pf.gis_FW_sub1_version, resp_msg(5, 1))
  subtree:add(resp_pf.gis_FW_sub2_version, resp_msg(6, 1))
  subtree:add(resp_pf.payload, resp_msg(7, 1))
  t2 = subtree:add(resp_pf.payload, resp_msg(8, 1))
    t2:add(resp_pf.gis_pd_status, resp_msg(8, 1))
    t2:add(resp_pf.gis_con_status, resp_msg(8, 1))

  t2 = subtree:add(resp_pf.gis_v_p_id, resp_msg(9, 4))
    t2:append_text(": " ..
      string.format("VID 0x%04x, PID 0x%04x",
        resp_msg(9, 2):le_uint(), resp_msg(11, 2):le_uint()))
    t2:add_le(resp_pf.gis_vid, resp_msg(9, 2))
    t2:add_le(resp_pf.gis_pid, resp_msg(11, 2))

  subtree:add(resp_pf.payload, resp_msg(13, 1))
  subtree:add(resp_pf.gis_fbo, resp_msg(14, 1))

  subtree:add(resp_pf.payload, resp_msg(15, 7))

  t2 = subtree:add(resp_pf.gis_pd_r_v, resp_msg(22, 4))
    t2:append_text(": " ..
      string.format("PD Revision %d.%d, Version %d.%d",
        resp_msg(22, 1):uint(), resp_msg(23, 1):uint(),
        resp_msg(24, 1):uint(), resp_msg(25, 1):uint()))
    t2:add(resp_pf.gis_pd_r_h, resp_msg(22, 1))
    t2:add(resp_pf.gis_pd_r_l, resp_msg(23, 1))
    t2:add(resp_pf.gis_pd_v_h, resp_msg(24, 1))
    t2:add(resp_pf.gis_pd_v_l, resp_msg(25, 1))

  if resp_msg:len() >= (26 + 12) then
    -- the GET_IC_STATUS may only request partial results
    subtree:add(resp_pf.payload, resp_msg(26, 12))
  end

  pktinfo.cols.info:set("Response-Get-IC-Status")
end
