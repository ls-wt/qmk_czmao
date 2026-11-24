// ============================================================
//  CZMAO DS17 R3（右手版）· VIA keymap + 自动攻击宏
//
//  放置位置（GitHub 网页里直接覆盖这个文件）：
//    keyboards/czmao/ds17/r3/keymaps/via/keymap.c
//
//  开关方式（与 r6 版一致）：
//    按住左上角 NumLock（FN 层） + 数字 9  → 开启 / 关闭
//    按住左上角 NumLock（FN 层） + 数字 3  → 立刻停止
//
//  若你的 PCB 丝印是 R6 而不是 R3，把路径里的 r3 换成 r6，内容不变。
// ============================================================
#include QMK_KEYBOARD_H

/* ================= 宏开关用的自定义键码 ================= */
enum my_keycodes {
    MACRO_TOGGLE = SAFE_RANGE,  // 开 / 关
    MACRO_STOP,                 // 立刻停
};

/* ===================== 宏状态机 ===================== */
typedef enum {
    ST_ATTACKING,
    ST_HOLDING,
    ST_PAUSING,
    ST_NOISE
} macro_state_t;

static bool          macro_on          = false;
static macro_state_t macro_state       = ST_ATTACKING;
static uint32_t      session_start     = 0;
static uint32_t      session_duration  = 0;
static uint32_t      next_action_time  = 0;
static uint32_t      key_release_time  = 0;
static uint32_t      next_param_change = 0;
static uint16_t      noise_keycode     = 0;

/* ========== 参数区，想调直接改这里 ========== */
static uint16_t attack_key        = KC_C;   // 攻击按键
static uint16_t base_mean         = 2200;   // 攻击间隔均值(ms)
static uint16_t press_mean        = 150;    // 按压时长均值(ms)
static uint16_t press_sd          = 40;     // 按压时长标准差(ms)
static uint8_t  pause_prob        = 5;      // 犹豫概率 %
static uint8_t  tail_prob         = 3;      // 走神长间隔概率 %
static uint8_t  noise_prob        = 1;      // 方向键噪声概率 %
static uint32_t max_tail_interval = 8000;   // 走神间隔上限(ms)

/* ===================== 工具函数 ===================== */
/* 自带的小型伪随机（线性同余），不依赖 QMK 或 libc 的随机函数 */
static uint32_t rng_state = 0x12345678u;

static void rng_seed(uint32_t s) {
    rng_state = s ? s : 0x12345678u;
}

static uint16_t rnd16(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (uint16_t)(rng_state >> 16);
}

static int16_t gaussian_100(void) {
    int16_t sum = 0;
    for (uint8_t i = 0; i < 12; i++) {
        sum += rnd16() % 101;
    }
    return sum - 600;
}

static uint32_t normal_clamped(uint32_t mean, uint32_t sd, uint32_t min, uint32_t max) {
    int32_t g = gaussian_100();
    int32_t v = (int32_t)mean + (g * (int32_t)sd) / 100;
    if (v < (int32_t)min) v = (int32_t)min;
    if (v > (int32_t)max) v = (int32_t)max;
    return (uint32_t)v;
}

static uint32_t rand_range(uint32_t min, uint32_t max) {
    if (max <= min) return min;
    return min + (rnd16() % (max - min + 1));
}

/* 只释放宏自己按下的键，不动你手上正按的键 */
static void stop_macro(void) {
    if (macro_state == ST_HOLDING) {
        unregister_code(attack_key);
    } else if (macro_state == ST_NOISE && noise_keycode) {
        unregister_code(noise_keycode);
    }
    noise_keycode = 0;
    macro_on      = false;
    macro_state   = ST_ATTACKING;
}

/* 开局慢一点，后半段疲劳变慢 */
static uint32_t effective_mean(void) {
    uint32_t elapsed = timer_read32() - session_start;
    uint32_t total   = session_duration;
    if (total == 0) return base_mean;

    int32_t start_factor = 100;
    if (elapsed < 120000UL) {
        start_factor = 115 - (int32_t)((elapsed * 15) / 120000UL);
    }

    int32_t  fatigue_factor = 100;
    uint32_t half           = total / 2;
    if (half && elapsed > half) {
        uint32_t late = elapsed - half;
        int32_t  add  = (12 * (int32_t)late) / (int32_t)half;
        fatigue_factor = 100 + add;
    }

    return ((uint32_t)base_mean * start_factor * fatigue_factor) / 10000UL;
}

static uint32_t generate_interval(void) {
    uint32_t mean = effective_mean();
    uint32_t sd   = (mean * rand_range(12, 25)) / 100;

    if ((rnd16() % 100) < tail_prob) {
        uint32_t raw = normal_clamped(mean, sd, 2000, 6000) + rand_range(800, 4000);
        return raw > max_tail_interval ? max_tail_interval : raw;
    }
    return normal_clamped(mean, sd, 2000, 6000);
}

static void randomize_params(void) {
    base_mean  = rand_range(2100, 2800);
    press_mean = rand_range(80, 220);
    press_sd   = rand_range(20, 60);
    pause_prob = rand_range(3, 8);
    tail_prob  = rand_range(1, 5);
    noise_prob = rand_range(0, 2);
}

static void start_macro(void) {
    rng_seed(timer_read32());
    session_duration = rand_range(15, 40) * 60000UL;  // 15-40 分钟自动收工
    session_start    = timer_read32();
    macro_on         = true;
    macro_state      = ST_ATTACKING;
    noise_keycode    = 0;
    randomize_params();

    next_action_time  = session_start + generate_interval();
    next_param_change = session_start + rand_range(180, 360) * 1000UL;
}

/* ===================== 键位表 ===================== */
/* FN 层里两处改动：
     KC_CALC -> MACRO_TOGGLE
     KC_N    -> MACRO_STOP                                     */
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

    if (now - session_start >= session_duration) {
        stop_macro();
        return;
    }

    if (now >= next_param_change) {
        randomize_params();
        next_param_change = now + rand_range(180, 360) * 1000UL;
    }

    switch (macro_state) {
        case ST_PAUSING:
            if (now >= next_action_time) {
                macro_state      = ST_ATTACKING;
                next_action_time = now + generate_interval();
            }
            break;

        case ST_HOLDING:
            if (now >= key_release_time) {
                unregister_code(attack_key);

                uint16_t roll = rnd16() % 100;

                if (roll < noise_prob) {
                    uint16_t dirs[] = {KC_LEFT, KC_RIGHT, KC_UP, KC_DOWN};
                    noise_keycode    = dirs[rnd16() % 4];
                    register_code(noise_keycode);
                    key_release_time = now + rand_range(20, 50);
                    macro_state      = ST_NOISE;
                } else if (roll < noise_prob + pause_prob) {
                    macro_state      = ST_PAUSING;
                    next_action_time = now + normal_clamped(2000, 800, 800, 5000);
                } else {
                    macro_state      = ST_ATTACKING;
                    next_action_time = now + generate_interval();
                }
            }
            break;

        case ST_NOISE:
            if (now >= key_release_time) {
                unregister_code(noise_keycode);
                noise_keycode    = 0;
                macro_state      = ST_ATTACKING;
                next_action_time = now + generate_interval();
            }
            break;

        case ST_ATTACKING:
            if (now >= next_action_time) {
                register_code(attack_key);
                key_release_time = now + normal_clamped(press_mean, press_sd, 50, 350);
                macro_state      = ST_HOLDING;
            }
            break;
    }
}

/* ===================== 开关键处理 ===================== */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;

    if (keycode == MACRO_TOGGLE) {
        if (macro_on) {
            stop_macro();
        } else {
            start_macro();
        }
        return false;
    }
    if (keycode == MACRO_STOP) {
        stop_macro();
        return false;
    }
    return true;
}

/* ===================== RGB 指示（保留原厂逻辑） ===================== */
// 用 basic 指示器钩子：QMK 在整帧渲染结束后只调用一次（见 quantum/rgb_matrix/rgb_matrix.c
// rgb_task_state == FLUSHING 分支），此处的写入即为本帧最终值，不会与动画逐分片互相覆盖导致爆闪。
bool rgb_matrix_indicators_user(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    // 非默认层：整板铺层色（保持原有观感）
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

    // numlock 指示：必须最后写，保证任何层都生效（不再被层色覆盖）
    if (host_keyboard_led_state().num_lock) {
        rgb_matrix_set_color(0, 255, 255, 255);
    }

    // 宏运行中：整板暗橙，一眼就能看出它还在跑
    if (macro_on) {
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            rgb_matrix_set_color(i, 60, 25, 0);
        }
    }
    return true;
}
