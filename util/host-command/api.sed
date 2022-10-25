
#
# sed -f api.sed util/ectool.cc
#

# join interesting lines, remove newlines
/.*ec_command(EC_CMD_[A-Z0-9_]\+,.*,$/{N;s/\n\s*/ /g}
#/.*ec_command(EC_CMD_[A-Z0-9_]\+,.*,$/{N;s/\n\s*/ /g}

#s@.*EC_CMD_GSV_.*@HHHH&HHHH@p

# has param and response
s@ec_command(EC_CMD_\([A-Z0-9_]\+\),\s*0,\s*&\([a-z]\+\),\s*sizeof(\2),\s&\([a-z]\+\), sizeof(\3)@\LEC_CMD_\1(\&\2, \&\3@
s@ec_command(EC_CMD_\([A-Z0-9_]\+\),\s*1,\s*\(&p\),\s*sizeof(p)\(,\s&r\), sizeof(r)@\LEC_CMD_\1_v1(\2\3@
#s@ec_command(EC_CMD_\([A-Z0-9_]\+\),\s*2,\s*\(&p\),\s*sizeof(p)\(,\s&r\), sizeof(r)@\LEC_CMD_\1_v2(\2\3@

# no param, has reply
s@ec_command(EC_CMD_\([A-Z0-9_]\+\),\s*0,\s*NULL,\s*0,\s*&\([a-z]\+\),\ssizeof(\2)@\LEC_CMD_\1(\&\2@

# has param, no reply
s@ec_command(EC_CMD_\([A-Z0-9_]\+\),\s*0,\s*&\([a-z]\+\),\ssizeof(\2),\s*NULL,\s*0@\LEC_CMD_\1(\&\2@

# has no param, no reply
s@ec_command(EC_CMD_\([A-Z0-9_]\+\),\s*0,\s*NULL,\s0,\s*NULL,\s*0@\LEC_CMD_\1(@
