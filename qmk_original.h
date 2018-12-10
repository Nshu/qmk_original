//
// Created by abata on 2018/12/10.
//

#ifndef QMK_FIRMWARE2_QMK_ORIGINAL_H
#define QMK_FIRMWARE2_QMK_ORIGINAL_H

#define atk(keycode)    alphabet_to_keypos[keycode - 0x04]
#define bspc_keypos     alphabet_to_keypos[26]

uint16_t ktk(keypos_t key) ;
void keycode_val_to_name(uint16_t keycode, char *keycode_name);
void que_print(bool is_use_event_que, char *str);
void action_exec_by_keycode(uint16_t keycode, uint16_t pressed_time, uint16_t release_time) ;
void action_exec_by_series_keycode(uint16_t *keycode, uint8_t num_of_keycode, uint16_t last_event_time,
                                   uint16_t current_event_time) ;
bool is_convert_action_event(keyevent_t action_event, bool is_ime_on);
bool is_in_prefix_key(uint16_t keycode);
void unenque_zenkaku(bool is_use_event_que);
void resort_event_que(void);


#endif //QMK_FIRMWARE2_QMK_ORIGINAL_H
