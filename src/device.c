#include "device.h"
#include "log.h"

#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <hidsdi.h>
#include <tchar.h>
#include <devpkey.h>
#include <stdio.h>
#include <stdbool.h>
#include <hidclass.h>

static GUID hidclass_guid = {0x745a17a0, 0x74d3, 0x11d0, {0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda}};

void list_device_descriptions(HDEVINFO devices) {
    if (devices == INVALID_HANDLE_VALUE) {
        printf("Failed to get device information set.\n");
        return;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    DWORD deviceIndex = 0;

    while (SetupDiEnumDeviceInfo(devices, deviceIndex, &devInfoData)) {
        deviceIndex++;

        DWORD required_size = 0;
        DEVPROPTYPE propertyType;
        BOOL result = SetupDiGetDevicePropertyW(
            devices,
            &devInfoData,
            &DEVPKEY_Device_BusReportedDeviceDesc,
            &propertyType,
            NULL,
            0,
            &required_size,
            0
        );
        if(!result) {
            log("Failed to get required size for device %lu, error: %d\n", deviceIndex, GetLastError());
        }
        else {
            log("Size required for device %lu: %lu\n", deviceIndex, required_size);
        }

        WCHAR deviceDesc[1024];
        DWORD requiredSize = 0;


        // Initialize memory to avoid alignment issues
        ZeroMemory(deviceDesc, sizeof(deviceDesc));

        // Query the DEVPKEY_Device_BusReportedDeviceDesc property
        if (SetupDiGetDevicePropertyW(devices, &devInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
                                      &propertyType, (PBYTE)deviceDesc, sizeof(deviceDesc), 0, 0)) {
            wprintf(L"Device %lu: %ls\n", deviceIndex, deviceDesc);
                                      } else {
                                          DWORD error = GetLastError();
                                          if (error == ERROR_INSUFFICIENT_BUFFER) {
                                              wprintf(L"Device %lu: Buffer size insufficient (required: %lu bytes).\n", deviceIndex, requiredSize);
                                          } else if (error == ERROR_NOT_FOUND) {
                                              wprintf(L"Device %lu: Description not available.\n", deviceIndex);
                                          } else {
                                              wprintf(L"Device %lu: Failed to get description, error: %lu\n", deviceIndex, error);
                                          }
                                      }
    }

    DWORD lastError = GetLastError();
    if (lastError != ERROR_NO_MORE_ITEMS) {
        printf("Error during device enumeration: %lu\n", lastError);
    }

    SetupDiDestroyDeviceInfoList(devices);
}

device_t* get_devices(int vid, int pid, int mi, int* device_count) {
    const int MAX_DEVICES = 2;
    const WCHAR* cardio_name = L"AIC Pico CardIO";
    GUID guid;
    HidD_GetHidGuid(&guid);
    const HDEVINFO devices = get_device_info();
    if(devices == NULL) {
        return NULL;
    }

    device_t* output = NULL;
    *device_count = 0;

    DWORD device_index = 0;
    SP_DEVINFO_DATA device_info_data;
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    while(SetupDiEnumDeviceInfo(devices, device_index, &device_info_data)) {
        device_index++;
        device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

        LPWSTR device_id = get_string_property(devices, &device_info_data, &DEVPKEY_Device_InstanceId, NULL);
        if(device_id == NULL) {
            continue;
        }

        device_details_t* details = get_device_details(device_id);
        if(details == NULL || details->vid != vid || details->pid != pid) {
            continue;
        }

        log("Device ID: %ls\n", device_id);

        DWORD required_size = 0;
        SetupDiGetDeviceRegistryProperty(
            devices,
            &device_info_data,
            SPDRP_LOCATION_INFORMATION,
            NULL,
            NULL,
            0,
            &required_size
        );
        if(required_size != 0) {
            LPSTR location_information = malloc(required_size);
            if(SetupDiGetDeviceRegistryProperty(
                devices,
                &device_info_data,
                SPDRP_LOCATION_INFORMATION,
                NULL,
                (PBYTE)location_information,
                required_size,
                NULL
            )) {
                if(!device_location_exists(location_information, output, *device_count)) {
                    output = add_device(output, device_count, location_information);
                }

                log("Location information: %s\n", location_information);

                DWORD sibling_size = 0;
                LPWSTR siblings = get_string_property(devices, &device_info_data, &DEVPKEY_Device_Siblings, &sibling_size);
                if(siblings != NULL) {
                    add_siblings(output, *device_count, location_information, siblings, sibling_size);
                    log("Siblings: ");
                    for(int i = 0; i < sibling_size / sizeof(WCHAR); i++) {
                        putwchar(siblings[i]);
                    }
                    putchar('\n');
                }
            }
        }

        LPWSTR parent = get_string_property(devices, &device_info_data, &DEVPKEY_Device_Parent, NULL);
        if(parent != NULL) {
            log("Parent: %ls\n", parent);
        }

        DWORD children_size = 0;
        LPWSTR children = get_string_property(devices, &device_info_data, &DEVPKEY_Device_Children, &children_size);
        if(children != NULL) {
            log("Children: ");
            for(int i = 0; i < children_size / sizeof(WCHAR); i++) {
                putwchar(children[i]);
            }
            putchar('\n');
            free(children);
        }

        DWORD interface_index = 0;
        SP_DEVICE_INTERFACE_DATA device_interface_data;
        device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        while(SetupDiEnumDeviceInterfaces(devices, &device_info_data, &guid, interface_index, &device_interface_data)) {
            interface_index++;

            required_size = 0;
            SetupDiGetDeviceInterfaceDetail(devices, &device_interface_data, NULL, 0, &required_size, NULL);
            if(required_size == 0) {
                log_windows_error("SetupDiGetDeviceInterfaceDetail failed", GetLastError());
                continue;
            }

            PSP_INTERFACE_DEVICE_DETAIL_DATA interface_detail_data = malloc(required_size);
            interface_detail_data->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
            if(!SetupDiGetDeviceInterfaceDetail(devices, &device_interface_data, interface_detail_data, required_size, NULL, NULL)) {
                log_windows_error("SetupDiGetDeviceInterfaceDetail failed", GetLastError());
                free(interface_detail_data);
                continue;
            }

            log("Device path: %s\n", interface_detail_data->DevicePath);

            if(details->mi == 0) {
                add_cardio_path(output, *device_count, interface_detail_data->DevicePath, parent);
            }
            else if(details->mi == 1) {
                add_keypad_path(output, *device_count, interface_detail_data->DevicePath, parent);
            }
        }

        if(interface_index == 0) {
            log_windows_error("SetupDiEnumDeviceInterfaces failed", GetLastError());
        }

        putchar('\n');
    }

    return output;
}

HDEVINFO get_device_info() {
    GUID guid;
    HidD_GetHidGuid(&guid);
    const HDEVINFO devices = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE);
    if(devices == INVALID_HANDLE_VALUE) {
        log_windows_error("SetupDiGetClassDevs failed", GetLastError());
        return NULL;
    }

    return devices;
}

device_details_t* get_device_details(const LPWSTR device_id) {
    const LPWSTR uppercase_device_id = _wcsdup(device_id);
    _wcsupr_s(uppercase_device_id, wcslen(uppercase_device_id) + 1);

    const LPWSTR vid_pos = wcsstr(uppercase_device_id, L"VID_");
    const LPWSTR pid_pos = wcsstr(uppercase_device_id, L"PID_");
    const LPWSTR mi_pos = wcsstr(uppercase_device_id, L"MI_");

    if(vid_pos == NULL || pid_pos == NULL || mi_pos == NULL) {
        free(uppercase_device_id);
        return NULL;
    }

    device_details_t* details = malloc(sizeof(device_details_t));
    details->vid = wcstol(vid_pos + 4, NULL, 16);
    details->pid = wcstol(pid_pos + 4, NULL, 16);
    details->mi = wcstol(mi_pos + 3, NULL, 16);

    free(uppercase_device_id);

    return details;
}

DWORD get_required_property_size(const HDEVINFO devices, PSP_DEVINFO_DATA device_info_data, const DEVPROPKEY* property_key) {
    DEVPROPTYPE property_type;
    DWORD required_size = 0;

    SetupDiGetDevicePropertyW(
        devices,
        device_info_data,
        property_key,
        &property_type,
        NULL,
        0,
        &required_size,
        0
    );

    if(required_size == 0) {
        DWORD error = GetLastError();
        if(error != ERROR_NOT_FOUND) {
            log_windows_error("SetupDiGetDevicePropertyW for required size failed", GetLastError());
        }

        return 0;
    }

    return required_size;
}

LPWSTR get_string_property(const HDEVINFO devices, PSP_DEVINFO_DATA device_info_data, const DEVPROPKEY* property_key, DWORD* size_out) {
    const DWORD size = get_required_property_size(devices, device_info_data, property_key);
    if(size == 0) {
        return NULL;
    }

    DEVPROPTYPE property_type;
    LPWSTR buffer = malloc(size);

    const BOOL result = SetupDiGetDevicePropertyW(
        devices,
        device_info_data,
        property_key,
        &property_type,
        (PBYTE)buffer,
        size,
        NULL,
        0
    );

    if(!result) {
        log_windows_error("SetupDiGetDevicePropertyW for string property failed", GetLastError());
        free(buffer);
        return NULL;
    }

    if(size_out != NULL) {
        *size_out = size;
    }

    return buffer;
}

device_t* init_device_t() {
    device_t* device = malloc(sizeof(device_t));
    device->siblings_length = 0;
    memset(device->siblings, 0, sizeof(device->siblings));
    memset(device->location, 0, sizeof(device->location));
    memset(device->cardio_path, 0, sizeof(device->cardio_path));
    memset(device->keypad_identifier, 0, sizeof(device->keypad_identifier));

    return device;
}

BOOL device_location_exists(PSTR location, const device_t* devices, const int device_count) {
    for(int i = 0; i < device_count; i++) {
        if(strcmp(location, devices[i].location) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

device_t* add_device(device_t* devices, int* device_count, PSTR location) {
    if(devices == NULL) {
        devices = init_device_t();
        memcpy(devices->location, location, strlen(location) + 1);
        (*device_count)++;
        return devices;
    }

    devices = realloc(devices, (*device_count + 1) * sizeof(device_t));
    devices[*device_count] = *init_device_t();
    memcpy(devices[*device_count].location, location, strlen(location) + 1);
    (*device_count)++;
    return devices;
}

void add_siblings(device_t* devices, int device_count, PSTR location, PWSTR siblings, DWORD siblings_length) {
    for(int i = 0; i < device_count; i++) {
        if(strcmp(devices[i].location, location) != 0) {
            continue;
        }

        if(devices[i].siblings_length == 0) {
            memcpy(devices[i].siblings, siblings, siblings_length);
            devices[i].siblings_length = siblings_length;
        }
        else {
            memcpy((CHAR*)devices[i].siblings + devices[i].siblings_length, siblings, siblings_length);
            devices[i].siblings_length += siblings_length;
        }

        // Replace all null bytes with spaces
        for(int j = 0; j < devices[i].siblings_length / sizeof(WCHAR); j++) {
            if(devices[i].siblings[j] == '\0') {
                devices[i].siblings[j] = ' ';
            }
        }
    }
}

void add_cardio_path(device_t* devices, int device_count, PTSTR cardio_path, PWSTR cardio_parent) {
    for(int i = 0; i < device_count; i++) {
        WCHAR* parent_loc = wcsstr(devices[i].siblings, cardio_parent);
        if(parent_loc == NULL) {
            continue;
        }

        memcpy(devices[i].cardio_path, cardio_path, strlen(cardio_path) + 1);
    }
}

void add_keypad_path(device_t* devices, int device_count, PTSTR keypad_path, PWSTR keypad_parent) {
    for(int i = 0; i < device_count; i++) {
        WCHAR* parent_loc = wcsstr(devices[i].siblings, keypad_parent);
        if(parent_loc == NULL) {
            continue;
        }

        PCHAR mi_pos = strstr(keypad_path, "&mi_01#");
        if(mi_pos == NULL) {
            continue;
        }

        PCHAR bracket_pos = strchr(keypad_path, '{');
        if(bracket_pos == NULL) {
            continue;
        }
        *--bracket_pos = '\0';

        strcpy(devices[i].keypad_identifier, mi_pos + 7);
    }
}

BOOL validate_device(const char* device_path, const int vid, const int pid, const int mi, const bool capitalized) {
    if (capitalized) {
        const char* vid_pos = strstr(device_path, "VID_");
        if(vid_pos == NULL) {
            return FALSE;
        }
        const int parsed_vid = strtol(vid_pos + 4, NULL, 16);

        const char* pid_pos = strstr(device_path, "PID_");
        if(pid_pos == NULL) {
            return FALSE;
        }
        const int parsed_pid = strtol(pid_pos + 4, NULL, 16);

        const char* mi_pos = strstr(device_path, "MI_");
        if(mi_pos == NULL) {
            return FALSE;
        }
        const int parsed_mi = strtol(mi_pos + 3, NULL, 16);

        return parsed_vid == vid && parsed_pid == pid && parsed_mi == mi;
    }
    else {
        const char* vid_pos = strstr(device_path, "vid_");
        if(vid_pos == NULL) {
            return FALSE;
        }
        const int parsed_vid = strtol(vid_pos + 4, NULL, 16);

        const char* pid_pos = strstr(device_path, "pid_");
        if(pid_pos == NULL) {
            return FALSE;
        }
        const int parsed_pid = strtol(pid_pos + 4, NULL, 16);

        const char* mi_pos = strstr(device_path, "mi_");
        if(mi_pos == NULL) {
            return FALSE;
        }
        const int parsed_mi = strtol(mi_pos + 3, NULL, 16);

        return parsed_vid == vid && parsed_pid == pid && parsed_mi == mi;
    }
}
