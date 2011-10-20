/* keyboard_backlight.h - Keyboard backlight
 *
 * (Chromium license) */

#ifndef __CHIP_INTERFACE_KEYBOARD_H
#define __CHIP_INTERFACE_KEYBOARD_H

#include <stdint.h>

struct KeyIndex {
  union {
    struct {
      uint8_t col:5;
      uint8_t row:3;
    };
    uint8_t index;
  };
};


enum KeyPressState {
  KEY_RELEASED = 0,
  KEY_PRESSED = 1,
};


/* Returns the max col and row number supported by EC hardware.
 * These numbers can help to arrange memory.
 * Note that this is different to the col/row numbers defined in board-specific.
 */
EcError CrKeyboardGetMaxColRow(int8_t *col, int8_t *row);

/* Regiters a callback function to underlayer EC lib. So that any key state
 * change would notify the upper EC main code.
 */
EcError CrKeyboardRegister(
    void (*cb)(int8_t col, int8_t row, enum KeyPressState state));

/* Asks the underlayer EC lib what keys are pressed right now.
 * Returns a byte array of pressed key.
 */
EcError CrKeyboardGetState(struct KeyIndex *out, int size, int *ret_num);

#endif  /* __CHIP_INTERFACE_KEYBOARD_H */
