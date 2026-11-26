// ============================================================
//  CZMAO DS17 R3（右手版）· 双宏 v8
//
//  放置位置：
//    keyboards/czmao/ds17/r3/keymaps/via/keymap.c
//
//  ┌─────────────┬──────────────────┬──────────────────────────┐
//  │  1 号宏     │ NumLock + 9      │ 攻击键 C，CD 2 秒        │
//  │             │ NumLock + 3      │ 停止（两个宏都停）        │
//  │             │                  │ 每 10~15 次挪一步，左右交替  │
//  ├─────────────┼──────────────────┼──────────────────────────┤
//  │  2 号宏     │ NumLock + 8      │ 攻击键 F，CD 0.5 秒      │
//  │             │ NumLock + 8      │ 再按一次停止              │
//  └─────────────┴──────────────────┴──────────────────────────┘
//
//  两个宏互斥：开一个会自动停掉另一个。
//  运行中按小键盘任何键 → 让路 0.5 秒，松手后自动恢复。
// ============================================================
#include QMK_KEYBOARD_H

/* ================= 键码 ================= */
enum my_keycodes {
    MACRO1_TOGGLE = SAFE_RANGE,  // NumLock + 9
    MACRO2_TOGGLE,               // NumLock + 8
    MACRO_STOP,                  // NumLock + 3，两个都停
};

/* ===================== 随机 ===================== */
static uint32_t rng_state = 0x12345678u;

static uint16_t rnd16(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return (uint16_t)(rng_state >> 16);
}

static uint32_t rand_range(uint32_t a, uint32_t b) {
    if (b <= a) return a;
    return a + (rnd16() % (b - a + 1));
}

/* 正向偏置随机：取值落在 [a,b]，但更偏向大的一侧（约 2/3 处）
   用于"宁晚不早"的施法延迟，避免在 CD 没好时提前按下 */
static uint32_t rand_skew_high(uint32_t a, uint32_t b) {
    uint32_t span = b - a;
    if (span == 0) return a;
    uint32_t u1 = rnd16() % (span + 1);
    uint32_t u2 = rnd16() % (span + 1);
    return a + (u1 > u2 ? u1 : u2);
}

/* ===================== 阶段 ===================== */
typedef enum {
    PH_WAIT = 0,   // 等下次施法
    PH_PRESS,      // 正按着攻击键
    PH_MOVE_WAIT,  // 等施法硬直结束再挪步
    PH_MOVE,       // 正在挪步（只走一个方向）
    PH_RECENTER,   // 回中补偿
} phase_t;

/* ===================== 宏结构 ===================== */
typedef struct {
    bool     on;
    uint16_t key;            // 攻击键
    uint32_t cd;             // 技能 CD
    uint32_t extra_min;      // CD 后延迟
    uint32_t extra_max;
    uint32_t press_min;      // 按下时长
    uint32_t press_max;
    uint8_t  skip_prob;      // 漏一拍概率
    uint32_t skip_min;       // 漏拍时额外等待
    uint32_t skip_max;
    bool     skew_high;      // true = 延迟偏向大的一侧（宁晚不早）
    uint8_t  long_tail_prob; // 偶尔慢半拍的概率(%)
    uint32_t long_tail_min;
    uint32_t long_tail_max;
    bool     use_move;       // 是否插入挪步
    uint16_t move_every_min; // 每多少下挪一次
    uint16_t move_every_max;
    uint32_t move_ms_min;    // 单方向挪步时长
    uint32_t move_ms_max;
    /* 运行状态 */
    phase_t  phase;
    uint32_t until;
    uint32_t next_press;
    uint32_t press_start;
    uint16_t count;
    uint16_t move_target;
    uint32_t move_ms;
    uint16_t mv1;
    uint16_t mv2;
} macro_t;

/* ---- 1 号宏：C / 2 秒 CD / 带挪步 ---- */
static macro_t m1 = {
    .key = KC_C,
    .cd = 2000,                              // 技能 CD 2 秒
    .extra_min = 100, .extra_max = 300,      // 总间隔 2.1~2.3 秒
    .press_min = 120, .press_max = 320,
    .skip_prob = 4, .skip_min = 1200, .skip_max = 2600,
    .skew_high = true,                       // 延迟偏大，宁晚不早
    .long_tail_prob = 5,                     // 5% 概率慢半拍
    .long_tail_min = 200, .long_tail_max = 500,
    .use_move = true,
    .move_every_min = 10, .move_every_max = 15,   // 每 10~15 次挪一次
    .move_ms_min = 60, .move_ms_max = 105,        // 原 40~70 的 1.5 倍
};

/* ---- 2 号宏：F / 0.5 秒 CD / 不挪步 ---- */
static macro_t m2 = {
    .key = KC_F,
    .cd = 500,
    .extra_min = 30,  .extra_max = 110,
    .press_min = 40,  .press_max = 90,
    .skip_prob = 3, .skip_min = 200, .skip_max = 500,
    .skew_high = false,
    .use_move = false,
};

/* ===================== 全局 ===================== */
static uint32_t suspend_until = 0;
static bool     next_move_left = true;   // 奇数次往左，偶数次往右，逐次交替
static uint32_t pending_move_ms = 0;     // 上一方向未配对的时长，供下一次反方向等长使用

/* ---- 回中（无传感器，靠记账 + 偏置） ---- */
static uint16_t recenter_every   = 6;    // 每 6 次挪步回中一次
static uint32_t recenter_bias_ms = 20;   // 每次回中额外向右补的毫秒
static uint32_t recenter_max_ms  = 220;  // 单次回中最大时长
static int32_t  drift_ms         = 0;    // 估算横向偏移：+右 -左
static uint16_t moves_done       = 0;    // 自上次回中以来挪了几步
static uint32_t move_start_ts    = 0;
static uint16_t recenter_key     = KC_RIGHT;
static uint32_t recenter_ms      = 0;
static uint32_t recenter_start_ts = 0;

/* ===================== 通用动作 ===================== */
static void drift_add(uint32_t ms, bool is_left) {
    drift_ms += is_left ? -(int32_t)ms : (int32_t)ms;
}

static void m_release(macro_t *m) {
    uint32_t now = timer_read32();

    /* 挪步中途被打断：按已走时长记账，避免误差累积 */
    if (m->phase == PH_MOVE) {
        uint32_t elapsed = now - move_start_ts;
        if (elapsed > m->move_ms) elapsed = m->move_ms;
        drift_add(elapsed, m->mv1 == KC_LEFT);
        moves_done++;
        pending_move_ms = 0;          // 配对作废，下次重新配对
    } else if (m->phase == PH_RECENTER) {
        uint32_t elapsed = now - recenter_start_ts;
        if (elapsed > recenter_ms) elapsed = recenter_ms;
        drift_add(elapsed, recenter_key == KC_LEFT);
    }

    unregister_code(m->key);
    if (m->use_move) {
        unregister_code(KC_LEFT);
        unregister_code(KC_RIGHT);
    }
    m->phase = PH_WAIT;
}

static void m_stop(macro_t *m) {
    m_release(m);
    m->on = false;

    /* 关宏即清账：位置由使用者自己纠正，不再沿用旧估算 */
    drift_ms        = 0;
    moves_done      = 0;
    pending_move_ms = 0;
}

static void m_schedule(macro_t *m) {
    uint32_t base  = m->press_start + m->cd;
    uint32_t delay = m->skew_high ? rand_skew_high(m->extra_min, m->extra_max)
                                  : rand_range(m->extra_min, m->extra_max);

    if ((rnd16() % 100) < m->skip_prob) {
        delay += rand_range(m->skip_min, m->skip_max);
    } else if ((rnd16() % 100) < m->long_tail_prob) {
        delay += rand_range(m->long_tail_min, m->long_tail_max);
    }
    m->next_press = base + delay;
    m->phase      = PH_WAIT;
}

static void m_press(macro_t *m) {
    register_code(m->key);
    m->press_start = timer_read32();
    m->until       = m->press_start + rand_range(m->press_min, m->press_max);
    m->phase       = PH_PRESS;
}

/* 方案 B：一次挪步只走一个方向，左右逐次交替；
   相邻两次（左、右）严格等时长，保证位置回到原点 */
static void m_start_move(macro_t *m, uint32_t now) {
    if (pending_move_ms) {
        m->move_ms      = pending_move_ms;   // 与上一次反向的那步完全一致
        pending_move_ms = 0;
    } else {
        m->move_ms      = rand_range(m->move_ms_min, m->move_ms_max);
        pending_move_ms = m->move_ms;        // 留给下一次反方向
    }

    if (next_move_left) {
        m->mv1 = KC_LEFT;
    } else {
        m->mv1 = KC_RIGHT;
    }
    next_move_left = !next_move_left;   // 下一次换另一边

    register_code(m->mv1);
    move_start_ts = now;
    m->until      = now + m->move_ms;
    m->phase      = PH_MOVE;
}

/* 回中：把估算的偏移补回来，并额外向右补 recenter_bias_ms
   返回 true 表示真的执行了回中动作 */
static bool start_recenter(macro_t *m, uint32_t now) {
    int32_t corr = -drift_ms + (int32_t)recenter_bias_ms;

    if (corr > (int32_t)recenter_max_ms) corr = (int32_t)recenter_max_ms;
    if (corr < -(int32_t)recenter_max_ms) corr = -(int32_t)recenter_max_ms;

    recenter_ms = (uint32_t)(corr < 0 ? -corr : corr);

    drift_ms   = 0;
    moves_done = 0;

    if (recenter_ms < 15) return false;   // 偏差太小，不值得动

    recenter_key      = (corr < 0) ? KC_LEFT : KC_RIGHT;
    recenter_start_ts = now;
    register_code(recenter_key);
    m->until = now + recenter_ms;
    m->phase = PH_RECENTER;
    return true;
}

static void stop_all(void) {
    m_stop(&m1);
    m_stop(&m2);
}

static void start_macro(macro_t *m) {
    stop_all();
    m->on          = true;
    m->count       = 0;
    m->move_target = rand_range(m->move_every_min, m->move_every_max);
    suspend_until  = 0;
    next_move_left = true;   // 每次启动，第一次挪步先往左
    m_press(m);
}

/* ===================== 单个宏的主循环 ===================== */
static void m_scan(macro_t *m, uint32_t now) {
    if (!m->on) return;

    if (now < suspend_until) {
        m_release(m);
        return;
    }

    switch (m->phase) {
        case PH_PRESS:
            if (now >= m->until) {
                unregister_code(m->key);
                m->count++;

                if (m->use_move && m->count >= m->move_target) {
                    // 等硬直结束再挪步
                    m->until = m->press_start + m->cd + rand_range(40, 120);
                    m->phase = PH_MOVE_WAIT;
                } else {
                    m_schedule(m);
                }
            }
            break;

        case PH_MOVE_WAIT:
            if (now >= m->until) {
                m_start_move(m, now);
            }
            break;

        case PH_MOVE:
            if (now >= m->until) {
                unregister_code(m->mv1);
                drift_add(m->move_ms, m->mv1 == KC_LEFT);
                moves_done++;
                m->count       = 0;
                m->move_target = rand_range(m->move_every_min, m->move_every_max);

                /* 到次数了就回中一次，否则继续打 */
                if (m->use_move && moves_done >= recenter_every && start_recenter(m, now)) {
                    break;
                }
                m->next_press = now + rand_range(120, 350);
                m->phase      = PH_WAIT;
            }
            break;

        case PH_RECENTER:
            if (now >= m->until) {
                unregister_code(recenter_key);
                m->next_press = now + rand_range(100, 300);
                m->phase      = PH_WAIT;
            }
            break;

        case PH_WAIT:
        default:
            if (now >= m->next_press) {
                m_press(m);
            }
            break;
    }
}

/* ===================== 主循环 ===================== */
void matrix_scan_user(void) {
    uint32_t now = timer_read32();
    m_scan(&m1, now);
    m_scan(&m2, now);
}

/* ===================== 键位表 ===================== */
/* FN 层改动：
     P8（8）: KC_UP    -> MACRO2_TOGGLE
     P9（9）: KC_TRNS  -> MACRO1_TOGGLE
     P3（3）: KC_TRNS  -> MACRO_STOP（两个都停）        */
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
        KC_HOME, MACRO2_TOGGLE, MACRO1_TOGGLE,
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

/* ===================== 开关与让路 ===================== */
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        if (keycode == MACRO1_TOGGLE) {
            if (m1.on) {
                m_stop(&m1);
            } else {
                start_macro(&m1);
            }
            suspend_until = timer_read32() + 200;
            return false;
        }
        if (keycode == MACRO2_TOGGLE) {
            if (m2.on) {
                m_stop(&m2);
            } else {
                start_macro(&m2);
            }
            suspend_until = timer_read32() + 200;
            return false;
        }
        if (keycode == MACRO_STOP) {
            stop_all();
            suspend_until = timer_read32() + 200;
            return false;
        }
        if (m1.on || m2.on) {
            m_release(&m1);
            m_release(&m2);
            suspend_until = timer_read32() + 500;
        }
    } else if ((m1.on || m2.on) && suspend_until && timer_read32() >= suspend_until) {
        suspend_until = 0;
        if (m1.on) {
            m1.next_press = timer_read32() + rand_range(150, 500);
            m1.phase      = PH_WAIT;
        }
        if (m2.on) {
            m2.next_press = timer_read32() + rand_range(150, 500);
            m2.phase      = PH_WAIT;
        }
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

    if (m1.on || m2.on) {
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
            rgb_matrix_set_color(i, 60, 25, 0);
        }
    }
    return true;
}
