#pragma once

#include <stdint.h>
#include <stdbool.h>

void init();
uint16_t get_keypad_state(int device_index);
uint8_t get_reader_bytes(uint8_t unit_no, uint8_t* input);
bool get_reader_state(uint8_t unit_no);