#define COBJMACROS

#include "structs.h"
#include <uiautomation.h>

static HWND g_hidden_wnd = NULL;

typedef struct WindowsEventHandler {
  IUIAutomationEventHandlerVtbl *lp_vtbl;
  LONG ref_count;
  WmParams *wm_params;
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
                                 .wm_params = wm_params};
  IUIAutomationEventHandler *ph_interface =
      (IUIAutomationEventHandler *)&handler;

  HRESULT hr = IUIAutomation_AddAutomationEventHandler(
      ui_automation, UIA_Window_WindowOpenedEventId, root_el, TreeScope_Subtree,
      NULL, ph_interface);

  MSG msg = {0};
  BOOL m_ret;

  if (FAILED(hr)) {
    printf("Failed to register event handler\n");
    return;
  }

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

void start_event_loop(WmParams *wm_params) {
  HRESULT hr;
  IUIAutomationElement *root_el = NULL;
  WNDCLASSEX wc = {0};

  create_hidden_window(&wc);

  if (!SetConsoleCtrlHandler(exit_handler, TRUE)) {
    printf("Could not set control handler\n");
    return;
  }

  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  IUIAutomation *ui_utomation = NULL;
  hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                        &IID_IUIAutomation, &ui_utomation);

  if (FAILED(hr) || ui_utomation == NULL) {
    goto Cleanup;
  }

  hr = IUIAutomation_GetRootElement(ui_utomation, &root_el);
  if (FAILED(hr) || root_el == NULL) {
    goto Cleanup;
  }

  register_windows_event_handler(ui_utomation, root_el, wm_params);

Cleanup:
  if (ui_utomation != NULL) {
    hr = IUIAutomation_RemoveAllEventHandlers(ui_utomation);
    if (FAILED(hr))
      IUIAutomation_Release(ui_utomation);
  }

  if (root_el != NULL)
    IUIAutomationElement_Release(root_el);

  CoUninitialize();
}

STDMETHODIMP EventHandler_QueryInterface(IUIAutomationEventHandler *this,
                                         REFIID riid, void **ppv_object) {
  if (!ppv_object)
    return E_POINTER;

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
  printf("Event received! Event ID: %d\n", event_id);
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