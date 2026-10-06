#define COBJMACROS

#include "main.h"
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include <stdio.h>
#include <uiautomation.h>

void wnd_array_add(Vector_Window *arr, Window *handle);
BOOL CALLBACK enum_windows_proc(HWND handle, LPARAM l_param);
void position_windows(Vector_Window *arr);

void init(Wm_params *wm_params) {
  wm_params->active_workspace = 1;
  wm_params->windows = malloc(sizeof(Vector_Window));

  wm_params->windows->size = 0;
  wm_params->windows->capacity = 0;
  wm_params->windows->data = NULL;

  EnumWindows(enum_windows_proc, (LPARAM)wm_params->windows);
}

void main(void) {
  // setlocale(LC_ALL, "");
  // SetConsoleOutputCP(CP_UTF8);
  // _setmode(_fileno(stdout), _O_U16TEXT);
  Wm_params wm_params = {0};
  init(&wm_params);

  Vector_Window *windows = wm_params.windows;
  position_windows(windows);

  free(windows);
}

void position_windows(Vector_Window *windows) {
  HDWP hdwp = BeginDeferWindowPos(windows->size);
  if (hdwp == NULL) {
    printf("Failed\n");
    return;
  }

  printf("Windows num: %zu\n", windows->size);

  for (size_t i = 0; i < windows->size; i++) {
    HWND wnd = windows->data[i].handle;
    // DWORD p_id = 0;
    // GetWindowThreadProcessId(wnd, &p_id);
    // printf("window %zu: %d\n", i, p_id);

    if (IsZoomed(wnd)) {
      ShowWindow(wnd, SW_RESTORE);
    }

    HDWP def_hdpw = DeferWindowPos(hdwp, wnd, NULL, 10, 10, 400, 400,
                                   SWP_NOZORDER | SWP_NOACTIVATE);

    if (def_hdpw == NULL) {
      printf("Failed: %lu\n", GetLastError());
      return;
    }
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed\n");
  }
}

void wnd_array_add(Vector_Window *arr, Window *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1) * sizeof(HWND);
    arr->data = realloc(arr->data, arr->capacity);
  }

  arr->data[arr->size] = *el;
  arr->size += 1;
}

void extract_exe_name(char *path, DWORD p_l, char *name) {
  size_t last_slash = -1;

  for (size_t i = 0; i < p_l; i++) {
    if (path[i] == '\\') {
      last_slash = i;
    }
  }

  size_t name_l = p_l - 1 - last_slash;

  for (size_t i = 0; i < name_l; i++) {
    name[i] = path[last_slash + 1 + i];
  }

  name[name_l] = '\0';
}

void get_process_name_by_window(HWND h_wnd, char *p_name) {
  DWORD p_id = 0;
  if (!GetWindowThreadProcessId(h_wnd, &p_id)) {
    return;
  }

  HANDLE h_p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p_id);
  char p_full_path[MAX_EXE_PATH_LENGTH];
  DWORD p_path_l = MAX_EXE_PATH_LENGTH;

  if (!QueryFullProcessImageNameA(h_p, 0, p_full_path, &p_path_l)) {
    return;
  }

  extract_exe_name(p_full_path, p_path_l, p_name);
}

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param) {
  Vector_Window *windows = (Vector_Window *)l_param;
  wchar_t buff[255];

  if (IsWindowVisible(h_wnd)) {
    Window *window = malloc(sizeof(Window));
    // HMONITOR monitor = MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);

    window->handle = h_wnd;

    char *p_name = malloc(sizeof(char) * MAX_EXE_PATH_LENGTH);
    get_process_name_by_window(h_wnd, p_name);

    window->process_name = p_name;

    printf("%s\n", window->process_name);

    wnd_array_add(windows, window);
  }

  return TRUE;
}

// IUIAutomation *g_pAutomation = NULL;

// void enumerate_root_children(IUIAutomationElement *pRootElement, int depth);
// void find_all_windows(IUIAutomationElement *pRootElement);

// int main()
// {
//     SetConsoleOutputCP(CP_UTF8);
//     _setmode(_fileno(stdout), _O_U16TEXT);

//     HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

//     if (hr != S_OK)
//     {
//         printf("Failed to initialize COM. Error: %d\n", hr);
//         return 1;
//     }

//     hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
//     &IID_IUIAutomation, &g_pAutomation);

//     if (SUCCEEDED(hr) && g_pAutomation != NULL)
//     {
//         IUIAutomationElement *pRootElement = NULL;

//         hr = IUIAutomation_GetRootElement(g_pAutomation, &pRootElement);

//         if (SUCCEEDED(hr) && pRootElement != NULL)
//         {
//             find_all_windows(pRootElement);
//             // BSTR bstrName;

//             // hr = IUIAutomationElement_get_CurrentName(pRootElement,
//             &bstrName);
//             // if (SUCCEEDED(hr))
//             // {
//             //     wprintf(L"Root Element Name: %s\n", bstrName);
//             //     SysFreeString(bstrName);
//             // }

//             IUIAutomationElement_Release(pRootElement);
//         }

//         IUIAutomation_Release(g_pAutomation);
//     }
//     else
//     {
//         printf("Failed to create CUIAutomation. Error: %d\n, %p", hr,
//         g_pAutomation);
//     }

//     CoUninitialize();

//     return 0;
// }

// void enumerate_root_children(IUIAutomationElement *pRootElement, int depth)
// {
//     if (depth > 10)
//     {
//         return;
//     }

//     IUIAutomationTreeWalker *pControlWalker = NULL;
//     IUIAutomationElement *pNode = NULL;

//     HRESULT hr = IUIAutomation_get_ControlViewWalker(g_pAutomation,
//     &pControlWalker); if (FAILED(hr) || pControlWalker == NULL)
//         return;

//     hr = IUIAutomationTreeWalker_GetFirstChildElement(pControlWalker,
//     pRootElement, &pNode); if (FAILED(hr) || pNode == NULL)
//     {
//         IUIAutomationTreeWalker_Release(pControlWalker);
//         return;
//     }

//     while (pNode)
//     {
//         BSTR desc;
//         IUIAutomationElement_get_CurrentLocalizedControlType(pNode, &desc);
//         wprintf(L"%s\n", desc);
//         SysFreeString(desc);

//         hr = IUIAutomationTreeWalker_GetNextSiblingElement(pControlWalker,
//         pRootElement, &pNode); if (FAILED(hr) || pNode == NULL)
//         {
//             break;
//         }
//     }

//     if (pNode != NULL)
//         IUIAutomationElement_Release(pNode);

//     if (pControlWalker != NULL)
//         IUIAutomationTreeWalker_Release(pControlWalker);
// }

// void find_all_windows(IUIAutomationElement *pRootElement)
// {
//     IUIAutomationCondition *condition = NULL;
//     VARIANT var = {.vt = VT_I4, .lVal = UIA_WindowControlTypeId};
//     HRESULT hr = IUIAutomation_CreatePropertyCondition(g_pAutomation,
//     UIA_ControlTypePropertyId, var, &condition);

//     if (FAILED(hr) || condition == NULL)
//         return;

//     IUIAutomationElementArray *windows_array = NULL;
//     hr = IUIAutomationElement_FindAll(pRootElement, TreeScope_Children,
//     condition, &windows_array);

//     if (FAILED(hr) || windows_array == NULL)
//     {
//         IUIAutomationCondition_Release(condition);
//         return;
//     }

//     int count = 0;
//     IUIAutomationElementArray_get_Length(windows_array, &count);
//     wprintf(L"Found %d windows\n", count);

//     IUIAutomationElement *window = NULL;
//     BSTR bstrName;

//     for (size_t i = 0; i < count; i++)
//     {
//         window = NULL;
//         hr = IUIAutomationElementArray_GetElement(windows_array, i, &window);

//         if (FAILED(hr) || window == NULL)
//         {
//             break;
//         }

//         hr = IUIAutomationElement_get_CurrentName(pRootElement, &bstrName);
//         if (SUCCEEDED(hr))
//         {
//             wprintf(L"Window: %s\n", bstrName);
//         }

//         IUIAutomationElement_Release(window);
//     }

//     SysFreeString(bstrName);
//     if (window != NULL)
//         IUIAutomationElement_Release(window);

//     if (condition != NULL)
//         IUIAutomationCondition_Release(condition);

//     if (windows_array != NULL)
//         IUIAutomationElementArray_Release(windows_array);
// }