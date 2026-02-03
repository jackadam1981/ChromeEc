This document contains public agent context for EC development.

# Overview

This repository contains the Zephyr EC (embedded controller) firmware for Chromium OS devices. The EC handles low-level tasks such as power sequencing, battery charging, keyboard control, thermal management, power delivery, etc.

There are two distinct EC implementations:
1.  **Legacy EC**: aka CrOS EC. A custom, lightweight OS used on older Chromebook designs. Legacy EC development has moved to the [ec-legacy branch](https://chromium.googlesource.com/chromiumos/platform/ec-legacy) located at ../ec-legacy. The common shared code has been forked from this repo and will likely diverge over time. Legacy only code in this repo will eventually be removed.
2.  **Zephyr EC**: (this repo) A newer EC implementation based on the [Zephyr Project RTOS](https://zephyrproject.org/), used in newer Chromebook designs. The downstream [Zephyr RTOS](https://chromium.googlesource.com/chromiumos/third_party/zephyrproject) is located at ../../third_party/zephyrproject/zephyr. Prefer using Zephyr APIs and drivers over custom implementations. See @docs/zephyr/README.md for more information.

## Directory Structure

*   **`common/`**: High-level code shared between boards and both EC implementations.
*   **`driver/`**: Low-level drivers for on-board peripherals shared between implementations.
*   **`include/`**: Header files for common and driver code.
*   **`zephyr/`**: Root directory for the Zephyr EC application.
    *   `zephyr/program/`: Project-specific configurations (replaces `board/` in Zephyr).
    *   `zephyr/shim/`: Compatibility layer adapting Legacy EC APIs to Zephyr.
    *   `zephyr/app/`: Zephyr EC application entry point.
    *   `zephyr/test/`: Unit tests for the Zephyr EC.
*   **`board/`** (Legacy): Board-specific code and configuration for legacy devices.
*   **`chip/`** (Legacy): Chip-specific code for interfacing with hardware blocks.
*   **`core/`** (Legacy): Core OS functionality (task scheduling, memory management).
*   **`test/`** (Legacy): Unit tests for the legacy EC.
*   **`docs/`**: Documentation for both EC implementations.
*   **`../../third_party/zephyrproject/zephyr/`**: Zephyr RTOS source code.
*   **`../ec-private`**: Internal private code for EC development.
*   **`build/`**: Build output directory. Exclude this directory when searching or grepping.

## Development

* Repo is used to manage multiple git repositories. It must be run outside the chroot.
  * `repo sync .` to sync this repo
  * `repo start <branch-name> .` to start a new working branch.
  * `repo upload . --cbr` to upload the current branch to gerrit.
* Code Review:
  * Gerrit is used for code review.
  * The gerrit cli is `gerrit`.
  * See @docs/code_review.md for more information.
* Coding style is defined in @README.md
  * Use `cros format <file>` to format files.
* Commit messages:
  * Must include `BUG=b:<id>` and `TEST=<description>`.
  * A gerrit Change-Id is required, and will be automatically added by `git commit`.
  * Preserve the original change-id when cherry-picking.
* New Boards: See @docs/zephyr/zephyr_new_board_checklist.md (Zephyr) and @docs/new_board_checklist.md (Legacy).
* See @docs/ for much more information.

## Building and Running

### Build Environment

The chroot environment is used for building and testing.
The `cros_sdk <cmd>` command is used to execute <cmd> in the chroot environment.
Use `cros_sdk --working-dir <dir> <cmd>` to set the working directory to <dir>.

### Building Zephyr EC

For detailed instructions, see @docs/zephyr/zephyr_build.md

The Zephyr EC uses the `zmake` tool for building.

*   **Build a project**:
    ```cros_sdk --working-dir .
    zmake build <project_name>
    ```
    *   Example: `cros_sdk --working-dir . zmake build skyrim`
    *   Output: `build/zephyr/<project_name>/output/ec.bin`
### Legacy EC

For detailed instructions, see @docs/getting_started_quickly.md.

Use `make` to build legacy boards.

```bash
# Enter chroot
cros_sdk --working-dir .

# Build a specific board (e.g., elm)
make BOARD=elm -j

# Output location
# build/elm/ec.bin
```

## Testing

### Unit Tests (Legacy)

For detailed instructions, see @docs/unit_tests.md

*   **Build all unit tests**:
    ```cros_sdk --working-dir .
    make buildall -j
    ```
*   **Run all unit tests**:
    ```cros_sdk --working-dir .
    make runtests -j
    ```
*   **Build/Run a specific test**:
    ```cros_sdk --working-dir .
    make run-<test_name>
    ```

### Testing (Zephyr)

For detailed instructions, see @docs/zephyr/ztest.md

Zephyr EC tests are located in `zephyr/test/`. Use the `./twister` wrapper script (inside chroot).

*   **Run all tests in a directory**:
    ```bash
    cros_sdk --working-dir . ./twister -T zephyr/test/drivers/
    ```
*   **Run a specific test suite**:
    ```bash
    cros_sdk --working-dir . ./twister -T zephyr/test/drivers/ -s drivers.default
    ```
    *   Test suites are defined in `testcase.yaml` files within the test directories.
