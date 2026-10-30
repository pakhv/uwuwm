#include "common.h"
#include "eventloop.h"
#include <dwmapi.h>
#include <process.h>
#include <windows.h>

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param);
BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param);

void init(WmParams *wm_params) {
  wm_params->active_workspace = 1;

  wm_params->ignored_windows = malloc(sizeof(StrVector));
  INIT_VECTOR(wm_params->ignored_windows)
  // TODO:
  str_vec_add(wm_params->ignored_windows, "Code.exe");

  wm_params->workspaces = malloc(sizeof(WorkspacesVector));
  INIT_VECTOR(wm_params->workspaces)

  EnumDisplayMonitors(NULL, NULL, enum_monitors_proc,
                      (LPARAM)wm_params->workspaces);
  EnumWindows(enum_windows_proc, (LPARAM)wm_params);

  set_active_tracking(wm_params);
}

void main(void) {
  WmParams wm_params = {0};
  init(&wm_params);

  printf("Windows num: %zu\n", get_windows_number(&wm_params));
  printf("Workspaces num: %zu\n", wm_params.workspaces->size);

  calculate_windows_init(&wm_params);
  position_workspaces_windows(&wm_params, FALSE);

  start_event_loop(&wm_params);

  restore_windows(&wm_params);
  cleanup(&wm_params);
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

BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param) {
  WorkspacesVector *workspaces = (WorkspacesVector *)l_param;

  Workspace *workspace = malloc(sizeof(Workspace));
  workspaces_vec_add(workspaces, workspace);

  workspace->monitor = h_monitor;
  workspace->num = workspaces->size;

  WindowsVector *windows = malloc(sizeof(WindowsVector));
  INIT_VECTOR(windows);

  workspace->windows = windows;

  return TRUE;
}