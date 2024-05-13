-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

local h = rts45_req_header_action

local response_handler_per_packet = {}
local response_hints_per_packet = {}

--
-- set up the handler for the response
--
-- the protocol is basically request-response.
-- the only way to know how to decode the response is by knowing
-- what the request was.
--
-- hints are additional parameters derived from the request to
-- help decode the response (where needed).
--

function set_response_handler(pktinfo, fn, hints)
  response_handler_per_packet[pktinfo.number] = fn
  response_hints_per_packet[pktinfo.number] = hints
end

--
-- return the most recent handler for decoding a response
--

function find_recent_handler(pktinfo)
  local v = nil
  local h = nil
  local n = pktinfo.number

  if not response_handler_per_packet[n - 1] then
    return nil
  end

  v = response_handler_per_packet[n - 1]
  h = response_hints_per_packet[n - 1]

  return { [0] = v, [1] = h }

end

--

-- rts54xx cmd 0x01 0xda VENDOR_CMD_ENABLE

local rts54xx_cmd01_proto = Proto("rts54xx.cmd01", "CMD 01")
rts54xx_cmd01_proto.fields = rts54_fields.cmd_vce_pf

local function decode_cmd_01_da(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd_vce_pf.sub_cmd2, bytes(0, 1))
  t:add_le(rts54_fields.cmd_vce_pf.sub_cmd3, bytes(1, 1))

  pktinfo.cols.info:set("Vendor Command Enable")
end

h[0x01] = {
  pf = rts54_fields.cmd_vce_pf.sub_cmd,
  [0xda] = decode_cmd_01_da,
}

-- rts54xx cmd 0x08 (prefix)

local rts54xx_cmd08_proto = Proto("rts54xx.cmd08", "CMD 08")
rts54xx_cmd08_proto.fields = rts54_fields.cmd08_pf

h[0x08] = {
  pf  = rts54_fields.cmd08_pf.sub_cmd,
}

-- rts54xx cmd 0x08 0x00 TCPM_RESET

local function decode_cmd_08_00(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  local t2 = t:add(rts54_fields.cmds_pf.payload, bytes(1, 1))
  t2:add_le(rts54_fields.cmd08_pf.tr_reset_type, bytes(1, 1))
  t2:add_le(rts54_fields.cmd08_pf.tr_reserved, bytes(1, 1))

  pktinfo.cols.info:set("TCPM Reset")
end

h[0x08][0x00] = decode_cmd_08_00

-- cmd 0x08 0x01 SET_NOTIFICATION_ENABLE

local rts54xx_cmd_sne_proto = Proto("rts54xx.cmd_sne", "SNE")
rts54xx_cmd_sne_proto.fields = rts54_fields.cmd_sne_pf

local function decode_cmd_08_01(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))
  local t2 = t:add_le(rts54_fields.cmd08_pf.data32, bytes(1, 4))
  t_add_list(t2, rts54_fields.cmd_sne_data_pf, bytes(1, 4))

  pktinfo.cols.info:set("Set Notification Enable")
end

h[0x08][0x01] = decode_cmd_08_01

-- cmd 0x08 0x03 SET_PDO

local rts54xx_cmd_set_pdo_proto = Proto("rts54xx.cmd_set_pdo", "SPDO")
rts54xx_cmd_sne_proto.fields = rts54_fields.cmd_set_pdo_pf
--rts54xx_cmd_sne_proto.fields = _gg_rdo_pf

local function decode_cmd_08_03(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  local t2 = t:add_le(rts54_fields.cmd_set_pdo_pf.pdo_info, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_pdo_pf.spr_num, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_pdo_pf.pdo_type, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_pdo_pf.epr_num, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_pdo_pf.reserved, bytes(1, 1))

  local npdo = lefield(bytes(1, 1), 0, 3)
  local i
  for i = 1, npdo do
    t:add_le(rts54_fields.cmd_set_pdo_pf.pdo, bytes(2 + 4 * (i - 1), 4))
  end

  pktinfo.cols.info:set("Set PDO")
end

h[0x08][0x03] = decode_cmd_08_03

-- cmd 0x08 0x04 SET_RDO

local function decode_cmd_08_04(t, bytes, pktinfo)
  local pf = rts54_fields.cmd08_pf

  t:add_le(pf.port_num, bytes(0, 1))
--  tree_add_fields32(t, rts_pf.rdo, bytes(1, 4))
  tree_add_fields32(t, rts54_fields.rdo_pf, bytes(1, 4))

  pktinfo.cols.info:set("Set RDO")
end

h[0x08][0x04] = decode_cmd_08_04

-- cmd 0x08 0x05 SET_TPC_RP

local function decode_cmd_08_05(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))
  local t2 = t:add_le(rts54_fields.cmd08_pf.data8, bytes(1, 1))
  t2:add_le(rts54_fields.cmd08_pf.set_tpc_rp_tc_rp, bytes(1, 1))
  t2:add_le(rts54_fields.cmd08_pf.set_tpc_rp_pd_rp, bytes(1, 1))

  pktinfo.cols.info:set("Set Type-C Rp")
end

h[0x08][0x05] = decode_cmd_08_05

-- cmd 0x08 0x19 SEND_VDM

local function decode_cmd_08_19(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  t:add_le(rts54_fields.cmd08_pf.data32, bytes(1, 4))
  local nvdo = (bytes:len() - (1 + 4)) / 4
  for i = 1, nvdo do
    t:add_le(rts54_fields.cmd08_pf.data32, bytes((1 + 4) + 4 * (i - 1), 4))
  end

  pktinfo.cols.info:set("Send VDM")
end

h[0x08][0x19] = decode_cmd_08_19

-- cmd 0x08 0x1a SET_VDO

local rts54xx_cmd_set_vdo_proto = Proto("rts54xx.cmd_set_vdo", "SVDO")
rts54xx_cmd_sne_proto.fields = rts54_fields.cmd_set_vdo_pf

local function decode_cmd_08_1a(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  local t2 = t:add_le(rts54_fields.cmd_set_vdo_pf.vdo_info, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_vdo_pf.vdo_num, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_vdo_pf.vdo_origin, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_set_vdo_pf.reserved, bytes(1, 1))

  local nvdo = lefield(bytes(1, 1), 0, 3)
  local i
  for i = 1, nvdo do
    t:add_le(rts54_fields.cmd_set_vdo_pf.vdo_type, bytes(2 + 5 * (i - 1), 1))
    t:add_le(rts54_fields.cmd_set_vdo_pf.vdo, bytes(3 + 5 * (i - 1), 4))
  end

  pktinfo.cols.info:set("Set VDO")
end

h[0x08][0x1a] = decode_cmd_08_1a

-- cmd 0x08 0x1d SET_TPC_CSD_OPREATIN_MODE

local function decode_cmd_08_1d(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))
  t:add_le(rts54_fields.cmd08_pf.data8, bytes(1, 1))

  pktinfo.cols.info:set("Set TPC CSD Opreatin Mode")
end

h[0x08][0x1d] = decode_cmd_08_1d

-- cmd 0x08 0x1f SET_TPC_RECONNECT

local function decode_cmd_08_1f(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))
  t:add_le(rts54_fields.cmd08_pf.data8, bytes(1, 1))

  pktinfo.cols.info:set("Set TPC Reconnect")
end

h[0x08][0x1f] = decode_cmd_08_1f

-- cmd 0x08 0x20 INIT_PD_AMS

local function decode_cmd_08_20(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))
  t:add_le(rts54_fields.cmd08_pf.data8, bytes(1, 1))

  pktinfo.cols.info:set("Init PD AMS")
end

h[0x08][0x20] = decode_cmd_08_20

-- cmd 0x08 0x21 FORCE_SET_POWER_SWITCH

local rts54xx_cmd_force_set_power_switch_proto = Proto("rts54xx.cmd_force_set_power_switch", "FSPW")
rts54xx_cmd_force_set_power_switch_proto.fields = rts54_fields.cmd_force_set_power_switch_pf

local function decode_cmd_08_21(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  t2 = t:add_le(rts54_fields.cmd08_pf.data8, bytes(1, 1))
  t2:add(rts54_fields.cmd_force_set_power_switch_pf.vsbin_en, bytes(1, 1))
  t2:add(rts54_fields.cmd_force_set_power_switch_pf.lp_en, bytes(1, 1))
  t2:add(rts54_fields.cmd_force_set_power_switch_pf.reserved, bytes(1, 1))
  t2:add(rts54_fields.cmd_force_set_power_switch_pf.vsbin_en_c, bytes(1, 1))
  t2:add(rts54_fields.cmd_force_set_power_switch_pf.lp_en_c, bytes(1, 1))

  pktinfo.cols.info:set("Force Set Power Switch")
end

h[0x08][0x21] = decode_cmd_08_21

-- cmd 0x08 0x23 SET_TPC_DISCONNECT

local function decode_cmd_08_23(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  pktinfo.cols.info:set("Set Type-C Disconnect")
end

h[0x08][0x23] = decode_cmd_08_23

-- cmd 0x08 0x24 ACK_POWER_CTRL_REQ

local function decode_cmd_08_24(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  pktinfo.cols.info:set("ACK Power Ctrl Req")
end

h[0x08][0x24] = decode_cmd_08_24

-- cmd 0x08 0x26 SET_BYPASS_SVID

-- TODO: data sheet says wr_len is 0x02, but it should
-- probably be 3 + 2 * SVID_NUM

local function decode_cmd_08_26(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  pktinfo.cols.info:set("Set Bypass SVID")
end

h[0x08][0x26] = decode_cmd_08_26

-- cmd 0x08 0x83 GET_PDO

local rts54xx_cmd_get_pdo_proto = Proto("rts54xx.cmd_get_pdo", "GPDO")
rts54xx_cmd_get_pdo_proto.fields = rts54_fields.cmd_get_pdo_pf

local function decode_cmd_08_83(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  local t2 = t:add_le(rts54_fields.cmd_get_pdo_pf.pdo_info, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_pdo_pf.pdo_source, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_pdo_pf.pdo_pp, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_pdo_pf.pdo_offset, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_pdo_pf.pdo_num, bytes(1, 1))

  set_response_handler(pktinfo, decode_resp_get_pdo)

  pktinfo.cols.info:set("Get PDO")
end

h[0x08][0x83] = decode_cmd_08_83

-- cmd 0x08 0x84 GET_RDO

local rts54xx_cmd_get_rdo_proto = Proto("rts54xx.cmd_get_rdo", "GRDO")
--rts54xx_cmd_get_rdo_proto.fields = rts54_fields.cmd_get_rdo_pf

local function decode_cmd_08_84(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  set_response_handler(pktinfo, decode_resp_get_rdo)

  pktinfo.cols.info:set("Get RDO")
end

h[0x08][0x84] = decode_cmd_08_84

-- cmd 0x08 0x9a GET_VDO

local rts54xx_cmd_get_vdo_proto = Proto("rts54xx.cmd_get_vdo", "GVDO")
rts54xx_cmd_get_vdo_proto.fields = rts54_fields.cmd_get_vdo_pf

local function decode_cmd_08_9a(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd08_pf.port_num, bytes(0, 1))

  local t2 = t:add_le(rts54_fields.cmd_get_vdo_pf.vdo_info, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_vdo_pf.vdo_num, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_vdo_pf.vdo_origin, bytes(1, 1))
  t2:add_le(rts54_fields.cmd_get_vdo_pf.vdo_reserved, bytes(1, 1))

  local resp_hints = {}

  local nvdo = lefield(bytes(1, 1), 0, 3)
  local i
  for i = 1, nvdo do
    --t:add(rts54_fields.cmds_pf.payload, bytes(2 + (i - 1), 1))

    type_num = bytes(2 + (i - 1), 1):uint()

    --t:add(rts54_fields.cmd_get_vdo_pf.vdo_type, bytes(2 + (i - 1), 1))
    t:add(rts54_fields.cmd_get_vdo_pf.vdo_type, type_num)

    resp_hints[i] = type_num

  end

  set_response_handler(pktinfo, decode_resp_get_vdo, resp_hints)

  pktinfo.cols.info:set("Get VDO")
end

h[0x08][0x9a] = decode_cmd_08_9a

--  table.unpack(rts45_req_08_header_action),

-- cmd 0x09 GET_RTK_STATUS

local rts54xx_cmd09_proto = Proto("rts54xx.cmd09", "CMD 09")
rts54xx_cmd09_proto.fields = rts54_fields.cmd09_pf

local function decode_get_rtk_status(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd09_pf.offset, bytes(0, 1))
  t:add_le(rts54_fields.cmd09_pf.port_num, bytes(1, 1))
  t:add_le(rts54_fields.cmd09_pf.sts_len, bytes(2, 1))

  set_response_handler(pktinfo, decode_resp_get_rtk_status)

  pktinfo.cols.info:set("Get RTK Status")
end

h[0x09] = {
  d = decode_get_rtk_status,
}

-- rts54xx cmd 0x0a ACK_CC_CI

local rts54xx_cmd0a_proto = Proto("rts54xx.cmd0a", "CMD 0a")
rts54xx_cmd0a_proto.fields = rts54_fields.cmd0a_pf

local function decode_ack_cc_ci(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0a_pf.reserved, bytes(0, 1))
  t:add_le(rts54_fields.cmd0a_pf.port_num, bytes(1, 1))
  t:add_le(rts54_fields.cmd0a_pf.data32, bytes(2, 4))
  t:add_le(rts54_fields.cmd0a_pf.command_complete, bytes(6, 1))

  pktinfo.cols.info:set("Acknowledge CC/CI")
end

h[0x0a] = {
  d = decode_ack_cc_ci,
}
