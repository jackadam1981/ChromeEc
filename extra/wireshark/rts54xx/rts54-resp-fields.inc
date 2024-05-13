-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

local vdo_idh_pf = {
  data32 = ProtoField.uint32("rts.vdo_idh_data", "VDO (ID Header)", base.HEX),

  ProtoField.uint32("rts.vdo_idh_vid", "USB VID",                       base.HEX, nil, 0x0000ffff),
  ProtoField.uint32("rts.vdo_idh_res16", "res16",                       base.HEX, nil, 0x001f0000),
  ProtoField.uint32("rts.vdo_idh_ct", "Conn Type",                      base.DEC, nil, 0x00600000),
  ProtoField.uint32("rts.vdo_idh_pt_d", "Prod Type D",                  base.DEC, nil, 0x03800000),
  ProtoField.bool("rts.vdo_idh_modal", "modal",                               32, nil, 0x04000000),
  ProtoField.uint32("rts.vdo_idh_pt_u", "Prod Type U",                  base.DEC, nil, 0x38000000),
  ProtoField.bool("rts.vdo_idh_usb_comm", "usb comm D",                       32, nil, 0x40000000),
  ProtoField.bool("rts.vdo_idh_usb_comm", "usb comm H",                       32, nil, 0x80000000),
}

Proto("rts54xx.vdo_idh", "VDO_IDH").fields = vdo_idh_pf

local vdo_pt_pf = {
  data32 = ProtoField.uint32("rts.vdo_pt_data", "VDO (Product)", base.HEX),

  ProtoField.uint32("rts.vdo_pt_device",  "Device",                      base.HEX, nil, 0x0000ffff),
  ProtoField.uint32("rts.vdo_pt_product", "Product",                     base.HEX, nil, 0xffff0000),
}

Proto("rts54xx.vdo_pt", "VDO_PT").fields = vdo_pt_pf

rts_vdo_n_pf = {
 [1] = vdo_idh_pf,
 [3] = vdo_pt_pf,
}
