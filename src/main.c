#include "eventloop.h"
#include <dwmapi.h>

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param);
BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param);

void calculate_windows_init(WmParams *params);
void position_windows_init(WmParams *wm_params);

BOOL position_windows(HDWP hdwp, WindowsVector *windows, BOOL visible);
BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect);

BOOL map_to_window_coords(HWND window, LPRECT t_rect);
size_t get_windows_number(WmParams *wm_params);
Workspace *get_active_workspace(WmParams *wm_params);

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

void calculate_windows_init(WmParams *params) {
  WindowsVector *v_windows = malloc(sizeof(WindowsVector));
  INIT_VECTOR(v_windows)

  HMONITOR monitor;
  MONITORINFO m_info;

  for (size_t i = 0; i < params->workspaces->size; i++) {
    Workspace *workspace = params->workspaces->data[i];
    m_info.cbSize = sizeof(MONITORINFO);

    if (!GetMonitorInfo(workspace->monitor, &m_info)) {
      printf("Failed to retrieve information about monitor for workspace %zu",
             workspace->num);
      continue;
    }

    for (size_t i = 0; i < workspace->windows->size; i++) {
      Window *window = workspace->windows->data[i];

      if (!IsIconic(window->handle)) {
        wnd_array_add(v_windows, window);
      }
    }

    calculate_windows(v_windows, &m_info.rcWork);
  }

  free(v_windows);
}

BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect) {
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

void position_windows_init(WmParams *wm_params) {
  size_t w_num = get_windows_number(wm_params);

  HDWP hdwp = BeginDeferWindowPos(w_num);
  if (hdwp == NULL) {
    printf("Failed to position windows \n");
    return;
  }

  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    Workspace *workspace = wm_params->workspaces->data[i];

    if (workspace->num == wm_params->active_workspace) {
      if (!position_windows(hdwp, workspace->windows, TRUE)) {
        printf("Failed to position visible windows\n");
        return;
      }

      continue;
    }

    if (!position_windows(hdwp, workspace->windows, FALSE)) {
      printf("Failed to position invisible windows\n");
      return;
    }
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed to position windows\n");
  }
}

BOOL position_windows(HDWP hdwp, WindowsVector *windows, BOOL visible) {
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

size_t get_windows_number(WmParams *wm_params) {
  size_t w_num = 0;
  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    w_num += wm_params->workspaces->data[i]->windows->size;
  }

  return w_num;
}

Workspace *get_active_workspace(WmParams *wm_params) {
  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    if (wm_params->workspaces->data[i]->num == wm_params->active_workspace) {
      return wm_params->workspaces->data[i];
    }
  }

  return NULL;
}