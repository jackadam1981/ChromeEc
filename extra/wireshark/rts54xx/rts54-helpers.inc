-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

function pf_auto_bits32(fields)
  mask = 0x00000001
  tb = {}

  for _, v in ipairs(fields) do
    table.insert(tb,
      ProtoField.bool("sne."..v, v, 32, nil, mask))
    mask = mask * 2
  end
  return tb
end

-- extract bitfield value

function lefield(bytes, start, len)
  local le_val = bytes:le_uint()
  le_val = bit.rshift(le_val, start)
  local mask = bit.lshift(1, len) - 1
  return bit.band(le_val, mask)
end

-- add subfields to a tree node
--  pf must be an ordered list

function tree_add_fields32(t, pf, data32)
  local t2 = t:add_le(pf.data32, data32)

  for _, v in ipairs(pf) do
    t2:add_le(v, data32)
  end
end

function t_add_list(t, fields, bytes4)
  for _, v in ipairs(fields) do
    t:add_le(v, bytes4)
  end
end


--
-- loaded files do not have access to variables defined in their parent,
-- so define as globals here
--

rts45_header_action = { }
rts45_req_header_action = { }
rts45_fields = { }
