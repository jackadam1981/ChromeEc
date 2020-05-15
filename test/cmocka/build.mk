# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

cmocka_srcs := test/cmocka
cmocka_out := $(out)/cmocka

# Register all tests
tests:=

# Include common mocks
common_mocks = $(wildcard $(cmocka_srcs)/mocks/*)
$(foreach test, $(tests), \
	$(eval $(test)-srcs += $(common_mocks)))

CMOCKA_CFLAGS += -DCMOCKA_TEST
CMOCKA_CFLAGS += -I$(cmocka_srcs)/include
CMOCKA_LDFLAGS += -lcmocka

# This is necessary for elegant creation of per-test targets
.SECONDEXPANSION:

# Create actual targets for unit test binaries
# $1 - test name
define CC_template
$($(1)-objs): CMOCKA_CFLAGS +=-DTEST_$(1)=$(EMPTY)
	CMOCKA_CFLAGS +=-DTEST_$$(shell echo $(1) | tr '[:lower:]' '[:upper:]')
$($(1)-objs): out = $(cmocka_out)/$(1)
$($(1)-objs): $(cmocka_out)/$(1)/%.o: $$$$*.c
	@mkdir -p $$(dir $$@)
	$(Q)$$(call quiet,cmocka_c_to_obj,CC    )

$($(1)-bin): out = $(cmocka_out)
$($(1)-bin): $($(1)-objs)
	$(Q)$$(call quiet,cmocka_exe,EXE   )
endef

$(foreach test, $(tests), \
	$(eval $(test)-objs:=$(addprefix $(cmocka_out)/$(test)/, \
		$(patsubst %.c,%.o,$($(test)-srcs)))))
$(foreach test, $(tests), \
	$(eval $(test)-bin:=$(cmocka_out)/$(test)/exe))
$(foreach test, $(tests), \
	$(eval $(call CC_template,$(test))))

$(foreach test, $(tests), \
	$(eval all-test-objs+=$($(test)-objs)))

test_deps := $(addsuffix .d,$(basename $(all-test-objs)))
-include $(test_deps)

cmocka-test-targets = $(foreach test, $(tests), cmocka-$(test))
cmocka-run-test-targets = $(foreach test, $(tests), cmocka-run-$(test))

.PHONY: $(cmocka-test-targets) $(cmocka-run-test-targets)
.PHONY: $(addprefix cmocka-clean-,$(tests))
.PHONY: cmocka-unit-tests cmocka-build-unit-tests cmocka-run-unit-tests
.PHONY: cmocka-clean-unit-tests

$(cmocka-test-targets): cmocka-%: $$($$*-bin)

$(cmocka-run-test-targets): cmocka-run-%: $$($$*-bin)
	@./$^

cmocka-unit-tests: cmocka-build-unit-tests cmocka-run-unit-tests

cmocka-build-unit-tests: $(cmocka-test-targets)

cmocka-run-unit-tests: $(cmocka-run-test-targets)
	@echo "**********************"
	@echo "   ALL TESTS PASSED"
	@echo "**********************"

$(addprefix cmocka-clean-,$(tests)): cmocka-clean-%:
	rm -rf $(cmocka_out)/$*

cmocka-clean-unit-tests:
	rm -rf $(cmocka_out)
