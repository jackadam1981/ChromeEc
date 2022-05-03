#!/bin/bash

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#. /usr/share/misc/shflags

#DEFINE_string

color=
keys=()

print_help() {
  echo "rgbtool - A utility tool for RGB keyboard"
  echo
  echo "1. rgbtool clear [color]"
  echo "2. rgbtool reset"
  echo "3. rgbtool rainbow"
  echo "4. rgbtool rainbow2"
  echo "5. rgbtool update [/path/to/image.bin]"
  echo
}

#ectool --device 18d1:5022 rgbkbd 26 0x1937d2

set_colors() {
  for key in "${keys[@]}"; do
    ectool --device 18d1:5022 rgbkbd "${key}" "${color}"
  done
}

toggle_reset_pin() {
  echo 0 > gpio154/value && sleep 1 && echo 1 > gpio154/value
}

reset() {
  stay_in_ro="${1:-0}"
  pushd /sys/class/gpio > /dev/null || exit

  if [ ! -d gpio154 ]; then
    echo 154 > ./export
  fi

  if [ "${stay_in_ro}" -eq 1 ]; then
    toggle_reset_pin && sleep 1 && usb_updater2 --stay_in_ro
  else
    toggle_reset_pin
  fi

  echo 154 > unexport
  popd > /dev/null || exit
}

update() {
  reset 1
  usb_updater2 "$1"
  reset
}

clear() {
  ectool --device 18d1:5022 rgbkbd clear "$1"
}

rainbow() {
  # Pink
  color=0xc5221f
  keys=(110 1 2 16 17 30 44 58 111)
  set_colors

  # Orange
  color=0xec6a08
  keys=(112 113 3 4 18 19 31 32 33 46 47 60)
  set_colors

  # Green
  color=0x33801c
  keys=(114 115 116 5 6 7 20 21 22 34 35 48 49 50 61)
  set_colors

  # Turquoise
  color=0x20b189
  keys=(117 118 119 8 9 10 23 24 36 37 38 51 52 53 61 62)
  set_colors

  # Blue
  color=0x1937d2
  keys=(120 121 122 11 12 25 26 27 39 40 41 54 55 64 79)
  set_colors

  # Purple
  color=0x8420b4
  keys=(123 124 125 13 28 29 43 57 83 84 89 15 59)
  set_colors
}

rainbow_reverse() {
  # Purple
  color=0x8420b4
  keys=(123 124 125 13 28 29 43 57 83 84 89 15 59)
  set_colors

  # Blue
  color=0x1937d2
  keys=(120 121 122 11 12 25 26 27 39 40 41 54 55 64 79)
  set_colors

  # Turquoise
  color=0x20b189
  keys=(117 118 119 8 9 10 23 24 36 37 38 51 52 53 61 62)
  set_colors

  # Green
  color=0x33801c
  keys=(114 115 116 5 6 7 20 21 22 34 35 48 49 50 61)
  set_colors

  # Orange
  color=0xec6a08
  keys=(112 113 3 4 18 19 31 32 33 46 47 60)
  set_colors

  # Pink
  color=0xc5221f
  keys=(110 1 2 16 17 30 44 58 111)
  set_colors
}

rainbow2() {
  base="$1"
  rainbow

  color="${base}"
  keys=(110 1 2 16 17 30 44 58 111)
  set_colors
  keys=(112 113 3 4 18 19 31 32 33 46 47 60)
  set_colors
  keys=(114 115 116 5 6 7 20 21 22 34 35 48 49 50 61)
  set_colors
  keys=(117 118 119 8 9 10 23 24 36 37 38 51 52 53 61 62)
  set_colors
  keys=(120 121 122 11 12 25 26 27 39 40 41 54 55 64 79)
  set_colors
  keys=(123 124 125 13 28 29 43 57 83 84 89 15 59)
  set_colors

  rainbow_reverse
}

if [ "$#" -eq 0 ]; then
  print_help
  exit
fi

echo "Running $1"

if [ "$1" == "rainbow" ]; then
  rainbow
elif [ "$1" == "rainbow2" ]; then
  color="${2:-0x202050}"
  clear "${color}"
  rainbow2 "${color}"
elif [ "$1" == "clear" ]; then
  clear "${2:-0}"
elif [ "$1" == "reset" ]; then
  reset
elif [ "$1" == "update" ]; then
  update "${2:-/tmp/kbmcu.bin}"
fi
