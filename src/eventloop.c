#define COBJMACROS

#include "common.h"
#include <UIAutomationClient.h>

static HWND g_hidden_wnd = NULL;
static IUIAutomation *ui_automation = NULL;

typedef struct WindowsEventHandler {
  IUIAutomationEventHandlerVtbl *lp_vtbl;
  LONG ref_count;
  WmParams *wm_params;
  IUIAutomation *ui_automation;
} WindowsEventHandler;

STDMETHODIMP EventHandler_QueryInterface(IUIAutomationEventHandler *this,
                                         REFIID riid, void **ppv_object);
STDMETHODIMP_(ULONG) EventHandler_AddRef(IUIAutomationEventHandler *this);
STDMETHODIMP_(ULONG) EventHandler_Release(IUIAutomationEventHandler *this);
STDMETHODIMP EventHandler_HandleAutomationEvent(IUIAutomationEventHandler *this,
                                                IUIAutomationElement *sender,
                                                EVENTID event_id);

BOOL WINAPI exit_handler(DWORD c_type);
LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT msg, WPARAM w_param,
                                    LPARAM l_param);
void create_hidden_window(WNDCLASSEX *wc);

void register_windows_event_handler(IUIAutomation *ui_automation,
                                    IUIAutomationElement *root_el,
                                    WmParams *wm_params) {
  IUIAutomationEventHandlerVtbl windows_event_handler_vtbl = {
      EventHandler_QueryInterface, EventHandler_AddRef, EventHandler_Release,
      EventHandler_HandleAutomationEvent};
  WindowsEventHandler handler = {.lp_vtbl = &windows_event_handler_vtbl,
                                 .ref_count = 1,
                                 .wm_params = wm_params,
                                 .ui_automation = ui_automation};
  IUIAutomationEventHandler *ph_interface =
      (IUIAutomationEventHandler *)&handler;

  HRESULT hr = IUIAutomation_AddAutomationEventHandler(
      ui_automation, UIA_Window_WindowOpenedEventId, root_el, TreeScope_Subtree,
      NULL, ph_interface);

  if (FAILED(hr)) {
    printf("Failed to register window opened event handler\n");
    return;
  }

  hr = IUIAutomation_AddAutomationEventHandler(
      ui_automation, UIA_Window_WindowClosedEventId, root_el, TreeScope_Subtree,
      NULL, ph_interface);

  if (FAILED(hr)) {
    printf("Failed to register window closed event handler\n");
    return;
  }

  MSG msg = {0};
  BOOL m_ret;

  while ((m_ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
    if (msg.message == WM_QUIT) {
      break;
    }
    if (m_ret == -1) {
      printf("Error reading message from the queue: %d\n", GetLastError());
      break;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  printf("Successfully exited the event loop\n");

  IUIAutomation_RemoveAutomationEventHandler(
      ui_automation, UIA_Invoke_InvokedEventId, root_el, ph_interface);
}

BOOL init_uiautomation() {
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  HRESULT hr;
  hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IUIAutomation, &ui_automation);

  if (FAILED(hr) || ui_automation == NULL) {
    return FALSE;
  }

  return TRUE;
}

void start_event_loop(WmParams *wm_params) {
  HRESULT hr;
  IUIAutomationElement *root_el = NULL;
  WNDCLASSEX wc = {0};

  create_hidden_window(&wc);

  if (!SetConsoleCtrlHandler(exit_handler, TRUE)) {
    printf("Could not set control handler\n");
    return;
  }

  hr = IUIAutomation_GetRootElement(ui_automation, &root_el);
  if (FAILED(hr) || root_el == NULL) {
    goto Cleanup;
  }

  register_windows_event_handler(ui_automation, root_el, wm_params);

Cleanup:
  if (root_el != NULL) {
    IUIAutomationElement_Release(root_el);
  }

  if (ui_automation != NULL) {
    IUIAutomation_Release(ui_automation);
  }

  CoUninitialize();
}

HRESULT get_window_runtime_id(HWND hwnd, SAFEARRAY **rid) {
  IUIAutomationElement *element = NULL;
  HRESULT hr = IUIAutomation_ElementFromHandle(ui_automation, hwnd, &element);

  if (FAILED(hr)) {
    printf("Failed to get runtime id for a handle");
    return hr;
  }

  hr = IUIAutomationElement_GetRuntimeId(element, rid);
  return hr;
}

HRESULT compare_runtime_ids(SAFEARRAY *rid1, SAFEARRAY *rid2, BOOL *result) {
  return IUIAutomation_CompareRuntimeIds(ui_automation, rid1, rid2, result);
}

STDMETHODIMP EventHandler_QueryInterface(IUIAutomationEventHandler *this,
                                         REFIID riid, void **ppv_object) {
  if (!ppv_object) {
    return E_POINTER;
  }

  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_IUIAutomationEventHandler)) {
    *ppv_object = this;
    this->lpVtbl->AddRef(this);
    return S_OK;
  }

  *ppv_object = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EventHandler_AddRef(IUIAutomationEventHandler *this) {
  WindowsEventHandler *myThis = (WindowsEventHandler *)this;
  return InterlockedIncrement(&myThis->ref_count);
}

STDMETHODIMP_(ULONG) EventHandler_Release(IUIAutomationEventHandler *this) {
  WindowsEventHandler *myThis = (WindowsEventHandler *)this;
  LONG count = InterlockedDecrement(&myThis->ref_count);
  if (count == 0) {
  }
  return count;
}

STDMETHODIMP EventHandler_HandleAutomationEvent(IUIAutomationEventHandler *this,
                                                IUIAutomationElement *sender,
                                                EVENTID event_id) {
  WindowsEventHandler *e_handler = (WindowsEventHandler *)this;
  UIA_HWND window = NULL;
  SAFEARRAY *rid = NULL;

  HRESULT hr = IUIAutomationElement_GetRuntimeId(sender, &rid);
  if (FAILED(hr)) {
    printf("Error handling event %d: failed to get element runtime id\n",
           event_id);
    return S_OK;
  }

  Workspace *active_workspace = get_active_workspace(e_handler->wm_params);

  if (event_id == UIA_Window_WindowOpenedEventId &&
      !is_window_tracked(rid, e_handler->wm_params)) {
    HRESULT hr =
        IUIAutomationElement_get_CurrentNativeWindowHandle(sender, &window);

    if (SUCCEEDED(hr)) {
      if (!add_window(window, active_workspace->windows)) {
        return S_OK;
      }

      calculate_workspace_windows(active_workspace);
      position_workspaces_windows(e_handler->wm_params);
    }
  }

  if (event_id == UIA_Window_WindowClosedEventId) {
    if (!remove_window(rid, e_handler->wm_params->workspaces)) {
      return S_OK;
    }

    calculate_workspace_windows(active_workspace);
    position_workspaces_windows(e_handler->wm_params);
  }

  log_state(e_handler->wm_params);

  return S_OK;
}

void create_hidden_window(WNDCLASSEX *wc) {
  wc->cbSize = sizeof(WNDCLASSEX);
  wc->lpfnWndProc = hidden_window_proc;
  wc->hInstance = GetModuleHandle(NULL);
  wc->lpszClassName = "uwu_hidden_window";
  RegisterClassEx(wc);

  g_hidden_wnd = CreateWindowEx(0, wc->lpszClassName, NULL, 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, wc->hInstance, NULL);
}

BOOL WINAPI exit_handler(DWORD c_type) {
  if ((c_type == CTRL_C_EVENT || c_type == CTRL_CLOSE_EVENT) &&
      g_hidden_wnd != NULL) {
    PostMessage(g_hidden_wnd, WM_CLOSE, 0, 0);
    return TRUE;
  }

  return FALSE;
}

LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT msg, WPARAM w_param,
                                    LPARAM l_param) {
  switch (msg) {
  case WM_CLOSE:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hwnd, msg, w_param, l_param);
}