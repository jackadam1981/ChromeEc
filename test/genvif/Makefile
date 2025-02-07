# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

CC=gcc
CFLAGS=-O2 -Isrc -I../../include
CFLAGS+= -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

genvif: ../../util/genvif.c src/helper.c
	@$(CC) -o $@ $^ $(CFLAGS)

outdir := vif_output

$(outdir):
	mkdir vif_output

$(outdir)/test_vif.xml: vif/$(notdir $@) genvif | $(outdir)
	./genvif -b test -o $(outdir)

$(outdir)/exp_test_vif.xml: vif/$(notdir $@) genvif | $(outdir)
	./genvif -b exp_test -o $(outdir) -n -v vif/$(notdir $@)

$(outdir)/over_test_vif.xml: vif/$(notdir $@) genvif | $(outdir)
	./genvif -b test_over -o $(outdir) -v vif/$(notdir $@)

$(outdir)/exp_test_over_vif.xml: vif/$(notdir $@) genvif | $(outdir)
	./genvif -b exp_test_over -o $(outdir) -n -v vif/$(notdir $@)

.PHONY: clean test test_over

test: genvif $(outdir)/test_vif.xml $(outdir)/exp_test_vif.xml
	diff $(outdir)/test_over_vif.xml $(outdir)/exp_test_over_vif.xml

test_over: genvif $(outdir)/over_test_vif.xml $(outdir)/exp_test_over_vif.xml
	diff $(outdir)/test_over_vif.xml $(outdir)/exp_test_over_vif.xml

clean:
	rm -f genvif
	rm -rf vif_output
