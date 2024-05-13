-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

local h = rts45_req_header_action

--h[0x08] = {
--  pf  = rts54_fields.cmd0e_pf.sub_cmd,
--}

-- rts54xx cmd 0x0e

local rts54xx_cmd0e_proto = Proto("rts54xx.cmd0e", "CMD 0e")
rts54xx_cmd0e_proto.fields = rts54_fields.cmd0e_pf

-- rts54xx cmd 0x0e 0x01

local function decode_ppm_reset(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))

  pktinfo.cols.info:set("PPM Reset")
end

-- rts54xx cmd 0x0e 0x03

local function decode_connector_reset(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))

  local t2 = t:add(rts54_fields.cmds_pf.payload, bytes(1, 1))
  t2:add_le(rts54_fields.cmd0e_pf.cr_connector_number, bytes(1, 1))
  t2:add_le(rts54_fields.cmd0e_pf.cr_reset_type, bytes(1, 1))

  pktinfo.cols.info:set("Connector Reset")
end

-- rts54xx cmd 0x0e 0x06

local function decode_get_capability(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))

  set_response_handler(pktinfo, decode_resp_get_capability)

  pktinfo.cols.info:set("Get Capabilities")
end

-- rts54xx cmd 0x0e 0x07

local function decode_get_connector_capability(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))

  set_response_handler(pktinfo, decode_resp_get_connector_capability)

  pktinfo.cols.info:set("Get Connector Capability")
end

-- rts54xx cmd 0x0e 0x08

local function decode_set_ccom(t, bytes, pktinfo)
  t:add(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))
  local t2 = t:add(rts54_fields.cmds_pf.payload, bytes(1, 2))
  t2:add_le(rts54_fields.cmd0e_pf.sccom_cc_operation_mode_rp, bytes(1, 2))
  t2:add_le(rts54_fields.cmd0e_pf.sccom_cc_operation_mode_rd, bytes(1, 2))
  t2:add_le(rts54_fields.cmd0e_pf.sccom_cc_operation_mode_drp, bytes(1, 2))
  t2:add_le(rts54_fields.cmd0e_pf.sccom_cc_operation_mode_z, bytes(1, 2))

  pktinfo.cols.info:set("Set CCOM")
end

-- rts54xx cmd 0x0e 0x09

local function decode_set_uor(t, bytes, pktinfo)
  t:add(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))
  t:add(rts54_fields.cmds_pf.payload, bytes(1, 2))

  --set_response_handler(pktinfo, decode_resp_set_uor)

  pktinfo.cols.info:set("Set UOR")
end

-- rts54xx cmd 0x0e 0x0b

local function decode_set_pdr(t, bytes, pktinfo)
  t:add(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))
  t:add(rts54_fields.cmds_pf.payload, bytes(1, 2))

  --set_response_handler(pktinfo, decode_resp_set_pdr)

  pktinfo.cols.info:set("Set PDR")
end

-- rts54xx cmd 0x0e 0x11

local function decode_get_cable_property(t, bytes, pktinfo)
  t:add(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))
  t:add(rts54_fields.cmd0e_pf.reserved, bytes(1, 1))

  -- pcap file has response length 9 instead of 6
  set_response_handler(pktinfo, decode_resp_get_cable_property)

  pktinfo.cols.info:set("Get Cable Property")
end

-- rts54xx cmd 0x0e 0x12

local function decode_ucsi_get_connector_status(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))
  t:add_le(rts54_fields.cmd0e_pf.reserved, bytes(1, 1))

  set_response_handler(pktinfo, decode_resp_ucsi_get_connector_status)

  pktinfo.cols.info:set("UCSI Get Connector Status")
end

-- rts54xx cmd 0x0e 0x13

local function decode_ucsi_get_error_status(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))

  set_response_handler(pktinfo, decode_resp_ucsi_get_error_status)

  pktinfo.cols.info:set("UCSI Get Error Status")
end

-- rts54xx cmd 0x0e 0x1e

local function decode_ucsi_read_power_level(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd0e_pf.pr_data_length, bytes(0, 1))

  --set_response_handler(pktinfo, decode_resp_ucsi_read_power_level)

  pktinfo.cols.info:set("UCSI Read Power Level")
end

h[0x0e] = {
  pf  = rts54_fields.cmd0e_pf.sub_cmd,
  [0x01] = decode_ppm_reset,
  [0x03] = decode_connector_reset,
  [0x06] = decode_get_capability,
  [0x07] = decode_get_connector_capability,
  [0x08] = decode_set_ccom,
  [0x09] = decode_set_uor,
  [0x0b] = decode_set_pdr,
  [0x11] = decode_get_cable_property,
  [0x12] = decode_ucsi_get_connector_status,
  [0x13] = decode_ucsi_get_error_status,
  [0x1e] = decode_ucsi_read_power_level,
}
