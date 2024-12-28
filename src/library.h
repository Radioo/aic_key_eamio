#ifndef AIC_KEY_EAMIO_LIBRARY_H
#define AIC_KEY_EAMIO_LIBRARY_H

#include <stdint.h>

int initialize(void* ctx);
void process_card_slot_cmd(uint8_t unit_no, uint8_t cmd);

#endif //AIC_KEY_EAMIO_LIBRARY_H