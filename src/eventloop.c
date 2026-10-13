#define COBJMACROS

#include "main.h"
#include <stdio.h>
#include <uiautomation.h>
#include <windows.h>

typedef struct WindowsEventHandler {
  IUIAutomationEventHandlerVtbl *lp_vtbl;
  LONG ref_count;
  Wm_params *wm_params;
} WindowsEventHandler;

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

static IUIAutomationEventHandlerVtbl windows_event_handler_vtbl = {
    EventHandler_QueryInterface, EventHandler_AddRef, EventHandler_Release,
    EventHandler_HandleAutomationEvent};

void register_windows_event_handler(IUIAutomation *ui_automation,
                                    IUIAutomationElement *root_el,
                                    Wm_params *wm_params) {
  WindowsEventHandler handler = {.lp_vtbl = &windows_event_handler_vtbl,
                                 .ref_count = 1,
                                 .wm_params = wm_params};
  IUIAutomationEventHandler *ph_interface =
      (IUIAutomationEventHandler *)&handler;

  HRESULT hr = IUIAutomation_AddAutomationEventHandler(
      ui_automation, UIA_Window_WindowOpenedEventId, root_el, TreeScope_Subtree,
      NULL, ph_interface);

  MSG msg;
  BOOL m_ret;

  if (SUCCEEDED(hr)) {
    printf("Successfully registered event handler!\n");

    while ((m_ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
      if (m_ret == -1) {
        break;
      } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    IUIAutomation_RemoveAutomationEventHandler(
        ui_automation, UIA_Invoke_InvokedEventId, root_el, ph_interface);
  } else {
    printf("Failed to register handler\n");
  }
}

void start_events_loop(Wm_params *wm_params) {
  HRESULT hr;
  IUIAutomationElement *root_el = NULL;

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