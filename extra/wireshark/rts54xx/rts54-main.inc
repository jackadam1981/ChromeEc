-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

local RTS54_LINK_RX = 0
local RTS54_LINK_TX = 1

local rts54_encap_proto = Proto("rts54xx-encap", "RTS54XX-encap")

-- rts54xx cmd 0x12

local rts54xx_cmd12_proto = Proto("rts54xx.cmd12", "CMD 12")
rts54xx_cmd12_proto.fields = rts54_fields.cmd12_pf

-- rts54xx cmd 0x20 0x00 SET_RETIMER_FW_UPDATE_MODE

local rts54xx_cmd20_proto = Proto("rts54xx.cmd20", "CMD 20")
rts54xx_cmd20_proto.fields = rts54_fields.cmd20_pf
--local rts54xx_cmd_set_pdo_proto = Proto("rts54xx.cmd_set_pdo", "SPDO")
--rts54xx_cmd_sne_proto.fields = rts54_fields.cmd_set_pdo_pf

local function decode_cmd_20_00(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd20_pf.port_num, bytes(0, 1))
  t:add_le(rts54_fields.cmd20_pf.fw_update_status, bytes(1, 1))

--  local t2 = t:add_le(rts54_fields.cmd_set_pdo_pf.pdo_info, bytes(1, 1))
--  t2:add_le(rts54_fields.cmd_set_pdo_pf.spr_num, bytes(1, 1))
--  t2:add_le(rts54_fields.cmd_set_pdo_pf.pdo_type, bytes(1, 1))
--  t2:add_le(rts54_fields.cmd_set_pdo_pf.epr_num, bytes(1, 1))
--  t2:add_le(rts54_fields.cmd_set_pdo_pf.reserved, bytes(1, 1))

  pktinfo.cols.info:set("Set Retimer FW Update Mode")
end

-- rts54xx cmd 0x3a

local rts54xx_cmd3a_proto = Proto("rts54xx.cmd3a", "CMD 3a")
rts54xx_cmd3a_proto.fields = rts54_fields.cmd3a_pf

local function decode_get_ic_status(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd3a_pf.offset, bytes(0, 1))
  t:add_le(rts54_fields.cmd3a_pf.reserved, bytes(1, 1))
  t:add_le(rts54_fields.cmd3a_pf.data_len, bytes(2, 1))

  set_response_handler(pktinfo, decode_resp_get_ic_status)

  pktinfo.cols.info:set("Get IC Status")
end


local function Xdecode_get_rtk_status(t, bytes, pktinfo)
  t:add_le(rts54_fields.cmd09_pf.offset, bytes(0, 1))
  t:add_le(rts54_fields.cmd09_pf.port_num, bytes(1, 1))
  t:add_le(rts54_fields.cmd09_pf.sts_len, bytes(2, 1))

  set_response_handler(pktinfo, decode_resp_get_rtk_status)

  pktinfo.cols.info:set("Get RTK Status")
end


local h = rts45_req_header_action

h[0x12] = {
  pf  = rts54_fields.cmd12_pf.sub_cmd,
}
h[0x20] = {
  pf  = rts54_fields.cmd20_pf.sub_cmd,
  [0x00] = decode_cmd_20_00,
}
h[0x3a] = {
  d = decode_get_ic_status,
}


local rt45_header_action = h

local function decode_cmd(tree, cmd_msg, pktinfo)

  local subtree = tree:add(rts54_proto, cmd_msg(0, nil))
  subtree:add(rts54_fields.cmds_pf.cmd, cmd_msg(0, 1))
  subtree:add(rts54_fields.cmds_pf.wr_len, cmd_msg(1, 1))

  local cmd1 = cmd_msg(0, 1):uint()
  if (not rt45_header_action[cmd1]) then
    local pl_len = cmd_msg:len() - 2
    local payload = cmd_msg(2, pl_len);
    subtree:add(rts54_fields.cmds_pf.payload, payload)
    return
  end

  local e = rt45_header_action[cmd1]

  if (e.d) then
    local pl_len = cmd_msg:len() - 2
    local payload = cmd_msg(2, pl_len);
    e.d(subtree, payload, pktinfo)
    return
  end

  local pf = e.pf
  local cmd2 = cmd_msg(2, 1):uint()
  local d = e[cmd2]
  if (pf) then
    subtree:add(pf, cmd_msg(2, 1))
  end
  local pl_len = cmd_msg:len() - 3
  local payload = cmd_msg(3, pl_len);
  local rest = 0
  if (d) then
    rest = d(subtree, payload, pktinfo)
  end
  if (rest and rest < pl_len) then
    subtree:add(rts54_fields.cmds_pf.payload, payload(rest, pl_len - rest))
  end

end

function rts54_encap_proto.dissector(buf, pktinfo, tree)

  pktinfo.cols.protocol:set("UCSI-RTS54xx")

  tree:add_le(rts54_fields.cmds_pf.seq_num, buf(0, 2))
  tree:add(rts54_fields.cmds_pf.port_num, buf(2, 1))
  tree:add(rts54_fields.cmds_pf.direction, buf(3, 1))
  tree:add(rts54_fields.cmds_pf.chip_type, buf(4, 1))

  local cmd_msg = buf(5, nil)
  if buf(3, 1):uint() == RTS54_LINK_RX then
    local handler

    pktinfo.cols.src = "PDC"
    pktinfo.cols.dst = "EC"

    handler = find_recent_handler(pktinfo)
    if handler then
      local fn
      local h

      fn = handler[0]
      h = handler[1]

      fn(tree, cmd_msg, pktinfo, h)
    else
      decode_resp(tree, cmd_msg, pktinfo)
    end
  else

    pktinfo.cols.src = "EC"
    pktinfo.cols.dst = "PDC"

    decode_cmd(tree, cmd_msg, pktinfo)
  end

end

local wtap_encap_table = DissectorTable.get("wtap_encap")
wtap_encap_table:add(wtap.USER0, rts54_encap_proto)

-- this is used for the live packet capture case where
-- messages are sent to UDP port 2012
local pdc_udp_table = DissectorTable.get("udp.port"):add(2012, rts54_encap_proto)


-- Local Variables:
-- indent-tabs-mode: nil
-- End:
