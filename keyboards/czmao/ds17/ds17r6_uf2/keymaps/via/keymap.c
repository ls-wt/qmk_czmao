#include QMK_KEYBOARD_H
#include "gpio.h"
#include "wait.h"
#include "eeconfig.h"

/* VIA "功能" tab custom commands (value ids must match the JSON menus).
 * data = [ command_id, channel_id, value_id, value_data ] */
enum via_czm_value {
    id_czm_reset_kb      = 1, /* 恢复出厂设置 (action) */
    id_czm_bootloader_kb = 2, /* 进入 Bootloader (action) */
    id_czm_nkro_kb       = 3, /* 全键无冲 NKRO on/off (0=6KRO, 1=NKRO) */
};

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    uint8_t *command_id = &data[0];
    uint8_t *channel_id = &data[1];
    uint8_t *value_id   = &data[2];
    uint8_t *value_data = &data[3];

    if (*channel_id != id_custom_channel) {
        *command_id = id_unhandled;
        return;
    }

    switch (*command_id) {
        case id_custom_get_value:
            switch (*value_id) {
                case id_czm_reset_kb:
                case id_czm_bootloader_kb:
                    /* Action-only buttons are read on menu load; answer 0. */
                    *value_data = 0;
                    break;
                case id_czm_nkro_kb:
                    *value_data = keymap_config.nkro ? 1 : 0;
                    break;
                default:
                    *command_id = id_unhandled;
                    break;
            }
            break;
        case id_custom_set_value:
            switch (*value_id) {
                case id_czm_reset_kb:
                    eeconfig_init_via();
                    clear_keyboard();
                    soft_reset_keyboard();
                    break;
                case id_czm_bootloader_kb:
                    clear_keyboard();
                    soft_reset_keyboard();
                    break;
                case id_czm_nkro_kb:
                    clear_keyboard();
                    keymap_config.nkro = (*value_data != 0);
                    eeconfig_update_keymap(keymap_config.raw);
                    clear_keyboard();
                    break;
                default:
                    *command_id = id_unhandled;
                    break;
            }
            break;
        case id_custom_save:
            break;
        default:
            *command_id = id_unhandled;
            break;
    }
}

void keyboard_pre_init_user(void) {
    gpio_set_pin_output(B13);
    gpio_write_pin_low(B13);
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
        KC_TRNS,  KC_TRNS,  MO(2),    RM_VALU,
        RM_TOGG,   RM_SATU,  KC_TRNS,  RM_VALD,
        RM_HUED,   KC_TRNS,  RM_HUEU,
        KC_TRNS,   RM_SATD,  KC_TRNS,  KC_TRNS,
        RM_NEXT,             KC_TRNS
    ),
    [2] = LAYOUT_numpad_5x4(
        QK_BOOT,  KC_TRNS,  KC_TRNS,  NK_TOGG,
        KC_TRNS,   KC_TRNS,  KC_TRNS,  KC_TRNS,
        KC_TRNS,   EE_CLR,  KC_TRNS,
        KC_TRNS,   KC_TRNS,  KC_TRNS,  KC_TRNS,
        KC_TRNS,             KC_TRNS
    )
};

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (host_keyboard_led_state().num_lock) {
        RGB_MATRIX_INDICATOR_SET_COLOR(0, 255, 255, 255);
    }
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    for (uint8_t i = led_min; i < led_max; i++) {
        switch (layer) {
            case 7: rgb_matrix_set_color(i, RGB_PURPLE); break;
            case 6: rgb_matrix_set_color(i, RGB_WHITE); break;
            case 5: rgb_matrix_set_color(i, RGB_YELLOW); break;
            case 4: rgb_matrix_set_color(i, RGB_PINK); break;
            case 3: rgb_matrix_set_color(i, RGB_BLUE); break;
            case 2: rgb_matrix_set_color(i, RGB_GREEN); break;
            case 1: rgb_matrix_set_color(i, RGB_RED); break;
            default: break;
        }
    }
    return false;
}
