#include "library.h"
#include "aic.h"
#include "bemanitools/eamio.h"

#include <stdio.h>
#include <curl/curl.h>

log_formatter_t misc_logger;
log_formatter_t info_logger;
log_formatter_t warning_logger;
log_formatter_t fatal_logger;

thread_create_t create_thread;
thread_join_t join_thread;
thread_destroy_t destroy_thread;

__declspec(dllexport) void eam_io_set_loggers(
    const log_formatter_t misc,
    const log_formatter_t info,
    const log_formatter_t warning,
    const log_formatter_t fatal
) {
    misc_logger = misc;
    info_logger = info;
    warning_logger = warning;
    fatal_logger = fatal;
}

__declspec(dllexport) bool eam_io_init(
    const thread_create_t thread_create,
    const thread_join_t thread_join,
    const thread_destroy_t thread_destroy
) {
    create_thread = thread_create;
    join_thread = thread_join;
    destroy_thread = thread_destroy;

    create_thread(initialize, NULL, 0, 0);

    return true;
}

__declspec(dllexport) void eam_io_fini(void) {
    misc_logger("aic_key_eamio", "Shutting down library");
}

__declspec(dllexport) uint16_t eam_io_get_keypad_state(uint8_t unit_no) {
    return get_keypad_state(unit_no);
}

__declspec(dllexport) uint8_t eam_io_get_sensor_state(uint8_t unit_no) {
    // misc_logger("eam_io_get_sensor_state", "unit_no: %d", unit_no);
    return 0;
}

__declspec(dllexport) uint8_t eam_io_read_card(uint8_t unit_no, uint8_t *card_id, uint8_t nbytes) {
    // misc_logger("eam_io_read_card", "unit_no: %d, nbytes: %d", unit_no, nbytes);
    return 0;
}

__declspec(dllexport) bool eam_io_card_slot_cmd(uint8_t unit_no, uint8_t cmd) {
    // misc_logger("eam_io_card_slot_cmd", "unit_no: %d, cmd: %d", unit_no, cmd);
    return false;
}

__declspec(dllexport) bool eam_io_poll(uint8_t unit_no) {
    return true;
}

__declspec(dllexport) const struct eam_io_config_api* eam_io_get_config_api(void) {
    // misc_logger("eam_io_get_config_api", "Returning NULL");
    return NULL;
}

int initialize(void* ctx) {
    misc_logger("aic_key_eamio", "Initializing library");
    init(misc_logger);
    return 0;
}