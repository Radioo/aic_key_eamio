#include "device.h"
#include "bemanitools/glue.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

#define KBD_DEVICE_USAGE_KEYBOARD 0x00010006
#define KBD_DEVICE_USAGE_KEYPAD 0x00010007

static const int VID = 0xCAFF;
static const int PID = 0x400E;
static const int CARDIO_MI = 0;
static const int KEYPAD_MI = 1;

static HWND window = NULL;
static uint16_t* keypad_states;
static WNDPROC orig_proc = NULL;

device_t* devices;
int device_count = 0;
log_formatter_t misc_logger;

uint16_t get_keypad_state(int device_index) {
    return keypad_states[device_index];
}

void update_keypad_state(int keypad_index, USHORT key, USHORT flags) {
    uint8_t bitmap_index = 0;

    switch(key) {
        case 45:
            bitmap_index = 0;
        break;
        case 109:
            bitmap_index = 8;
        break;
        case 46:
            bitmap_index = 4;
        break;
        case 35:
            bitmap_index = 1;
        break;
        case 40:
            bitmap_index = 5;
        break;
        case 34:
            bitmap_index = 9;
        break;
        case 37:
            bitmap_index = 2;
        break;
        case 12:
            bitmap_index = 6;
        break;
        case 39:
            bitmap_index = 10;
        break;
        case 36:
            bitmap_index = 3;
        break;
        case 38:
            bitmap_index = 7;
        break;
        case 33:
            bitmap_index = 11;
        break;
        default:
            return;
    }

    uint8_t depressed = flags & RI_KEY_BREAK ? 0 : 1;
    keypad_states[keypad_index] = (keypad_states[keypad_index] & ~(1 << bitmap_index)) | (depressed << bitmap_index);
    printf("Keypad state: %d\n", keypad_states[keypad_index]);
}

bool get_parent_device_id(const char* device_name, char* parent_id, size_t parent_id_size) {
    HDEVINFO device_info_set = SetupDiGetClassDevs(NULL, device_name, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (device_info_set == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed, error: %d\n", GetLastError());
        return false;
    }

    SP_DEVINFO_DATA device_info_data = {0};
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiEnumDeviceInfo(device_info_set, 0, &device_info_data)) {
        printf("SetupDiEnumDeviceInfo failed, error: %d\n", GetLastError());
        SetupDiDestroyDeviceInfoList(device_info_set);
        return false;
    }

    DEVPROPTYPE property_type;
    if (!SetupDiGetDevicePropertyW(
            device_info_set,
            &device_info_data,
            &DEVPKEY_Device_Parent,
            &property_type,
            (PBYTE)parent_id,
            (DWORD)parent_id_size,
            NULL,
            0)) {
        printf("SetupDiGetDeviceProperty failed, error: %d\n", GetLastError());
        SetupDiDestroyDeviceInfoList(device_info_set);
        return false;
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return true;
}

DWORD WINAPI scan(const LPVOID param) {
    const int device_id = (int)param;

    HANDLE file = CreateFile(
        devices[device_id].cardio_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if(file == INVALID_HANDLE_VALUE) {
        printf("Failed to open device %d, error: %d\n", device_id, GetLastError());
    }

    while(true) {
        uint8_t buffer[100] = {0};
        DWORD bytesRead;
        if(!ReadFile(file, buffer, sizeof(buffer), &bytesRead, NULL)) {
            printf("Failed to read from device %d, error: %d\n", device_id, GetLastError());
        }

        printf("Device %d [%d bytes]: Card type: %d\n", device_id, bytesRead, buffer[0]);
        for(auto j = 1; j < bytesRead; j++) {
            printf("%02X", buffer[j]);
        }
        putchar('\n');

        Sleep(100);
    }

    return EXIT_SUCCESS;
}

void ProcessRawInput(LPARAM lParam) {
    RAWINPUT rawInput;
    UINT dwSize;

    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    BYTE* lpb = (BYTE*)malloc(dwSize);

    if (lpb == NULL) {
        printf("Memory allocation failed\n");
        goto END;
    }

    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
        printf("GetRawInputData size mismatch\n");
        goto END;
    }

    const RAWINPUT* raw = (RAWINPUT*)lpb;

    if (raw->header.dwType != RIM_TYPEKEYBOARD) {
        goto END;
    }

    UINT name_size;
    GetRawInputDeviceInfoA(raw->header.hDevice, RIDI_DEVICENAME, NULL, &name_size);
    char* device_name = malloc(name_size);

    if (!GetRawInputDeviceInfoA(raw->header.hDevice, RIDI_DEVICENAME, device_name, &name_size) > 0) {
        goto END;
    }

    int device_index = -1;
    for(int i = 0; i < device_count; i++) {
        PCHAR keypad_match = strstr(device_name, devices[i].keypad_identifier);
        if(keypad_match == NULL) {
            continue;
        }

        device_index = i;
    }

    if(device_index == -1) {
        goto END;
    }

    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        update_keypad_state(device_index, raw->data.keyboard.VKey, raw->data.keyboard.Flags);
    }

    END:
    if(device_name) {
        free(device_name);
    }
    if(lpb) {
        free(lpb);
    }
}

LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // MessageBox(NULL, "Message received", "Message", MB_OK);
    if (uMsg == WM_INPUT) {
        LARGE_INTEGER freq, start, end;
        double time;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        ProcessRawInput(lParam);
        QueryPerformanceCounter(&end);

        misc_logger("aic-time", "Time taken: %f", (double)(end.QuadPart - start.QuadPart) / freq.QuadPart);
    }

    if(orig_proc) {
        return orig_proc(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI setup_keypad(LPVOID param) {
    window = FindWindow(
        "msg-thread",
        NULL
    );

    if(!window) {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = HiddenWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "aic-key-input";

        if(!RegisterClass(&wc)) {
            printf("Failed to register window class\n");
            return EXIT_FAILURE;
        }

        window = CreateWindow(
            wc.lpszClassName,
            "aic-key-input-window",
            WS_MINIMIZE,
            CW_USEDEFAULT, 0,
            CW_USEDEFAULT, 0,
            HWND_MESSAGE,
            NULL,
            wc.hInstance,
            NULL
        );
    }
    else {
        HINSTANCE geninput_dll = GetModuleHandle("geninput.dll");
        if(!geninput_dll) {
            MessageBox(window, "Failed to get geninput.dll handle", "Error", MB_OK);
            return EXIT_FAILURE;
        }

        // Get thread id from window
        DWORD thread_id = GetWindowThreadProcessId(window, NULL);
        if(!thread_id) {
            MessageBox(window, "Failed to get thread id", "Error", MB_OK);
        }

        if(!SetWindowsHookEx(
            WH_GETMESSAGE,
            (HOOKPROC)HiddenWndProc,
            geninput_dll,
            thread_id
        )) {
            MessageBox(window, "Failed to set hook", "Error", MB_OK);
            return EXIT_FAILURE;
        }

        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        if(!GetClassInfoEx(geninput_dll, "msg-thread", &wcex)) {
            MessageBox(window, "Failed to get class info", "Error", MB_OK);
            return EXIT_FAILURE;
        }

        orig_proc = wcex.lpfnWndProc;
        SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)HiddenWndProc);
        if (!orig_proc) {
            MessageBox(window, "Failed to set window procedure", "Error", MB_OK);
            return EXIT_FAILURE;
        }
    }

    // if(!window) {
    //     printf("Failed to create window\n");
    //     exit(EXIT_FAILURE);
    //     return EXIT_FAILURE;
    // }

    RAWINPUTDEVICE filter[2];
    filter[0].usUsagePage = KBD_DEVICE_USAGE_KEYBOARD >> 16;
    filter[0].usUsage = (uint16_t) KBD_DEVICE_USAGE_KEYBOARD;
    filter[0].dwFlags = RIDEV_INPUTSINK;
    filter[0].hwndTarget = window;

    filter[1].usUsagePage = KBD_DEVICE_USAGE_KEYPAD >> 16;
    filter[1].usUsage = (uint16_t) KBD_DEVICE_USAGE_KEYPAD;
    filter[1].dwFlags = RIDEV_INPUTSINK;
    filter[1].hwndTarget = window;

    if(!RegisterRawInputDevices(filter, 2, sizeof(filter[0]))) {
        printf("Failed to register raw input devices\n");
        return EXIT_FAILURE;
    }

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return EXIT_SUCCESS;
}

int init() {
    device_count = 0;
    devices = get_devices(VID, PID, CARDIO_MI, &device_count);

    if(device_count == 0) {
        printf("No devices found\n");
        return 0;
    }

    keypad_states = malloc(device_count * sizeof(uint16_t));

    printf("Devices found: %d\n", device_count);
    for(int i = 0; i < device_count; i++) {
        keypad_states[i] = 0;

        printf("Device %d:\n", i);
        printf("Location: %s\n", devices[i].location);

        printf("CardIO device path: %s\n", devices[i].cardio_path);
        printf("Keypad device path: %s\n", devices[i].keypad_identifier);

        putchar('\n');
    }

    HANDLE cardio_threads[2];
    for(int i = 0; i < device_count; i++) {
        cardio_threads[i] = CreateThread(
            NULL,
            0,
            scan,
            (LPVOID)i,
            0,
            NULL
        );
    }

    HANDLE keypad_thread = CreateThread(
        NULL,
        0,
        setup_keypad,
        NULL,
        0,
        NULL
    );

    for(int i = 0; i < device_count; i++) {
        WaitForSingleObject(cardio_threads[i], INFINITE);
    }

    return 0;
}