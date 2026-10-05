#define COBJMACROS

#include "main.h"
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include <stdio.h>
#include <uiautomation.h>

void wnd_array_add(Vector_HWND *arr, HWND *handle);
BOOL CALLBACK enum_windows_proc(HWND handle, LPARAM l_param);
void position_windows(Vector_HWND *arr);

Wm_params *init(void) {
  Wm_params *wm_params = malloc(sizeof(Wm_params));
  wm_params->windows = malloc(sizeof(Vector_HWND));

  wm_params->windows->size = 0;
  wm_params->windows->capacity = 0;
  wm_params->windows->data = NULL;

  EnumWindows(enum_windows_proc, (LPARAM)wm_params->windows);

  return wm_params;
}

void main(void) {
  // setlocale(LC_ALL, "");
  // SetConsoleOutputCP(CP_UTF8);
  // _setmode(_fileno(stdout), _O_U16TEXT);

  Wm_params *wm_params = init();
  Vector_HWND *windows = wm_params->windows;

  position_windows(windows);

  free(wm_params->windows);
  free(wm_params);
}

void position_windows(Vector_HWND *windows) {
  HDWP hdwp = BeginDeferWindowPos(windows->size);
  if (hdwp == NULL) {
    printf("Failed\n");
    return;
  }

  printf("Windows num: %zu\n", windows->size);

  for (size_t i = 0; i < windows->size; i++) {
    HWND wnd = windows->data[i];
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

void wnd_array_add(Vector_HWND *arr, HWND *handle) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1) * sizeof(HWND);
    arr->data = realloc(arr->data, arr->capacity);
  }

  arr->data[arr->size] = *handle;
  arr->size += 1;
}

BOOL CALLBACK enum_windows_proc(HWND handle, LPARAM l_param) {
  Vector_HWND *windows = (Vector_HWND *)l_param;
  wchar_t buff[255];

  if (IsWindowVisible(handle)) {
    wnd_array_add(windows, &handle);
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