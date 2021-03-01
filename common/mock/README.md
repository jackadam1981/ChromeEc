# Common Mocks
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

This directory holds mock implementations for use in fuzzers and tests.

Each mock is given some friendly build name, like ROLLBACK or FP_SENSOR.
This name is defined in [common/mock/build.mk](build.mk) and referenced
from unit tests and fuzzers' `.mocklist` file.

## Creating a new mock
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***


* Add the mock source to [common/mock](/common/mock) and the
  optional header file to [include/mock](/include/mock).
  Header files are only necessary if you want to expose additional
  [mock control](#mock-controls) functions/variables.
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

  See the [Design Patterns](#design-patterns) section
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

  for more detail on design patterns.
* Add a new entry in [common/mock/build.mk](build.mk)
  that is conditioned on your mock's name.

If a unit test or fuzzer requests this mock, the build system will
set the variable `HAS_MOCK_<BUILD_NAME>` to `y` at build time.
This variable is used to conditionally include the mock source
in [common/mock/build.mk](build.mk).

Example line from [common/mock/build.mk](build.mk):

```make
# Mocks
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

mock-$(HAS_MOCK_ROLLBACK) += mock/rollback_mock.o
```

## Using a mock
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

Unit tests and fuzzers can request a particular mock by adding an entry to
their `.mocklist` file. The mocklist file is similar to a `.tasklist`
file, where it is named according to the test/fuzz's name followed by
`.mocklist`, like `fpsensor.mocklist`.
The mocklist file is optional, so you may need to create one.

Example `.mocklist`:

```c
/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

 #define CONFIG_TEST_MOCK_LIST \
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

	MOCK(ROLLBACK)         \
	MOCK(FP_SENSOR)
```

If you need additional [mock control](#mock-controls) functionality,
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

you may need to include the mock's header file, which is prepended
with `mock/` in the include line.

For example, to control the return values of the rollback mock:

```c
#include "mock/rollback_mock.h"
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***


void yourfunction() {
	mock_ctrl_rollback.get_secret_fail = true;
}
```

## Mock Controls
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

Mocks can change their behavior by exposing "mock controls".

We do this, most commonly, by exposing an additional global struct
per mock that acts as the settings for the mock implementation.
The mock user can then modify fields of the struct to change the mock's behavior.
For example, the `fp_sensor_init_return` field may control what value
the mocked `fp_sensor_init` function returns.

The declaration for these controls are specified in the mock's header file,
which resides in [include/mock](/include/mock).

## Design Patterns
*** note
**Warning: This document is old & has moved.  Please update any links:**<br>
https://chromium.googlesource.com/chromiumos/platform/ec/+/HEAD/common/mock/README.md
***

* When creating mock controls, consider placing all your mock parameters in
  one externally facing struct, like in
  [fp_sensor_mock.h](/include/mock/fp_sensor_mock.h).
  The primary reason for this is to allow the mock to be easily used
  by a fuzzer (write random bytes into the struct with memcpy).
* When following the above pattern, please provide a macro for resetting
  default values for this struct, like in
  [fp_sensor_mock.h](/include/mock/fp_sensor_mock.h).
  This allows unit tests to quickly reset the mock state/parameters
  before each unrelated unit test.
