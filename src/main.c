#define COBJMACROS

#include "main.h"
#include <Windows.h>
#include <dwmapi.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include <stdio.h>
#include <uiautomation.h>

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param);
BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param);
void position_windows(Wm_params *wm_params, size_t workspace_num);
Vector_Window *get_workspace_windows_to_position(Vector_Window *windows,
                                                 size_t workspace_num);
void workspace_array_add(Vector_Workspace *arr, Workspace *el);
void wnd_array_add(Vector_Window *arr, Window *el);

void init(Wm_params *wm_params) {
  wm_params->active_workspace = 1;
  wm_params->windows = malloc(sizeof(Vector_Window));
  wm_params->workspaces = malloc(sizeof(Vector_Workspace));

  INIT_VECTOR(wm_params->windows)
  INIT_VECTOR(wm_params->workspaces)

  EnumDisplayMonitors(NULL, NULL, enum_monitors_proc,
                      (LPARAM)wm_params->workspaces);
  EnumWindows(enum_windows_proc, (LPARAM)wm_params);
}

void main(void) {
  Wm_params wm_params = {0};
  init(&wm_params);

  Vector_Window *windows = wm_params.windows;

  printf("Windows num: %zu\n", windows->size);
  printf("Workspaces num: %zu\n", wm_params.workspaces->size);

  position_windows(&wm_params, wm_params.active_workspace);

  FREE_VECTOR(wm_params.windows)
  FREE_VECTOR(wm_params.workspaces)

  free(wm_params.workspaces);
}

void position_windows(Wm_params *wm_params, size_t workspace_num) {
  Vector_Window *w_windows =
      get_workspace_windows_to_position(wm_params->windows, workspace_num);

  if (w_windows->size == 0) {
    printf("No windows to position for workspace %zu", workspace_num);
    free(w_windows);
    return;
  }

  HMONITOR monitor = w_windows->data[0]->workspace->monitor;
  MONITORINFO m_info = {.cbSize = sizeof(MONITORINFO)};

  if (!GetMonitorInfo(monitor, &m_info)) {
    printf("Failed to retrieve information about monitor for workspace %zu",
           workspace_num);
    free(w_windows);
    return;
  }

  HDWP hdwp = BeginDeferWindowPos(w_windows->size);
  if (hdwp == NULL) {
    printf("Failed to position windows for workspace %zu\n", workspace_num);
    free(w_windows);
    return;
  }

  size_t wnd_width =
      (m_info.rcMonitor.right - m_info.rcMonitor.left) / w_windows->size;
  size_t wnd_height = m_info.rcWork.bottom - m_info.rcWork.top;

  printf("Monitor width: %lu height: %lu; window width: %zu height: %zu \n",
         m_info.rcMonitor.right - m_info.rcMonitor.left,
         m_info.rcMonitor.bottom - m_info.rcMonitor.top, wnd_width, wnd_height);

  for (size_t i = 0; i < w_windows->size; i++) {
    HWND wnd = w_windows->data[i]->handle;

    if (IsZoomed(wnd)) {
      ShowWindow(wnd, SW_RESTORE);
    }

    RECT w_g_rect = {0};
    if (FAILED(DwmGetWindowAttribute(wnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                     &w_g_rect, sizeof(RECT)))) {
      printf("Failed to get window gaps %s", w_windows->data[i]->process_name);
      continue;
    }

    RECT w_rect = {0};
    GetWindowRect(wnd, &w_rect);

    int gap_x = w_g_rect.left - w_rect.left;
    int gap_y = w_rect.bottom - w_g_rect.bottom;

    HDWP def_hdpw = DeferWindowPos(hdwp, wnd, NULL,
                                   m_info.rcWork.left + i * wnd_width - gap_x,
                                   0, wnd_width + 2 * gap_x, wnd_height + gap_y,
                                   SWP_NOZORDER | SWP_NOACTIVATE);

    if (def_hdpw == NULL) {
      printf("Failed to position windows for workspace %zu\n: %lu",
             workspace_num, GetLastError());
      free(w_windows);
      return;
    }
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed\n");
  }

  free(w_windows);
}

Vector_Window *get_workspace_windows_to_position(Vector_Window *windows,
                                                 size_t workspace_num) {
  Vector_Window *w_windows = malloc(sizeof(Vector_Window));
  INIT_VECTOR(w_windows)

  for (size_t i = 0; i < windows->size; i++) {
    if (windows->data[i]->workspace->num == workspace_num &&
        !IsIconic(windows->data[i]->handle)) {
      wnd_array_add(w_windows, windows->data[i]);
    }
  }

  return w_windows;
}

void workspace_array_add(Vector_Workspace *arr, Workspace *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(Workspace *));
  }

  arr->data[arr->size] = el;
  arr->size += 1;
}

void wnd_array_add(Vector_Window *arr, Window *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(Window *));
  }

  arr->data[arr->size] = el;
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

  CloseHandle(h_p);

  extract_exe_name(p_full_path, p_path_l, p_name);
}

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param) {
  Wm_params *wm_params = (Wm_params *)l_param;
  Vector_Window *windows = wm_params->windows;

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

  Window *window = malloc(sizeof(Window));
  wnd_array_add(windows, window);

  window->handle = h_wnd;
  // assigning first workspace to all windows
  window->workspace = wm_params->workspaces->data[0];

  char *p_name = malloc(sizeof(char) * MAX_EXE_NAME_LENGTH);
  get_process_name_by_window(h_wnd, p_name);
  window->process_name = p_name;

  printf("%s\n", window->process_name);

  return TRUE;
}

BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param) {
  Vector_Workspace *workspaces = (Vector_Workspace *)l_param;

  Workspace *workspace = malloc(sizeof(Workspace));
  workspace_array_add(workspaces, workspace);

  workspace->monitor = h_monitor;
  workspace->num = workspaces->size;

  return TRUE;
}