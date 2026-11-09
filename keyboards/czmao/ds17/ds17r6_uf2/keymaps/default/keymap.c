#include QMK_KEYBOARD_H
#include "gpio.h"
#include "wait.h"

void keyboard_pre_init_user(void) {
    setPinOutput(B13);
    writePinLow(B13);
    wait_us(300);
}

void matrix_init_user(void) {
    rgb_matrix_set_color_all(0, 0, 0);
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_numpad_5x4(
        LT(1, KC_NUM), KC_PSLS, KC_PAST, KC_PMNS,
        KC_P7,         KC_P8,   KC_P9,   KC_PPLS,
        KC_P4,         KC_P5,   KC_P6,
        KC_P1,         KC_P2,   KC_P3,   KC_PENT,
        KC_P0,                  KC_PDOT
    ),
    [1] = LAYOUT_numpad_5x4(
        KC_TRNS,  KC_TRNS,  KC_TRNS,  RGB_TOG,
        RGB_MOD,   RGB_HUI,  RGB_SAI,  RGB_VAI,
        RGB_RMOD,  RGB_HUD,  RGB_SAD,
        KC_TRNS,   KC_TRNS,  KC_TRNS,  QK_BOOT,
        KC_TRNS,             RGB_VAD
    )
};

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (host_keyboard_led_state().num_lock) {
        RGB_MATRIX_INDICATOR_SET_COLOR(0, 255, 255, 255);
    }
    return false;
}
