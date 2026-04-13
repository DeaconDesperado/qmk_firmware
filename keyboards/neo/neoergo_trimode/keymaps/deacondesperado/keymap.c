// Copyright 2023 JoyLee (@itarze)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "wireless.h"

// External declarations for wireless functions
extern uint8_t *md_getp_bat(void);
extern uint8_t *md_getp_state(void);

enum layers {
    MAC_B,
    MAC_FN,
};

enum custom_keycodes {
    KC_SLEEP_NOW = SAFE_RANGE,
    KC_LED_ID,  // LED identification mode toggle
};

// LED identification mode state
static bool led_id_mode = false;

// Blink state for device indicators
static uint8_t blink_index = 0;
static bool blink_fast = true;
static bool blink_slow = true;

// Leader key state tracking
static bool leader_key_held = false;

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_B] = LAYOUT(
        KC_ESC,  KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_NUHS, KC_BSPC,
        KC_PGUP, KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,
        KC_PGDN, KC_ESC, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,          KC_ENT,
     KC_HOME, KC_LSFT, KC_NUBS, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    MO(MAC_FN),    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_UP,   KC_RSFT,
                 KC_LCTL, KC_LALT,                            KC_LALT, KC_LGUI,  KC_SPC,  KC_RCTL,                            KC_LEFT, KC_DOWN, KC_RGHT
    ),

    [MAC_FN] = LAYOUT(
        EE_CLR, KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  _______, _______,
        _______, KC_NXT, KC_GRV, LT(0, KC_BT1), LT(0, KC_BT2), LT(0, KC_BT3), LT(0, KC_2G4), _______, KC_USB, KC_SLEEP_NOW, KC_LED_ID, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,          _______,
        KC_END, _______, _______, RM_TOGG, RM_NEXT, RM_PREV, RM_HUEU, RM_HUED, RM_HUED, RM_SPDU, RM_SPDD, RM_VALU, RM_VALD, _______, _______, _______,
                 _______, _______,                          _______, _______, _______, _______,                            _______, _______, QK_BOOTLOADER
    )
};

// clang-format on

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case KC_SLEEP_NOW:
            if (record->event.pressed) {
                // Force keyboard into sleep mode immediately
                lpwr_set_timeout_manual(true);
            }
            return false;
        case KC_LED_ID:
            if (record->event.pressed) {
                // Toggle LED identification mode
                led_id_mode = !led_id_mode;
            }
            return false;

        // --- Device Selection Logic (moved from keyboard level) ---
        case KC_USB:
            if (record->event.pressed) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);
            }
            return false;

        case KC_NXT:
            if (record->event.pressed) {
                // Cycle through devices: USB -> BT1 -> BT2 -> BT3 -> 2.4G -> USB...
                uint8_t current = wireless_get_current_devs();
                uint8_t next;
                switch (current) {
                    case DEVS_USB: next = DEVS_BT1; break;
                    case DEVS_BT1: next = DEVS_BT2; break;
                    case DEVS_BT2: next = DEVS_BT3; break;
                    case DEVS_BT3: next = DEVS_2G4; break;
                    case DEVS_2G4: next = DEVS_USB; break;
                    default: next = DEVS_USB; break;
                }
                wireless_devs_change(current, next, false);
            }
            return false;

        case LT(0, KC_BT1):
            if (record->tap.count && record->event.pressed) {
                // Tap: Switch to BT1
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT1, false);
            } else if (record->event.pressed && *md_getp_state() != MD_STATE_PAIRING) {
                // Hold: Switch to BT1 and enter pairing mode
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT1, true);
            }
            return false;

        case LT(0, KC_BT2):
            if (record->tap.count && record->event.pressed) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT2, false);
            } else if (record->event.pressed && *md_getp_state() != MD_STATE_PAIRING) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT2, true);
            }
            return false;

        case LT(0, KC_BT3):
            if (record->tap.count && record->event.pressed) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT3, false);
            } else if (record->event.pressed && *md_getp_state() != MD_STATE_PAIRING) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT3, true);
            }
            return false;

        case LT(0, KC_2G4):
            if (record->tap.count && record->event.pressed) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, false);
            } else if (record->event.pressed && *md_getp_state() != MD_STATE_PAIRING) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, true);
            }
            return false;

        // --- Leader key tracking (backslash) ---
        case KC_BSLS:
            if (record->event.pressed) {
                leader_key_held = true;
            } else {
                leader_key_held = false;
            }
            break;  // Let the key still function normally
    }
    return true;
}

// Custom RGB indicator logic - disables all effects, uses LEDs as indicators
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    // LED identification mode - useful for mapping physical LEDs
    if (led_id_mode) {
        // Define unique colors for each LED (R, G, B)
        const uint8_t led_colors[11][3] = {
            {255, 0, 0},      // LED 5: Red
            {255, 127, 0},    // LED 6: Orange
            {255, 255, 0},    // LED 7: Yellow
            {127, 255, 0},    // LED 8: Yellow-Green
            {0, 255, 0},      // LED 9: Green
            {0, 255, 127},    // LED 10: Cyan-Green
            {0, 255, 255},    // LED 11: Cyan
            {0, 127, 255},    // LED 12: Light Blue
            {0, 0, 255},      // LED 13: Blue
            {127, 0, 255},    // LED 14: Purple
            {255, 0, 255}     // LED 15: Magenta
        };

        // Set each LED 5-15 to its unique color
        for (uint8_t i = 5; i <= 15; i++) {
            rgb_matrix_set_color(i, led_colors[i - 5][0], led_colors[i - 5][1], led_colors[i - 5][2]);
        }

        // Keep LEDs 0-4 off for clarity
        for (uint8_t i = 0; i <= 4; i++) {
            rgb_matrix_set_color(i, 0, 0, 0);
        }

        return false;  // Block all other RGB processing
    }

    // ==========================================
    // Custom Indicator Mode - No RGB effects
    // ==========================================

    // Turn off ALL LEDs first (no background effects)
    rgb_matrix_set_color_all(0, 0, 0);


    // --- Battery Level Indicators (LEDs 5, 6, 7) ---
    uint8_t battery_level = *md_getp_bat();  // 0-100%

    // Check if actively charging (BT_CHARGE_PIN is LOW when charging)
    bool is_charging = !gpio_read_pin(BT_CHARGE_PIN);

    // Use thirds for equal distribution: 67%+ / 34-66% / 1-33% / 0%
    // When charging, blink the battery LEDs slowly
    if (battery_level >= 67) {
        // High: 3 green LEDs
        if (!is_charging || blink_slow) {
            rgb_matrix_set_color(5, 0, 255, 0);
            rgb_matrix_set_color(6, 0, 255, 0);
            rgb_matrix_set_color(7, 0, 255, 0);
        }
    } else if (battery_level >= 34) {
        // Medium: 2 yellow LEDs
        if (!is_charging || blink_slow) {
            rgb_matrix_set_color(5, 255, 255, 0);
            rgb_matrix_set_color(6, 255, 255, 0);
        }
    } else if (battery_level >= 1) {
        // Low: 1 red LED
        if (!is_charging || blink_slow) {
            rgb_matrix_set_color(5, 255, 0, 0);
        }
    }
    // Critical (0%): All off (already cleared by set_color_all)


    // --- Layer & Modifier Indicators ---
    #define DIVINE_BLUE     186, 235, 247
    #define DEVS_USB_INDEX  0
    #define DEVS_BT_INDEX   2
    #define DEVS_2G_INDEX   4
    #define WHITE_INDICATION   200, 200, 200
    #define BT_1_COLOR      130, 200, 182
    #define BT_2_COLOR      186, 235, 147
    #define BT_3_COLOR      235, 131, 200

    if (layer_state_is(MAC_FN)) {
        rgb_matrix_set_color(8, DIVINE_BLUE);
    }

    // Shift indicator (either left or right shift)
    if (get_mods() & MOD_MASK_SHIFT) {
        rgb_matrix_set_color(11, DIVINE_BLUE);
    }

    // Control indicator
    if (get_mods() & MOD_MASK_CTRL) {
        rgb_matrix_set_color(12, DIVINE_BLUE);
    }

    // Alt indicator
    if (get_mods() & MOD_MASK_ALT) {
        rgb_matrix_set_color(13, DIVINE_BLUE);
    }

    // GUI/Command indicator
    if (get_mods() & MOD_MASK_GUI) {
        rgb_matrix_set_color(9, DIVINE_BLUE);
    }

    // Leader key indicator (backslash)
    if (leader_key_held) {
        rgb_matrix_set_color(10, BT_3_COLOR);
    }

    // --- Device Indicators (LEDs 0-4) ---
    // Update blink state
    blink_index = blink_index + 1;
    blink_fast  = (blink_index % 64 == 0) ? !blink_fast : blink_fast;
    blink_slow  = (blink_index % 128 == 0) ? !blink_slow : blink_slow;

    // Get current device and wireless state
    uint8_t current_device = wireless_get_current_devs();
    uint8_t wireless_state = *md_getp_state();

    // Show device indicator based on current device
    switch (current_device) {
        case DEVS_USB:
            rgb_matrix_set_color(DEVS_USB_INDEX, WHITE_INDICATION);
            break;
        case DEVS_BT1:
            if (wireless_state == MD_STATE_PAIRING) {
                // Fast blink when pairing
                if (blink_fast) {
                    rgb_matrix_set_color(DEVS_BT_INDEX, BT_1_COLOR);
                } else {
                    rgb_matrix_set_color(DEVS_BT_INDEX, 0, 0, 0);
                }
            } else if (wireless_state != MD_STATE_CONNECTED) {
                // Slow blink when disconnected
                if (blink_slow) {
                    rgb_matrix_set_color(DEVS_BT_INDEX, BT_1_COLOR);
                } else {
                    rgb_matrix_set_color(DEVS_BT_INDEX, 0, 0, 0);
                }
            } else {
                rgb_matrix_set_color(DEVS_BT_INDEX, BT_1_COLOR);  // Solid when connected
            }
            break;
        case DEVS_BT2:
            if (wireless_state == MD_STATE_PAIRING) {
                if (blink_fast) {
                    rgb_matrix_set_color(DEVS_BT_INDEX, BT_2_COLOR);
                } else {
                    rgb_matrix_set_color(DEVS_BT_INDEX, 0, 0, 0);
                }
            } else if (wireless_state != MD_STATE_CONNECTED) {
                if (blink_slow) {
                    rgb_matrix_set_color(DEVS_BT_INDEX, BT_2_COLOR);
                } else {
                    rgb_matrix_set_color(DEVS_BT_INDEX, 0, 0, 0);
                }
            } else {
                rgb_matrix_set_color(DEVS_BT_INDEX, BT_2_COLOR);
            }
            break;
        case DEVS_BT3:
            if (wireless_state == MD_STATE_PAIRING) {
                if (blink_fast) {
                    rgb_matrix_set_color(DEVS_BT_INDEX, BT_3_COLOR);
                } else {
                    rgb_matrix_set_color(DEVS_BT_INDEX, 0, 0, 0);
                }
            } else if (wireless_state != MD_STATE_CONNECTED) {
                if (blink_slow) {
                    rgb_matrix_set_color(DEVS_BT_INDEX, BT_3_COLOR);
                } else {
                    rgb_matrix_set_color(DEVS_BT_INDEX, 0, 0, 0);
                }
            } else {
                rgb_matrix_set_color(DEVS_BT_INDEX, BT_3_COLOR);
            }
            break;
        case DEVS_2G4:
            if (wireless_state == MD_STATE_PAIRING) {
                if (blink_fast) {
                    rgb_matrix_set_color(DEVS_2G_INDEX, WHITE_INDICATION);
                } else {
                    rgb_matrix_set_color(DEVS_2G_INDEX, 0, 0, 0);
                }
            } else if (wireless_state != MD_STATE_CONNECTED) {
                if (blink_slow) {
                    rgb_matrix_set_color(DEVS_2G_INDEX, WHITE_INDICATION);
                } else {
                    rgb_matrix_set_color(DEVS_2G_INDEX, 0, 0, 0);
                }
            } else {
                rgb_matrix_set_color(DEVS_2G_INDEX, WHITE_INDICATION);
            }
            break;
    }

    return false;
}
