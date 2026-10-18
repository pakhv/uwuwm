#define COBJMACROS

#include "common.h"
#include <UIAutomationClient.h>

static HWND g_hidden_wnd = NULL;

typedef struct WindowsEventHandler {
  IUIAutomationEventHandlerVtbl *lp_vtbl;
  LONG ref_count;
  WmParams *wm_params;
  IUIAutomation *ui_automation;
} WindowsEventHandler;

STDMETHODIMP EventHandler_QueryInterface(IUIAutomationEventHandler *h,
                                         REFIID riid, void **ppv_object);
STDMETHODIMP_(ULONG) EventHandler_AddRef(IUIAutomationEventHandler *h);
STDMETHODIMP_(ULONG) EventHandler_Release(IUIAutomationEventHandler *h);
STDMETHODIMP EventHandler_HandleAutomationEvent(IUIAutomationEventHandler *h,
                                                IUIAutomationElement *sender,
                                                EVENTID event_id);

static IUIAutomationEventHandlerVtbl windows_event_handler_vtbl = {
    EventHandler_QueryInterface, EventHandler_AddRef, EventHandler_Release,
    EventHandler_HandleAutomationEvent};

BOOL WINAPI exit_handler(DWORD c_type);
LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT msg, WPARAM w_param,
                                    LPARAM l_param);
void create_hidden_window(WNDCLASSEX *wc);

void register_windows_event_handler(IUIAutomation *ui_automation,
                                    IUIAutomationElement *root_el,
                                    WmParams *wm_params) {

  WindowsEventHandler *handler = malloc(sizeof(WindowsEventHandler));
  handler->lp_vtbl = &windows_event_handler_vtbl;
  handler->ref_count = 1;
  handler->wm_params = wm_params;
  handler->ui_automation = ui_automation;

  IUIAutomationEventHandler *ph_interface =
      (IUIAutomationEventHandler *)handler;

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

  getchar();

  printf("Successfully exited the event loop\n");

  IUIAutomation_RemoveAutomationEventHandler(
      ui_automation, UIA_Window_WindowOpenedEventId, root_el, ph_interface);
  IUIAutomation_RemoveAutomationEventHandler(
      ui_automation, UIA_Window_WindowClosedEventId, root_el, ph_interface);
  free(handler);
}

BOOL init_uiautomation(WmParams *wm_params) {
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  HRESULT hr;
  hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IUIAutomation, (void **)&wm_params->uiautomation);

  if (FAILED(hr)) {
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

  IUIAutomation *uiautomation = (IUIAutomation *)wm_params->uiautomation;

  hr = IUIAutomation_GetRootElement(uiautomation, &root_el);
  if (FAILED(hr) || root_el == NULL) {
    goto Cleanup;
  }

  register_windows_event_handler(uiautomation, root_el, wm_params);

Cleanup:
  if (root_el != NULL) {
    IUIAutomationElement_Release(root_el);
  }

  if (uiautomation != NULL) {
    IUIAutomation_Release(uiautomation);
  }

  CoUninitialize();
}

HRESULT get_window_runtime_id(HWND hwnd, LPARAM uiautomation, SAFEARRAY **rid) {
  IUIAutomationElement *element = NULL;
  HRESULT hr = IUIAutomation_ElementFromHandle((IUIAutomation *)uiautomation,
                                               hwnd, &element);

  if (FAILED(hr)) {
    printf("Failed to get runtime id for a handle");
    return hr;
  }

  hr = IUIAutomationElement_GetRuntimeId(element, rid);
  return hr;
}

HRESULT compare_runtime_ids(SAFEARRAY *rid1, SAFEARRAY *rid2,
                            LPARAM uiautomation, BOOL *result) {
  return IUIAutomation_CompareRuntimeIds((IUIAutomation *)uiautomation, rid1,
                                         rid2, result);
}

STDMETHODIMP EventHandler_QueryInterface(IUIAutomationEventHandler *h,
                                         REFIID riid, void **ppv_object) {
  if (!ppv_object) {
    return E_POINTER;
  }

  if (IsEqualIID(riid, &IID_IUnknown) ||
      IsEqualIID(riid, &IID_IUIAutomationEventHandler)) {
    *ppv_object = h;
    h->lpVtbl->AddRef(h);
    return S_OK;
  }

  *ppv_object = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EventHandler_AddRef(IUIAutomationEventHandler *h) {
  WindowsEventHandler *handler = (WindowsEventHandler *)h;
  return InterlockedIncrement(&handler->ref_count);
}

STDMETHODIMP_(ULONG) EventHandler_Release(IUIAutomationEventHandler *h) {
  WindowsEventHandler *handler = (WindowsEventHandler *)h;
  LONG count = InterlockedDecrement(&handler->ref_count);
  if (count == 0) {
  }
  return count;
}

STDMETHODIMP EventHandler_HandleAutomationEvent(IUIAutomationEventHandler *h,
                                                IUIAutomationElement *sender,
                                                EVENTID event_id) {
  WindowsEventHandler *handler = (WindowsEventHandler *)h;
  UIA_HWND window = NULL;
  SAFEARRAY *rid = NULL;

  HRESULT hr = IUIAutomationElement_GetRuntimeId(sender, &rid);
  if (FAILED(hr)) {
    printf("Error handling event %d: failed to get element runtime id\n",
           event_id);
    return S_OK;
  }

  Workspace *active_workspace = get_active_workspace(handler->wm_params);

  if (event_id == UIA_Window_WindowOpenedEventId &&
      !is_window_tracked(rid, handler->wm_params)) {
    HRESULT hr =
        IUIAutomationElement_get_CurrentNativeWindowHandle(sender, &window);

    if (SUCCEEDED(hr)) {
      if (!add_window(window, active_workspace->windows,
                      (LPARAM)handler->ui_automation)) {
        return S_OK;
      }

      calculate_workspace_windows(active_workspace);
      position_workspaces_windows(handler->wm_params);
    }
  }

  if (event_id == UIA_Window_WindowClosedEventId) {
    if (!remove_window(rid, handler->wm_params->workspaces,
                       handler->wm_params->uiautomation)) {
      return S_OK;
    }

    calculate_workspace_windows(active_workspace);
    position_workspaces_windows(handler->wm_params);
  }

  log_state(handler->wm_params);

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