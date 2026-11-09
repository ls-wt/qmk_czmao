#include "quantum.h"

// Power-on LED self-test: sweep R -> G -> B once, then release to the default animation.
static bool rgb_selftest_active = true;
static uint32_t rgb_selftest_timer = 0;

void keyboard_post_init_user(void) {
    rgb_selftest_timer = timer_read32();
    rgb_selftest_active = true;
    rgb_matrix_enable_noeeprom();  // ensure LEDs are on so the self-test is visible
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    if (rgb_selftest_active) {
        uint32_t elapsed = timer_elapsed32(rgb_selftest_timer);
        const uint32_t phase = 2000;  // ms each color is shown (2s R / 2s G / 2s B)

        if (elapsed < phase) {
            rgb_matrix_set_color_all(255, 0, 0);        // Red
        } else if (elapsed < phase * 2) {
            rgb_matrix_set_color_all(0, 255, 0);        // Green
        } else if (elapsed < phase * 3) {
            rgb_matrix_set_color_all(0, 0, 255);        // Blue
        } else if (elapsed < phase * 3 + 150) {
            rgb_matrix_set_color_all(0, 0, 0);          // brief off between B and default
        } else {
            rgb_selftest_active = false;                // self-test complete
        }

        if (rgb_selftest_active) {
            return true;  // our colors win during self-test; skip user layer coloring
        }
    }

    // self-test done: hand off to the keymap's layer indicators + default animation
    return rgb_matrix_indicators_advanced_user(led_min, led_max);
}
