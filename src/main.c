#include "common.h"
#include "eventloop.h"
#include <windows.h>

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param);
BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param);
BOOL init_com(WmParams *wm_params);

BOOL init(WmParams *wm_params) {
  wm_params->active_workspace = 1;

  wm_params->ignored_windows = malloc(sizeof(StrVector));
  INIT_VECTOR(wm_params->ignored_windows)
  // TODO:
  str_vec_add(wm_params->ignored_windows, "Code.exe");

  wm_params->workspaces = malloc(sizeof(WorkspacesVector));
  INIT_VECTOR(wm_params->workspaces)

  if (!init_com(wm_params)) {
    printf("COM initialization failed\n");
    return FALSE;
  }

  EnumDisplayMonitors(NULL, NULL, enum_monitors_proc, (LPARAM)wm_params);
  EnumWindows(enum_windows_proc, (LPARAM)wm_params);

  set_active_tracking(wm_params);

  return TRUE;
}

void main(void) {
  WmParams wm_params = {0};

  if (!init(&wm_params)) {
    printf("Initialization failed! Exiting...\n");
    cleanup(&wm_params);

    return;
  }

  log_state(&wm_params);

  calculate_windows_init(&wm_params);
  position_workspaces_windows(&wm_params, FALSE);

  start_event_loop(&wm_params);

  restore_windows(&wm_params);
  cleanup(&wm_params);

  CoUninitialize();
}

BOOL CALLBACK enum_windows_proc(HWND hwnd, LPARAM l_param) {
  WmParams *wm_params = (WmParams *)l_param;

  Workspace *active_workspace = get_active_workspace(wm_params);
  if (active_workspace == NULL) {
    printf("Couldn't find active workspace\n");
    return FALSE;
  }

  add_window(hwnd, active_workspace->windows, wm_params->ignored_windows);

  return TRUE;
}

BOOL CALLBACK enum_monitors_proc(HMONITOR hmonitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param) {
  WmParams *wm_params = (WmParams *)l_param;
  get_or_create_workspace(wm_params, wm_params->workspaces->size + 1, hmonitor);

  return TRUE;
}

BOOL init_com(WmParams *wm_params) {
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    return FALSE;
  }

  ITaskbarList *taskbar = NULL;
  hr = CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                        &IID_ITaskbarList, (void **)&taskbar);

  if (SUCCEEDED(hr)) {
    taskbar->lpVtbl->HrInit(taskbar);
    wm_params->taskbar = taskbar;

    return TRUE;
  }

  CoUninitialize();

  return FALSE;
}