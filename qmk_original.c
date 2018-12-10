//
// Created by abata on 2018/12/10.
//
#include <stdint.h>
#include <string.h>
#include "keyboard.h"
#include "matrix.h"
#include "keymap.h"
#include "host.h"
#include "led.h"
#include "keycode.h"
#include "timer.h"
#include "print.h"
#include "debug.h"
#include "command.h"
#include "util.h"
#include "sendchar.h"
#include "eeconfig.h"
#include "backlight.h"
#include "keymap_jp.h"
#include "action_layer.h"
#include "macro_keycode.h"
#include "que_module/que_module.h"
#include "qmk_original/qmk_original.h"

bool is_ime_on = false;

uint16_t ktk(keypos_t key) {
    return keymap_key_to_keycode(layer_switch_get_layer(key), key);
}

void unenque_zenkaku(bool is_use_event_que) {
    data_t que_data[QUE_SIZE];
    que_data_initialize(is_use_event_que,que_data);
    uint8_t *que_num = que_num_initialize(is_use_event_que);

    if (*que_num > 0) {
        keyevent_t last_event = read_que_from_last(is_use_event_que, 0);
        switch (ktk(last_event.key)) {
            case KC_A:
            case KC_I:
            case KC_U:
            case KC_E:
            case KC_O:
            case KC_Z:
            case KC_K:
            case KC_X:
                que_clear(is_use_event_que);
            default:
                unenque(is_use_event_que);
        }
    }
}

void resort_event_que(void){
    data_t event_que_data[QUE_SIZE];
    que_data_initialize(true,event_que_data);
    uint8_t *event_que_head = que_head_initialize(true);
    uint8_t *event_que_num = que_num_initialize(true);

    if(*event_que_num <= 1) return;
    uint8_t que_last_v_i = *event_que_num - 1; // >= 1

    //insert prefix_key in front of former pressed_and_noprefix_event
    uint8_t i_of_first_prefix_key = 255;
    uint8_t i_of_key_for_prefixing = 255;
    for(int v_i = que_last_v_i; v_i > 0; v_i--){
        uint8_t r_i = (*event_que_head + v_i) % QUE_SIZE;
        uint8_t r_i_1 = (*event_que_head + v_i - 1) % QUE_SIZE;
        if(is_in_prefix_key(ktk(event_que_data[r_i].key)) && \
            event_que_data[r_i].pressed){
            if(i_of_first_prefix_key == 255)
                i_of_first_prefix_key = r_i;
            if((event_que_data[r_i_1].pressed) && \
                !is_in_prefix_key(ktk(event_que_data[r_i_1].key))){
                i_of_key_for_prefixing = r_i_1;
                break;
            }
        }
    }
    swap_element_in_array(event_que_data,i_of_first_prefix_key,i_of_key_for_prefixing);
}
void que_print(bool is_use_event_que, char *str) {
    data_t que_data[QUE_SIZE];
    que_data_initialize(is_use_event_que,que_data);
    uint8_t *que_head = que_head_initialize(is_use_event_que);
    uint8_t *que_num = que_num_initialize(is_use_event_que);

    udprintf("=== que_print === %32s\n", str);
//    udprintf("*que_head: %d\t", *que_head);
//    udprintf("*que_num: %d\n", *que_num);

//    udprint("real_que keycode:[");
//    for (uint8_t i = 0; i < QUE_SIZE; i++) {
//        if (i != 0) udprint(",");
//        char keycode_name[17];
//        uint8_t current_keycode = ktk(que_data[i].key);
//        keycode_val_to_name(current_keycode,keycode_name);
//        udprintf(" %s", keycode_name);
//    }
//    udprint("]\n");
//    udprint("real_que time:[");
//    for (uint8_t i = 0; i < QUE_SIZE; i++) {
//        if (i != 0) udprint(",");
//        udprintf(" %u", que_data[i].time);
//    }
//    udprint("]\n");

    udprint("virtual_que keycode:[");
    for(uint8_t i = *que_head; i < *que_head + *que_num; i++){
        if (i != *que_head) udprint(",");
        char keycode_name[17];
        uint32_t current_keycode32 = ktk(que_data[i % QUE_SIZE].key);
        udprintf(" %u:",current_keycode32);
        uint8_t current_keycode = (uint8_t)current_keycode32;
        keycode_val_to_name(current_keycode,keycode_name);
        udprintf("%s", keycode_name);
    }
    udprint("]\n");
    udprint("virtual_que time:[");
    for(uint8_t i = *que_head; i < *que_head + *que_num; i++){
        if (i != *que_head) udprint(",");
        udprintf(" %u", que_data[i % QUE_SIZE].time);
    }
    udprint("]\n");

    udprintln("================");
}

void action_exec_by_keycode(uint16_t keycode, uint16_t pressed_time, uint16_t release_time) {
    // [0]=kc_a's keypos ... [25]= kc_z's keypos [26] = kc_backspace's keypos
    static keypos_t alphabet_to_keypos[27] = {};
    static bool initial_flag = true;
    if (initial_flag) {
        for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
            for (uint8_t c = 0; c < MATRIX_COLS; c++) {
                keypos_t current_keypos = {
                        .row = r,
                        .col = c
                };
                uint16_t current_keycode = pgm_read_word(&keymaps[0][r][c]);
//                udprintf("r = %2u. c = %2u, keycode = %3u\n",r,c,current_keycode);
                switch (current_keycode) {
                    case KC_A ... KC_Z:
                        // kc_a = 0x04 ... kc_z = 0x17
                        alphabet_to_keypos[current_keycode - 0x04] = current_keypos;
                        break;
                    case KC_BSPC:
                        alphabet_to_keypos[26] = current_keypos;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    keypos_t keypos = TICK.key;
    switch (keycode) {
        case KC_A ... KC_Z:
            keypos = atk(keycode);
            break;
        case KC_BSPC:
            keypos = alphabet_to_keypos[26];
            break;
        default:
            break;
    }
//    udprintln("----------------------------");
//    for(uint8_t i=0; i<27; i++){
//        udprintf("alphabet_to_keypos[%2d] : r = %u, c = %u\n",i,alphabet_to_keypos[i].row,alphabet_to_keypos[i].col);
//    }
//    udprintln("----------------------------");
//    udprintv(keypos.col,%u);
//    udprintv(keypos.row,%u);
    action_exec((keyevent_t) {
            .time = pressed_time,
            .pressed = true,
            .key = keypos
    });
    action_exec((keyevent_t) {
            .time = release_time,
            .pressed = false,
            .key = keypos
    });

    initial_flag ^= initial_flag;
}

void action_exec_by_series_keycode(uint16_t *keycode, uint8_t num_of_keycode, uint16_t last_event_time,
                                   uint16_t current_event_time) {
    udprintln("=== enter action_exec_by_series_keycode ===");
    uint16_t pressed_times[num_of_keycode];
    uint16_t release_times[num_of_keycode];

    uint16_t time_range_divider = num_of_keycode * 2 + 1;
    for (uint8_t i = 0; i < num_of_keycode * 2; i++) {
        if (i % 2 == 1) { // odd number
            uint8_t index_for_odd = (i - 1) / 2;
            pressed_times[index_for_odd] = TIMER_RATE_16(current_event_time, last_event_time, time_range_divider, i);
        } else { // even number
            uint8_t index_for_even = i / 2 - 1;
            release_times[index_for_even] = TIMER_RATE_16(current_event_time, last_event_time, time_range_divider, i);
        }
    }

    udprintvln(num_of_keycode,%u);
    for (uint8_t i = 0; i < num_of_keycode; i++) {
        udprintvln(keycode[i],%u);
        udprintvln(pressed_times[i],%u);
        udprintvln(release_times[i],%u);
        action_exec_by_keycode(keycode[i], pressed_times[i], release_times[i]);
    }
    udprintln("=== exit action_exec_by_series_keycode ===");
}

bool is_in_prefix_key(uint16_t keycode){
    switch(keycode){
        case MO(0) ... MO(2):
        case IME_ON:
        case IME_OFF:
        case KC_LCTRL ... KC_RGUI:
        case BACKL:
            return true;

        default:
            return false;
    }
}

void keycode_val_to_name(uint16_t keycode, char *keycode_name){
    static const char keycode_val_to_name[][16] = {
            "KC_NO",
            "KC_ROLL_OVER",
            "KC_POST_FAIL",
            "KC_UNDEFINED",
            "KC_A",
            "KC_B",
            "KC_C",
            "KC_D",
            "KC_E",
            "KC_F",
            "KC_G",
            "KC_H",
            "KC_I",
            "KC_J",
            "KC_K",
            "KC_L",
            "KC_M",
            "KC_N",
            "KC_O",
            "KC_P",
            "KC_Q",
            "KC_R",
            "KC_S",
            "KC_T",
            "KC_U",
            "KC_V",
            "KC_W",
            "KC_X",
            "KC_Y",
            "KC_Z",
            "KC_1",
            "KC_2",
            "KC_3",
            "KC_4",
            "KC_5",
            "KC_6",
            "KC_7",
            "KC_8",
            "KC_9",
            "KC_0",
            "KC_ENTER",
            "KC_ESCAPE",
            "KC_BSPACE",
            "KC_TAB",
            "KC_SPACE",
            "KC_MINUS",
            "KC_EQUAL",
            "KC_LBRACKET",
            "KC_RBRACKET",
            "KC_BSLASH",
            "KC_NONUS_HASH",
            "KC_SCOLON",
            "KC_QUOTE",
            "KC_GRAVE",
            "KC_COMMA",
            "KC_DOT",
            "KC_SLASH",
            "KC_CAPSLOCK",
            "KC_F1",
            "KC_F2",
            "KC_F3",
            "KC_F4",
            "KC_F5",
            "KC_F6",
            "KC_F7",
            "KC_F8",
            "KC_F9",
            "KC_F10",
            "KC_F11",
            "KC_F12",
            "KC_PSCREEN",
            "KC_SCROLLLOCK",
            "KC_PAUSE",
            "KC_INSERT",
            "KC_HOME",
            "KC_PGUP",
            "KC_DELETE",
            "KC_END",
            "KC_PGDOWN",
            "KC_RIGHT",
            "KC_LEFT",
            "KC_DOWN",
            "KC_UP",
            "KC_NUMLOCK",
    };
    if(keycode <= 83){
        strcpy(keycode_name,keycode_val_to_name[keycode]);
    }
    else{
        strcpy(keycode_name,"____");
    }
}

// converted > return true
bool is_convert_action_event(keyevent_t action_event, bool is_ime_on) {
    udprintf("ime: %d\n", is_ime_on);
    udprintf("action_event.pressed: %d\n", action_event.pressed);
    if (is_ime_on) {
        if (action_event.pressed) {
            uint8_t current_keycode = ktk(action_event.key);
            udprintvln(current_keycode, %u);

            keyevent_t last_event = read_que_from_last(false,0);

            uint16_t l_t = last_event.time;
            uint16_t a_t = action_event.time;

            switch (current_keycode) {
                case KC_A:
                case KC_I:
                case KC_U:
                case KC_E:
                case KC_O: {
                    switch (ktk(last_event.key)) {
                        case KC_C: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_C: // ex) cca > kka
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_K,
                                            KC_K,
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    action_exec(action_event);
                                    return true;
                                }
                                default: // ex) ca > ka
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_K,
                                    };
                                    action_exec_by_series_keycode(keycode_series, 2, l_t, a_t);
                                    action_exec(action_event);
                                    return true;
                                }
                            }
                        }
                        case KC_L: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_T: {
                                    switch (current_keycode) {
                                        case KC_I: // tli > texi
                                        {
                                            uint16_t keycode_series[] = {
                                                    KC_BSPC,
                                                    KC_E,
                                                    KC_X
                                            };
                                            action_exec_by_series_keycode(keycode_series, 3, l_t, a_t);
                                            action_exec(action_event);
                                            return true;
                                        }
                                        case KC_U: // tlu > toxu
                                        {
                                            uint16_t keycode_series[] = {
                                                    KC_BSPC,
                                                    KC_O,
                                                    KC_X
                                            };
                                            action_exec_by_series_keycode(keycode_series, 3, l_t, a_t);
                                            action_exec(action_event);
                                            return true;
                                        }
                                        default:
                                            return false;
                                    }
                                }
                                case KC_D: {
                                    switch (current_keycode) {
                                        case KC_I: { // dli > dexi
                                            uint16_t keycode_series[] = {
                                                    KC_BSPC,
                                                    KC_E,
                                                    KC_X
                                            };
                                            action_exec_by_series_keycode(keycode_series, 3, l_t, a_t);
                                            action_exec(action_event);
                                            return true;
                                        }
                                        default:
                                            return false;
                                    }
                                }
                                case KC_L: // ex) lla > zza
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Z,
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    action_exec(action_event);
                                    return true;
                                }

                                default: // ex) la > za
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_Z
                                    };
                                    action_exec_by_series_keycode(keycode_series, 2, l_t, a_t);
                                    action_exec(action_event);
                                    return true;
                                }
                            }
                        }
                        default:
                            return false;
                    }
                }
                case KC_Z: // z > ya
                {
                    switch (ktk(last_event.key)) {
                        case KC_C: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_C: // ex) ccz > kkya
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_K,
                                            KC_K,
                                            KC_Y,
                                            KC_A
                                    };
                                    action_exec_by_series_keycode(keycode_series, 6, l_t, a_t);
                                    return true;
                                }
                                default: // ex) cz > kya
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_K,
                                            KC_Y,
                                            KC_A
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    return true;
                                }
                            }
                        }
                        case KC_L: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_L: // ex) llz > zzya
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Z,
                                            KC_Y,
                                            KC_A
                                    };
                                    action_exec_by_series_keycode(keycode_series, 6, l_t, a_t);
                                    return true;
                                }
                                default: // ex) lz > zya
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Y,
                                            KC_A
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    return true;
                                }
                            }
                        }
                        default: {
                            uint16_t keycode_series[] = {
                                    KC_Y,
                                    KC_A
                            };
                            action_exec_by_series_keycode(keycode_series, 2, l_t, a_t);
                            return true;
                        }
                    }

                }
                case KC_K: // k > yu
                {
                    switch (ktk(last_event.key)) {
                        case KC_C: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_C: // ex) cck > kkyu
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_K,
                                            KC_K,
                                            KC_Y,
                                            KC_U
                                    };
                                    action_exec_by_series_keycode(keycode_series, 6, l_t, a_t);
                                    return true;
                                }
                                default: // ex) ck > kyu
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_K,
                                            KC_Y,
                                            KC_U
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    return true;
                                }
                            }
                        }
                        case KC_L: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_L: // ex) llk > zzyu
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Z,
                                            KC_Y,
                                            KC_U
                                    };
                                    action_exec_by_series_keycode(keycode_series, 6, l_t, a_t);
                                    return true;
                                }
                                default: // ex) lk > zyu
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Y,
                                            KC_U
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    return true;
                                }
                            }
                        }
                        default: {
                            uint16_t keycode_series[] = {
                                    KC_Y,
                                    KC_U
                            };
                            action_exec_by_series_keycode(keycode_series, 2, l_t, a_t);
                            return true;
                        }
                    }

                }
                case KC_Q: // q > yo
                {
                    switch (ktk(last_event.key)) {
                        case KC_C: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_C: // ex) ccq > kkyo
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_K,
                                            KC_K,
                                            KC_Y,
                                            KC_O
                                    };
                                    action_exec_by_series_keycode(keycode_series, 6, l_t, a_t);
                                    return true;
                                }
                                default: // ex) cq > kyo
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_K,
                                            KC_Y,
                                            KC_O
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    return true;
                                }
                            }
                        }
                        case KC_L: {
                            keyevent_t last2_event = read_que_from_last(false, 1);
                            switch (ktk(last2_event.key)) {
                                case KC_L: // ex) llq > zzyo
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Z,
                                            KC_Y,
                                            KC_O
                                    };
                                    action_exec_by_series_keycode(keycode_series, 6, l_t, a_t);
                                    return true;
                                }
                                default: // ex) lq > zyo
                                {
                                    uint16_t keycode_series[] = {
                                            KC_BSPC,
                                            KC_Z,
                                            KC_Y,
                                            KC_O
                                    };
                                    action_exec_by_series_keycode(keycode_series, 4, l_t, a_t);
                                    return true;
                                }
                            }
                        }
                        default: {
                            uint16_t keycode_series[] = {
                                    KC_Y,
                                    KC_O
                            };
                            action_exec_by_series_keycode(keycode_series, 2, l_t, a_t);
                            return true;
                        }
                    }

                }
                default:
                    return false;

            }
        }
    }
    return false;
}

/** \brief keyboard set leds
 *
 * FIXME: needs doc
 */
void keyboard_set_leds(uint8_t leds) {
    if (debug_keyboard) {
        debug("keyboard_set_led: ");
        debug_hex8(leds);
        debug("\n");
    }
    led_set(leds);
}

