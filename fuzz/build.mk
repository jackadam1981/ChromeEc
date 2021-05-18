# -*- makefile -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# fuzzer binaries
#

<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
fuzz-test-list-host = cr50_fuzz host_command_fuzz usb_pd_fuzz
=======
fuzz-test-list-host =
# Fuzzers should only be built for architectures that support sanitizers.
ifeq ($(ARCH),amd64)
fuzz-test-list-host += host_command_fuzz usb_pd_fuzz usb_tcpm_v2_rev20_fuzz \
	usb_tcpm_v2_rev30_fuzz
endif
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)

# For fuzzing targets libec.a is built from the ro objects and hides functions
# that collide with stdlib. The rw only objects are then linked against libec.a
# with stdlib support. Therefore fuzzing targets that need to call this internal
# functions should be marked "-y" or "-ro", and fuzzing targets that need stdlib
# should be marked "-rw". In other words:
#
# Does your object file need to link against the Cr50 implementations of stdlib
# functions?
#   Yes -> use <obj_name>-y
# Does your object file need to link against cstdlib?
#   Yes -> use <obj_name>-rw
# Otherwise use <obj_name>-y
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")
cr50_fuzz-rw = cr50_fuzz.o pinweaver_model.o mem_hash_tree.o
=======
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
host_command_fuzz-y = host_command_fuzz.o
usb_pd_fuzz-y = usb_pd_fuzz.o
<<<<<<< HEAD   (e924cf Revert "garg: Add simplo 916QA141H battery")

CR50_PROTO_HEADERS := $(out)/gen/fuzz/cr50_fuzz.pb.h \
  $(out)/gen/fuzz/pinweaver/pinweaver.pb.h
$(out)/RW/fuzz/pinweaver_model.o: ${CR50_PROTO_HEADERS}
$(out)/RW/fuzz/cr50_fuzz.o: ${CR50_PROTO_HEADERS}
$(out)/RW/fuzz/cr50_fuzz.o: CPPFLAGS+=${LIBPROTOBUF_MUTATOR_CFLAGS}

$(out)/cr50_fuzz.exe: $(out)/cryptoc/libcryptoc.a \
  $(out)/gen/fuzz/cr50_fuzz.pb.o \
  $(out)/gen/fuzz/pinweaver/pinweaver.pb.o \

$(out)/cr50_fuzz.exe: LDFLAGS_EXTRA+=-lcrypto ${LIBPROTOBUF_MUTATOR_LDLIBS}
=======
usb_tcpm_v2_rev30_fuzz-y = usb_pd_fuzz.o usb_tcpm_v2_rev30_fuzz.o \
	../test/fake_battery.o
usb_tcpm_v2_rev20_fuzz-y = usb_pd_fuzz.o usb_tcpm_v2_rev20_fuzz.o \
	../test/fake_battery.o
>>>>>>> BRANCH (d1db89 chgstv2: Check string validity)
