-- Copyright 2024 The ChromiumOS Authors
-- Use of this source code is governed by a BSD-style license that can be
-- found in the LICENSE file.

--
-- implementation notes:
--
-- this plugin should work in 2 use cases
--
-- 1. explicit invocation using "-X lua_script:rts54.lua"
-- 2. placed in the *shark plugins directory
--
-- *.lua files in the plugins directory are run at *tshark startup
-- but they are also run (a 2nd time) if found by the "require"
-- statement.
-- the workaround is to rename such files *.inc and load them
-- using "dofile" to avoid duplicate execution.
--

--
-- variable scoping rules (at least observed behavior) are strange:
--
-- globals defined here are not reachable from *.inc
-- globals defined in *.inc are reachable from *.inc
-- globals defined in *.inc are reachable here
--

dofile("rts54-helpers.inc")

dofile("rts54-req-fields.inc")
dofile("rts54-requests.inc")

dofile("rts54-resp-fields.inc")
dofile("rts54-responses.inc")

dofile("rts54-cmd-0e.inc")

dofile("rts54-main.inc")
