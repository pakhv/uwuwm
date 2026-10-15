#include "common.h"
#include "eventloop.h"
#include <dwmapi.h>

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param);
BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param);

void init(WmParams *wm_params) {
  wm_params->active_workspace = 1;
  wm_params->workspaces = malloc(sizeof(WorkspacesVector));
  INIT_VECTOR(wm_params->workspaces)

  EnumDisplayMonitors(NULL, NULL, enum_monitors_proc,
                      (LPARAM)wm_params->workspaces);
  EnumWindows(enum_windows_proc, (LPARAM)wm_params);
}

void main(void) {
  WmParams wm_params = {0};
  init(&wm_params);

  printf("Windows num: %zu\n", get_windows_number(&wm_params));
  printf("Workspaces num: %zu\n", wm_params.workspaces->size);

  calculate_windows_init(&wm_params);
  position_windows_init(&wm_params);

  start_event_loop(&wm_params);

  cleanup(&wm_params);
}

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param) {
  WmParams *wm_params = (WmParams *)l_param;

  if (!IsWindowVisible(h_wnd)) {
    return TRUE;
  }

  LONG_PTR exStyle = GetWindowLongPtr(h_wnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TOOLWINDOW) {
    return TRUE;
  }

  int cloaked = 0;
  HRESULT hr =
      DwmGetWindowAttribute(h_wnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  if (SUCCEEDED(hr) && cloaked != 0) {
    return TRUE;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);
  Window *window = malloc(sizeof(Window));

  char *p_name = malloc(sizeof(char) * MAX_EXE_NAME_LENGTH);
  get_process_name_by_window(h_wnd, p_name);

  window->process_name = p_name;
  window->handle = h_wnd;

  wnd_array_add(active_workspace->windows, window);

  printf("%s\n", window->process_name);

  return TRUE;
}

BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param) {
  WorkspacesVector *workspaces = (WorkspacesVector *)l_param;

  Workspace *workspace = malloc(sizeof(Workspace));
  workspace_array_add(workspaces, workspace);

  workspace->monitor = h_monitor;
  workspace->num = workspaces->size;

  WindowsVector *windows = malloc(sizeof(WindowsVector));
  INIT_VECTOR(windows);

  workspace->windows = windows;

  return TRUE;
}