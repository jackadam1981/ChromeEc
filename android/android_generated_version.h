// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Setup a system to auto generate and commit this information whenever a new commit is made to gsc_utils
// ChromeOS uses util/getversion.sh to generate the header file at compile time.
// However getversion.sh generates its version string by looking at git history,
// and git usage at compile time is discouraged and or disabled in Android. So
// we're just stubbing this out for the time being.
#define CROS_EC_VERSION32 "UNKNOWN_EC_VERSION32"
#define CROS_ECTOOL_VERSION "UNKNOWN_ECTOOL_VERSION"
#define CROS_STM32MON_VERSION "UNKNOWN_STM32MON_VERSION"
#define VERSION "UNKNOWN_VERSION"
#define BUILDER "UNKNOWN_USER@UNKNOWN_HOSTNAME"
#define DATE "UNKNOWN_DATE"
