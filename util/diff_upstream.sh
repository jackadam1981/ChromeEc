#!/bin/bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SRC_DIR="$(realpath "$( dirname "${BASH_SOURCE[0]}" )/../../..")"
ZEPHYR_DIR="${SRC_DIR}/third_party/zephyr"
echo "Zephyr root directory: ${ZEPHYR_DIR}"

# This should include all the third_party modules that zmake can see.
# And also any repos that copybot copies from indirectly, i.e.
# zephyrproject-rtos/cmsis -> zephyr/cmsis -> zephyrproject/modules/hal/cmsis
declare -A zephyr_repos=(
  # config/chre/main-public.ini
  ['android/platform/system/chre']='https://android.googlesource.com/platform/system/chre main'
  # config/chre/main.ini
  ['android/platform/system/chre_internal']='https://chrome-internal.googlesource.com/chromeos/third_party/chre upstream/main'
  # config/pigweed/main.ini
  ['pigweed']='https://pigweed.googlesource.com/pigweed/pigweed main'
  # config/zephyr/main.ini
  ['zephyrproject/zephyr']='https://github.com/zephyrproject-rtos/zephyr.git main'
  # config/zephyr/project-cmsis.ini
  ['zephyrproject/modules/hal/cmsis']='https://github.com/zephyrproject-rtos/cmsis.git master'
  # config/zephyr/project-cmsis_6.ini
  ['zephyrproject/modules/hal/cmsis_6']='https://github.com/zephyrproject-rtos/CMSIS_6.git main'
  # config/zephyr/project-egis_module.ini
  ['zephyrproject/modules/hal/egis_module']='https://github.com/EgisMCU/egis_module.git main'
  # config/zephyr/project-hal_egis.ini
  ['zephyrproject/modules/hal/egis']='https://github.com/EgisMCU/hal_egis.git main'
  # config/zephyr/project-intel.ini
  ['zephyrproject/modules/hal/intel']='https://github.com/zephyrproject-rtos/hal_intel.git main'
  # config/zephyr/project-stm32.ini
  ['zephyrproject/modules/hal/stm32']='https://github.com/zephyrproject-rtos/hal_stm32.git main'
  # config/zephyr/project-chre.ini
  ['zephyrproject/modules/lib/chre']='https://github.com/zephyrproject-rtos/chre.git zephyr'
  # config/zephyr/project-nanopb.ini
  ['zephyrproject/modules/lib/nanopb']='https://github.com/zephyrproject-rtos/nanopb.git zephyr'
  # config/zephyr/project-picolibc.ini
  ['zephyrproject/modules/lib/picolibc']='https://github.com/zephyrproject-rtos/picolibc.git main'
)

function die() {
  echo "$@"
  exit 1
}

for repo in "${!zephyr_repos[@]}"; do
  read -ra upstream <<<"${zephyr_repos[${repo}]}"
  upstream_repo="${upstream[0]}"
  upstream_branch="${upstream[1]}"

  cd "${ZEPHYR_DIR}/${repo}" || die "${ZEPHYR_DIR}/${repo} not found"
  repo start nodiffs . 2>/dev/null || die "repo start failed"
  git pull || die "git pull failed"
  upstream_commit="$(git log | sed -e '/^\s*GitOrigin-RevId:/!d' \
    -e 's/.*: //' -e 's/)$//' | head -1)"
  if [ "${upstream_commit}" == "" ]; then
    die "Could not find commit id to compare with"
  fi
  case "${upstream_commit}" in
    # cmsis has some commits out of order
    c3bd2094f92d574377f7af2aec147ae181aa5f8e)
      upstream_commit=4b96cbb174678dcd3ca86e11e1f24bc5f8726da0
      ;;
    # nanopb has some commits out of order
    0aa6f11bc7563989da85774a0decaecd3b304d6a)
      upstream_commit=65cbefb
      ;;
    # picolibc has a commit out of order
    b25f4a47784d2c24695977c903fe114565ae2bc6)
      upstream_commit=1c73900b79dbc02b80d09f5d637382249158e1ec
      ;;
  esac
  echo "==============================="
  echo "Diffing ${ZEPHYR_DIR}/${repo} vs ${upstream_repo}@${upstream_branch}"
  git remote rm upstream >/dev/null
  git remote add -f upstream -t "${upstream_branch}" "${upstream_repo}" \
    >/dev/null 2>/dev/null || die "Failed to add upstream remote"

  echo "Starting diff at ${upstream_commit}"
  echo "Copybot missed commits:"
  git --no-pager log --no-decorate --format='%h %s %cr' \
    upstream/"${upstream_branch}" ^"${upstream_commit}" \
    || die "git log failed"
  echo "---------"

  git --no-pager diff "${upstream_commit}" ':(exclude).vpython3' \
    ':(exclude)DIR_METADATA' ':(exclude)OWNERS' ':(exclude)PRESUBMIT.cfg' \
    || die "git diff failed"
done

exit 0
