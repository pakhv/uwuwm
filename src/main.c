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
void get_workspace_windows_to_position(Vector_Window *windows,
                                       size_t workspace_num,
                                       Vector_Window *v_windows,
                                       Vector_Window *h_windows);
BOOL position_monitor_windows(HDWP hdwp, Vector_Window *windows, LPRECT m_rect,
                              int flags);
BOOL map_to_window_coords(HWND window, LPRECT t_rect);
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
  Vector_Window *v_windows = malloc(sizeof(Vector_Window));
  Vector_Window *h_windows = malloc(sizeof(Vector_Window));

  INIT_VECTOR(v_windows)
  INIT_VECTOR(h_windows)

  get_workspace_windows_to_position(wm_params->windows, workspace_num,
                                    v_windows, h_windows);

  if (v_windows->size == 0 && h_windows->size == 0) {
    printf("No windows to position for workspace %zu", workspace_num);
    goto Cleanup;
  }

  HMONITOR monitor = v_windows->data[0]->workspace->monitor;
  MONITORINFO m_info = {.cbSize = sizeof(MONITORINFO)};

  if (!GetMonitorInfo(monitor, &m_info)) {
    printf("Failed to retrieve information about monitor for workspace %zu",
           workspace_num);
    goto Cleanup;
  }

  HDWP hdwp = BeginDeferWindowPos(v_windows->size);
  if (hdwp == NULL) {
    printf("Failed to position windows for workspace %zu\n", workspace_num);
    goto Cleanup;
  }

  if (!position_monitor_windows(hdwp, v_windows, &m_info.rcWork, WP_INIT)) {
    printf("Failed to position visible windows for workspace %zu:\n",
           workspace_num);
    goto Cleanup;
  }

  if (!position_monitor_windows(hdwp, h_windows, &m_info.rcWork,
                                WP_INIT | WP_HIDDEN)) {
    printf("Failed to position hidden windows for workspace %zu:\n",
           workspace_num);
    goto Cleanup;
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed to position windows for workspace %zu\n", workspace_num);
  }

Cleanup:
  free(v_windows);
  free(h_windows);
}

BOOL position_monitor_windows(HDWP hdwp, Vector_Window *windows, LPRECT m_rect,
                              int flags) {
  if (windows->size == 0) {
    return TRUE;
  }

  size_t w_width = (m_rect->right - m_rect->left) / windows->size;
  size_t w_height = m_rect->bottom - m_rect->top;

  BOOL is_init = flags & WP_INIT;
  BOOL is_hidden = flags & WP_HIDDEN;

  for (size_t i = 0; i < windows->size; i++) {
    HWND window = windows->data[i]->handle;

    if (IsZoomed(window)) {
      ShowWindow(window, SW_RESTORE);
    }

    RECT position = {0};
    if (is_init && !is_hidden) {
      position.left = m_rect->left + i * w_width;
      position.right = m_rect->left + (i + 1) * w_width;
      position.top = m_rect->top;
      position.bottom = m_rect->bottom;
    } else {
      position = windows->data[i]->w_rect;
    }

    if (is_hidden) {
      position.left = HIDDEN_WINDOW_X;
      position.right = HIDDEN_WINDOW_X + w_width;
    }

    if (is_init) {
      if (!map_to_window_coords(window, &position)) {
        printf("Failed to get window gaps %s", windows->data[i]->process_name);
        continue;
      }
    }

    if (is_init && !is_hidden) {
      windows->data[i]->w_rect = position;
    }

    printf("%d %d %d %d\n", position.left, position.right, position.top,
           position.bottom);

    HDWP def_hdpw = DeferWindowPos(hdwp, window, NULL, position.left,
                                   position.top, position.right - position.left,
                                   position.bottom - position.top,
                                   SWP_NOZORDER | SWP_NOACTIVATE);

    if (def_hdpw == NULL) {
      return FALSE;
    }
  }

  return TRUE;
}

BOOL map_to_window_coords(HWND window, LPRECT t_rect) {
  RECT real_w_rect = {0};
  if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS,
                                   &real_w_rect, sizeof(RECT)))) {
    return FALSE;
  }

  RECT w_rect = {0};
  if (!GetWindowRect(window, &w_rect)) {
    return FALSE;
  }

  int l_gap = real_w_rect.left - w_rect.left;
  int r_gap = w_rect.right - real_w_rect.right;
  int t_gap = real_w_rect.top - w_rect.top;
  int b_gap = w_rect.bottom - real_w_rect.bottom;

  t_rect->left -= l_gap;
  t_rect->right += r_gap;
  t_rect->top -= t_gap;
  t_rect->bottom += b_gap;

  return TRUE;
}

void get_workspace_windows_to_position(Vector_Window *windows,
                                       size_t workspace_num,
                                       Vector_Window *v_windows,
                                       Vector_Window *h_windows) {

  for (size_t i = 0; i < windows->size; i++) {
    if (IsIconic(windows->data[i]->handle)) {
      continue;
    }

    if (windows->data[i]->workspace->num == workspace_num) {
      wnd_array_add(v_windows, windows->data[i]);
    } else {
      // maybe filter windows that are already "hidden" out
      wnd_array_add(h_windows, windows->data[i]);
    }
  }
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