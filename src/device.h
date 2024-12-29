#pragma once

#include <windows.h>
#include <setupapi.h>
#include <stdbool.h>

typedef struct device_details {
    int vid;
    int pid;
    int mi;
} device_details_t;

typedef struct device {
    CHAR location[100];
    WCHAR siblings[1000];
    DWORD siblings_length;
    TCHAR cardio_path[MAX_PATH];
    CHAR keypad_identifier[MAX_PATH];
} device_t;

typedef struct parsed_device {
    WCHAR id[100];
    CHAR location[100];
    WCHAR parent[100];
    WCHAR siblings[1000];
    TCHAR cardio_path[MAX_PATH];
    CHAR keypad_identifier[MAX_PATH];
} parsed_device_t;

device_t* get_devices(int vid, int pid, int mi, int* device_count);
HDEVINFO get_device_info();
device_details_t* get_device_details(LPWSTR device_id);
DWORD get_required_property_size(HDEVINFO devices, PSP_DEVINFO_DATA device_info_data, const DEVPROPKEY* property_key);
LPWSTR get_string_property(HDEVINFO devices, PSP_DEVINFO_DATA device_info_data, const DEVPROPKEY* property_key, DWORD* size_out);
device_t* init_device_t();
BOOL device_location_exists(PSTR location, const device_t* devices, int device_count);
device_t* add_device(device_t* devices, int* device_count, PSTR location);
void add_siblings(device_t* devices, int device_count, PSTR location, PWSTR siblings, DWORD siblings_length);
void add_cardio_path(device_t* devices, int device_count, PTSTR cardio_path, PWSTR cardio_parent);
void add_keypad_path(device_t* devices, int device_count, PTSTR keypad_path, PWSTR keypad_parent);
BOOL validate_device(const char* device_path, int vid, int pid, int mi, bool capitalized);
void init_parsed_device(parsed_device_t* parsed_device);
device_t* group_devices(parsed_device_t* parsed_devices, int parsed_device_count, int* device_count);