// ============================================================
//  CZMAO DS17 R3（右手版）· 一键攻击 v6（拟人节奏）
//
//  放置位置：
//    keyboards/czmao/ds17/r3/keymaps/via/keymap.c
//
//  用法：
//    按住左上角 NumLock + 点 9  → 开始
//    按住左上角 NumLock + 点 3  → 停止
//    运行中按小键盘任何键        → 让路，松手后自动恢复
//
//  设计目标：模拟人的施法节奏
//    · 施法按 CD 3 秒走，每次在 CD 好之后 120~420ms 才按下
//    · 单次按下 120~320ms 后松开，等待下一次
//    · 4% 概率"漏一拍"（多等 1.2~2.6 秒，像走神）
//    · 你自己按键时立刻让路，松手 0.5 秒后恢复
//    · 不设自动休息，捡东西时手动停
// ============================================================
#include QMK_KEYBOARD_H

/* ================= 键码 ================= */
enum my_keycodes {
    MACRO_TOGGLE = SAFE_RANGE,
    MACRO_STOP,
};

/* ===================== 参数区（想调就改这里） ===================== */
static uint16_t attack_key = KC_C;      // 攻击键

static uint32_t cd_ms      = 3000;      // 技能 CD

static uint32_t extra_min  = 120;       // CD 好后延迟下限(ms)
static uint32_t extra_max  = 420;       // CD 好后延迟上限(ms)

static uint32_t press_min  = 120;       // 单次按住时长下限(ms)
static uint32_t press_max  = 320;       // 单次按住时长上限(ms)

static uint8_t  skip_prob  = 4;         // 漏一拍的概率(%)

static uint32_t skip_add_min = 1200;    // 漏拍时多等多久(ms)
static uint32_t skip_add_max = 2600;

static uint32_t suspend_ms = 500;       // 你按键时让路时长(ms)

/* ===================== 状态 ===================== */
static bool     macro_on     = false;
static bool     pressing     = false;
static uint32_t press_start  = 0;
static uint32_t press_until  = 0;
static uint32_t next_press   = 0;
static uint32_t suspend_until = 0;
static uint32_t rng_state    = 0x12345678u;

/* ===================== 随机 ===================== */
static uint16_t rnd16(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (uint16_t)(rng_state >> 16);
}

static uint32_t rand_range(uint32_t a, uint32_t b) {
    if (b <= a) return a;
    return a + (rnd16() % (b - a + 1));
}

/* ===================== 按键动作 ===================== */
static void attack_press(void) {
    if (!pressing) {
        register_code(attack_key);
        pressing    = true;
        press_start = timer_read32();
        press_until = press_start + rand_range(press_min, press_max);
    }
}

static void attack_release(void) {
    if (pressing) {
        unregister_code(attack_key);
        pressing = false;
    }
}

/* 安排下一次施法：CD + 人类延迟，偶尔漏一拍 */
static void schedule_next(void) {
    uint32_t base  = press_start + cd_ms;
    uint32_t delay = rand_range(extra_min, extra_max);

    if ((rnd16() % 100) < skip_prob) {
        delay += rand_range(skip_add_min, skip_add_max);
    }
    next_press = base + delay;
}

static void start_macro(void) {
    macro_on      = true;
    suspend_until = 0;
    attack_press();
}

static void stop_macro(void) {
    attack_release();
    macro_on      = false;
    suspend_until = 0;
}

/* ===================== 键位表 ===================== */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_numberpad_5x4(
        LT(1,KC_NUM), KC_PSLS, KC_PAST, KC_PMNS,
        LT(7,KC_P7),  KC_P8,   KC_P9,
        LT(4,KC_P4),  LT(5,KC_P5), LT(6,KC_P6), KC_PPLS,
        LT(1,KC_P1),  LT(2,KC_P2), LT(3,KC_P3),
        KC_P0,                  KC_PDOT, KC_PENT
    ),
    [1] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_NUM,  MO(2),   KC_CALC,
        KC_HOME, KC_UP,   MACRO_TOGGLE,
        KC_LEFT, KC_TRNS, KC_RGHT, KC_TRNS,
        KC_END,  KC_DOWN, MACRO_STOP,
        KC_LSFT,          KC_RSFT, KC_N
    ),
    [2] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_TRNS, KC_TRNS, RM_VALU,
        RM_TOGG, RM_SATU, KC_TRNS,
        RM_HUED, EE_CLR,  RM_HUEU, RM_VALD,
        KC_TRNS, RM_SATD, KC_TRNS,
        RM_NEXT,          KC_TRNS, KC_TRNS
    ),
    [3] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,          KC_TRNS, KC_TRNS
    ),
    [4] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,          KC_TRNS, KC_TRNS
    ),
    [5] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,          KC_TRNS, KC_TRNS
    ),
    [6] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,          KC_TRNS, KC_TRNS
    ),
    [7] = LAYOUT_numberpad_5x4(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,          KC_TRNS, EE_CLR
    )
};

/* ===================== 主循环 ===================== */
void matrix_scan_user(void) {
    if (!macro_on) return;

    uint32_t now = timer_read32();

    /* ① 你在用小键盘 → 让路 */
    if (now < suspend_until) {
        attack_release();
        return;
    }

    /* ② 按键保持中，到时间就松开 */
    if (pressing) {
        if (now >= press_until) {
            attack_release();
            schedule_next();
        }
        return;
    }

    /* ③ 到点施法 */
    if (now >= next_press) {
        attack_press();
    }
}

/* ===================== 开关与让路 ===================== */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if (keycode == MACRO_TOGGLE) {
            if (macro_on) {
                stop_macro();
            } else {
                start_macro();
            }
            suspend_until = timer_read32() + 200;
            return false;
        }
        if (keycode == MACRO_STOP) {
            stop_macro();
            suspend_until = timer_read32() + 200;
            return false;
        }
        if (macro_on) {
            attack_release();
            suspend_until = timer_read32() + suspend_ms;
        }
    } else if (macro_on && suspend_until && timer_read32() >= suspend_until) {
        suspend_until = 0;
        next_press    = timer_read32() + rand_range(150, 500);  // 松手后像人一样稍等再按
    }
    return true;
}

/* ===================== RGB 指示 ===================== */
bool rgb_matrix_indicators_user(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    if (layer) {
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            switch (layer) {
                case 7: rgb_matrix_set_color(i, RGB_PURPLE); break;
                case 6: rgb_matrix_set_color(i, RGB_WHITE);  break;
                case 5: rgb_matrix_set_color(i, RGB_YELLOW); break;
                case 4: rgb_matrix_set_color(i, RGB_PINK);   break;
                case 3: rgb_matrix_set_color(i, RGB_BLUE);   break;
                case 2: rgb_matrix_set_color(i, RGB_GREEN);  break;
                case 1: rgb_matrix_set_color(i, RGB_RED);    break;
                default: break;
            }
        }
    }

    if (host_keyboard_led_state().num_lock) {
        rgb_matrix_set_color(0, 255, 255, 255);
    }

    if (macro_on) {
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            rgb_matrix_set_color(i, 60, 25, 0);
        }
    }
    return true;
}
