#!/bin/bash
# Craig Hesling

OUTPUT_DIR=${1:-board-regs}

echo "# Scanning STM32 boards"
BOARDS=( )
for board in board/*/build.mk; do
  if grep stm32 $board >/dev/null; then
    BOARDS+=( $(dirname $board) )
  fi
done

echo "Boards: ${BOARDS[@]}"

echo "# Extracting constants"
mkdir -p $OUTPUT_DIR
declare -ux CHIP CHIP_VARIANT CHIP_FAMILY BOARD
for board in "${BOARDS[@]}"; do
  eval `grep stm32 $board/build.mk | tr -d ' ' | tr -d ':'`
  BOARD=$(basename $board | tr '-' '_')
  echo "BOARD_$BOARD"
  echo "CHIP_$CHIP"
  echo "CHIP_FAMILY_$CHIP_FAMILY"
  echo "CHIP_VARIANT_$CHIP_VARIANT"

  for asm in yes no; do
    output_base=$OUTPUT_DIR/$(basename $board)-${asm}asm
    OPTS=( )
    OPTS+=(
      "-DCHIP=$CHIP"
      "-DCHIP_FAMILY_$CHIP_FAMILY"
      "-DCHIP_VARIANT_$CHIP_VARIANT"
      "-DBOARD_$BOARD"
    )
    if [ "$asm" = "yes" ]; then
      OPTS+=("-D__ASSEMBLER__")
    fi
    OPTS+=(
      "-nostdinc"
      "-Ibuiltin"
      "-Iinclude"
      "-P"
      "-Ichip/stm32"
      "-I."
      "-I$board"
      "-Ifuzz"
      "-Itest"
    )
    # Get Macros
    {
      gcc "${OPTS[@]}" -dM -E - < chip/stm32/registers.h | sort > "${output_base}.h"
      if [ ${PIPESTATUS[0]} -ne 0 ]; then
        echo "Error - Failed to generate header"
        exit 1
      fi
      echo "Dumped to ${output_base}.h"
    } &

    # Get C declarations
    {
      cpp "${OPTS[@]}" chip/stm32/registers.h | sort > "${output_base}-c.h"
      if [ ${PIPESTATUS[0]} -ne 0 ]; then
        echo "Error - Failed to generate header"
        exit 1
      fi
      echo "Dumped to ${output_base}-c.h"
    } &
  done

  wait
  echo
done

# Compare board files
#
# diff --ignore-tab-expansion --ignore-trailing-space --ignore-space-change
# --ignore-all-space --ignore-blank-lines board-regs-original/ board-regs/
#
# OR
#
# meld board-regs-original board-regs
