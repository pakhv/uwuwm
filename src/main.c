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

void calculate_windows_init(Wm_params *params);
void position_windows_init(Wm_params *wm_params);

BOOL position_windows(HDWP hdwp, Vector_Window *windows, BOOL visible);
BOOL calculate_windows(Vector_Window *windows, LPRECT m_rect);

void get_workspace_windows_to_position(Vector_Window *windows,
                                       size_t workspace_num,
                                       Vector_Window *v_windows,
                                       Vector_Window *h_windows);
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

  printf("Windows num: %zu\n", wm_params.windows->size);
  printf("Workspaces num: %zu\n", wm_params.workspaces->size);

  calculate_windows_init(&wm_params);
  position_windows_init(&wm_params);

  FREE_VECTOR(wm_params.windows)
  FREE_VECTOR(wm_params.workspaces)
  free(wm_params.workspaces);
}

void calculate_windows_init(Wm_params *params) {
  Vector_Window *v_windows = malloc(sizeof(Vector_Window));
  INIT_VECTOR(v_windows)

  HMONITOR monitor;
  MONITORINFO m_info;

  for (size_t i = 0; i < params->workspaces->size; i++) {
    int w_num = params->workspaces->data[i]->num;
    monitor = params->workspaces->data[i]->monitor;
    m_info.cbSize = sizeof(MONITORINFO);

    if (!GetMonitorInfo(monitor, &m_info)) {
      printf("Failed to retrieve information about monitor for workspace %d",
             w_num);
      goto Cleanup;
    }

    for (size_t j = 0; j < params->windows->size; j++) {
      if (params->windows->data[j]->workspace->num == w_num) {
        wnd_array_add(v_windows, params->windows->data[j]);
      }
    }

    calculate_windows(v_windows, &m_info.rcWork);

    v_windows->size = 0;
  }

Cleanup:
  free(v_windows);
}

BOOL calculate_windows(Vector_Window *windows, LPRECT m_rect) {
  if (windows->size == 0) {
    return TRUE;
  }

  size_t w_width = (m_rect->right - m_rect->left) / windows->size;
  size_t w_height = m_rect->bottom - m_rect->top;

  for (size_t i = 0; i < windows->size; i++) {
    HWND window = windows->data[i]->handle;

    if (IsZoomed(window)) {
      ShowWindow(window, SW_RESTORE);
    }

    RECT position = {.left = m_rect->left + i * w_width,
                     .right = m_rect->left + (i + 1) * w_width,
                     .top = m_rect->top,
                     .bottom = m_rect->bottom};

    if (!map_to_window_coords(window, &position)) {
      printf("Failed to get window gaps %s", windows->data[i]->process_name);
      continue;
    }

    windows->data[i]->w_rect = position;
  }

  return TRUE;
}

void position_windows_init(Wm_params *wm_params) {
  HDWP hdwp = BeginDeferWindowPos(wm_params->windows->size);
  if (hdwp == NULL) {
    printf("Failed to position windows \n");
    return;
  }

  Vector_Window *v_windows = malloc(sizeof(Vector_Window));
  Vector_Window *h_windows = malloc(sizeof(Vector_Window));

  INIT_VECTOR(v_windows)
  INIT_VECTOR(h_windows)

  get_workspace_windows_to_position(
      wm_params->windows, wm_params->active_workspace, v_windows, h_windows);

  if (!position_windows(hdwp, v_windows, TRUE)) {
    goto Cleanup;
  }

  if (!position_windows(hdwp, h_windows, FALSE)) {
    goto Cleanup;
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed to position windows\n");
  }

Cleanup:
  free(v_windows);
  free(h_windows);
}

BOOL position_windows(HDWP hdwp, Vector_Window *windows, BOOL visible) {
  if (windows->size == 0) {
    return TRUE;
  }

  for (size_t i = 0; i < windows->size; i++) {
    Window *window = windows->data[i];
    RECT w_rect = window->w_rect;

    if (IsZoomed(window->handle)) {
      ShowWindow(window->handle, SW_RESTORE);
    }

    if (!visible) {
      size_t w_width = w_rect.right - w_rect.left;
      w_rect.left = HIDDEN_WINDOW_X;
      w_rect.right = HIDDEN_WINDOW_X + w_width;
    }

    HDWP def_hdpw =
        DeferWindowPos(hdwp, window->handle, NULL, w_rect.left, w_rect.top,
                       w_rect.right - w_rect.left, w_rect.bottom - w_rect.top,
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