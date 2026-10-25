#include "common.h"
#include <dwmapi.h>

BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect);
BOOL position_windows(HDWP hdwp, WindowsVector *windows, BOOL visible);
BOOL is_window_manageable(HWND hwnd);
BOOL window_width_changed(HWND hwnd, RECT w_rect, WmParams *wm_params);
void move_window(HWND hwnd, RECT w_rect, WmParams *wm_params);

void check_all_windows_user_sized(Workspace *active_workspace, HWND hwnd);
BOOL map_to_window_coords(HWND hwnd, LPRECT t_rect);
size_t get_window_auto_width(WindowsVector *windows, LPRECT m_rect);
void rearrange_windows(WindowsVector *windows, int w_idx, int s_idx);

void calculate_and_position_active_workspace(WmParams *params) {
  Workspace *active_workspace = get_active_workspace(params);

  calculate_workspace_windows(active_workspace);
  position_workspaces_windows(params);
}

void calculate_windows_init(WmParams *params) {
  for (size_t i = 0; i < params->workspaces->size; i++) {
    Workspace *workspace = params->workspaces->data[i];

    calculate_workspace_windows(workspace);
  }
}

void calculate_workspace_windows(Workspace *workspace) {
  WindowsVector *v_windows = malloc(sizeof(WindowsVector));
  INIT_VECTOR(v_windows)

  MONITORINFO m_info;
  m_info.cbSize = sizeof(MONITORINFO);

  if (!GetMonitorInfo(workspace->monitor, &m_info)) {
    printf("Failed to retrieve information about monitor for workspace %zu",
           workspace->num);
    goto Cleanup;
  }

  for (size_t i = 0; i < workspace->windows->size; i++) {
    Window *window = workspace->windows->data[i];

    if (!IsIconic(window->handle)) {
      windows_vec_add(v_windows, window);
    }
  }

  calculate_windows(v_windows, &m_info.rcWork);

Cleanup:
  free(v_windows);
}

BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect) {
  if (windows->size == 0) {
    return TRUE;
  }

  size_t w_width = get_window_auto_width(windows, m_rect);
  size_t w_height = m_rect->bottom - m_rect->top;

  size_t l_shift = m_rect->left;

  for (size_t i = 0; i < windows->size; i++) {
    HWND window = windows->data[i]->handle;

    size_t width =
        windows->data[i]->user_sized
            ? windows->data[i]->w_rect.right - windows->data[i]->w_rect.left
            : w_width;

    RECT position = {.left = l_shift,
                     .right = l_shift + width,
                     .top = m_rect->top,
                     .bottom = m_rect->bottom};

    windows->data[i]->w_rect = position;
    l_shift += width;
  }

  return TRUE;
}

void position_workspaces_windows(WmParams *wm_params) {
  size_t w_num = get_windows_number(wm_params);

  HDWP hdwp = BeginDeferWindowPos(w_num);
  if (hdwp == NULL) {
    printf("Failed to position windows\n");
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

    if (!map_to_window_coords(window->handle, &w_rect)) {
      printf("Failed to get window gaps %s\n", windows->data[i]->process_name);
      continue;
    }

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
      print_last_error();
      return FALSE;
    }
  }

  return TRUE;
}

BOOL map_to_window_coords(HWND hwnd, LPRECT t_rect) {
  RECT real_w_rect = {0};
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                   &real_w_rect, sizeof(RECT)))) {
    return FALSE;
  }

  RECT w_rect = {0};
  if (!GetWindowRect(hwnd, &w_rect)) {
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

void get_process_name_by_window(HWND hwnd, char *p_name) {
  DWORD p_id = 0;
  if (!GetWindowThreadProcessId(hwnd, &p_id)) {
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

BOOL is_window_manageable(HWND hwnd) {
  if (!IsWindow(hwnd)) {
    return FALSE;
  }

  if (!IsWindowVisible(hwnd)) {
    return FALSE;
  }

  LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
  LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TOOLWINDOW || exStyle & WS_SYSMENU == 0 ||
      exStyle & WS_EX_APPWINDOW == 0 || !(style & WS_CAPTION)) {
    return FALSE;
  }

  int cloaked = 0;
  HRESULT hr =
      DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  if (SUCCEEDED(hr) && cloaked != 0) {
    return FALSE;
  }

  return TRUE;
}

BOOL add_window(HWND hwnd, WindowsVector *windows) {
  if (!is_window_manageable(hwnd)) {
    return FALSE;
  }

  Window *window = malloc(sizeof(Window));

  char *p_name = malloc(sizeof(char) * MAX_EXE_NAME_LENGTH);
  get_process_name_by_window(hwnd, p_name);

  window->process_name = p_name;
  window->handle = hwnd;
  window->user_sized = FALSE;

  windows_vec_add(windows, window);

  if (IsZoomed(window->handle)) {
    ShowWindow(window->handle, SW_RESTORE);
  }

  printf("Added window with process name \"%s\"\n", window->process_name);

  return TRUE;
}

BOOL remove_window(HWND hwnd, WorkspacesVector *workspaces) {
  for (size_t i = 0; i < workspaces->size; i++) {
    WindowsVector *windows = workspaces->data[i]->windows;

    for (size_t j = 0; j < windows->size; j++) {
      if (windows->data[j]->handle == hwnd) {
        printf("Removing window \"%s\" from namespace %zu\n",
               windows->data[j]->process_name, workspaces->data[i]->num);

        windows_vec_remove(windows, j);

        return TRUE;
      }
    }
  }

  return FALSE;
}

void set_active_workspace(HWND hwnd, WmParams *wm_params) {
  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    for (size_t j = 0; j < wm_params->workspaces->data[i]->windows->size; j++) {
      if (wm_params->workspaces->data[i]->windows->data[j]->handle == hwnd &&
          wm_params->workspaces->data[i]->num != wm_params->active_workspace) {
        wm_params->active_workspace = wm_params->workspaces->data[i]->num;
      }
    }
  }
}

void set_active_tracking(WmParams *wm_params) {
  BOOL active_tracking;

  if (!SystemParametersInfo(SPI_GETACTIVEWINDOWTRACKING, 0,
                            (void *)&active_tracking, 0)) {
    print_last_error();
    return;
  }

  if (active_tracking == TRUE) {
    wm_params->active_tracking = TRUE;
    return;
  }

  active_tracking = TRUE;
  if (!SystemParametersInfo(SPI_SETACTIVEWINDOWTRACKING, 0,
                            (void *)&active_tracking,
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    print_last_error();
    return;
  }

  wm_params->active_tracking = FALSE;
}

void restore_active_tracking(WmParams *wm_params) {
  if (wm_params->active_tracking == TRUE) {
    return;
  }

  BOOL *active_tracking = FALSE;
  if (!SystemParametersInfo(SPI_SETACTIVEWINDOWTRACKING, 0,
                            (void *)active_tracking,
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    print_last_error();
    return;
  }
}

void log_state(WmParams *wm_params) {
  printf("\n-------\n");
  printf("Active workspace: %zu\n\n", wm_params->active_workspace);

  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    Workspace *workspace = wm_params->workspaces->data[i];

    printf("Workspace %zu:\n", workspace->num);
    for (size_t j = 0; j < workspace->windows->size; j++) {
      if (workspace->windows->data[j] == NULL) {
        continue;
      }

      Window *window = workspace->windows->data[j];
      printf("  %s: position (l = %d t = %d r = %d b = %d); hwnd (%p);\n",
             window->process_name, window->w_rect.left, window->w_rect.top,
             window->w_rect.right, window->w_rect.bottom, window->handle);
    }
  }

  printf("\n-------\n");
}

BOOL is_window_tracked(HWND hwnd, WmParams *wm_params) {
  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    Workspace *workspace = wm_params->workspaces->data[i];

    for (size_t j = 0; j < workspace->windows->size; j++) {
      if (workspace->windows->data[j]->handle == hwnd) {
        return TRUE;
      }
    }
  }

  return FALSE;
}

BOOL window_width_changed(HWND hwnd, RECT w_rect, WmParams *wm_params) {
  Workspace *active_workspace = get_active_workspace(wm_params);
  Window *window = NULL;

  for (size_t i = 0; i < active_workspace->windows->size; i++) {
    if (active_workspace->windows->data[i]->handle == hwnd) {
      window = active_workspace->windows->data[i];
      break;
    }
  }

  if (window == NULL) {
    printf("window_width_changed: Couldn't get window\n");
    return FALSE;
  }

  BOOL same_width = (window->w_rect.right - window->w_rect.left) ==
                    (w_rect.right - w_rect.left);

  if (!same_width) {
    window->user_sized = TRUE;
    window->w_rect = w_rect;

    check_all_windows_user_sized(active_workspace, hwnd);
  }

  return !same_width;
}

void move_window(HWND hwnd, RECT w_rect, WmParams *wm_params) {
  int w_idx = -1;

  Workspace *active_workspace = get_active_workspace(wm_params);
  for (size_t i = 0; i < active_workspace->windows->size; i++) {
    if (active_workspace->windows->data[i]->handle == hwnd) {
      w_idx = i;
      break;
    }
  }

  if (w_idx < 0) {
    printf("Couldn't find a window to move\n");
    return;
  }

  // TODO: user can drag a window to a workspace on another monitor
  Window *window = active_workspace->windows->data[w_idx];

  int s_idx = -1;
  for (size_t i = 0; i < active_workspace->windows->size; i++) {
    if (i == w_idx) {
      continue;
    }

    Window *cur_window = active_workspace->windows->data[i];

    size_t mid_point = (cur_window->w_rect.right + cur_window->w_rect.left) / 2;
    BOOL swap = i < w_idx ? mid_point >= w_rect.left
                          : cur_window->w_rect.left <= w_rect.left;
    if (swap) {
      s_idx = i;

      if (s_idx < w_idx) {
        break;
      }
    }
  }

  if (s_idx < 0) {
    return;
  }

  rearrange_windows(active_workspace->windows, w_idx, s_idx);
}

void window_moved_or_resized(HWND hwnd, WmParams *wm_params) {
  RECT w_rect = {0};
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &w_rect,
                                   sizeof(RECT)))) {
    return;
  }

  if (!window_width_changed(hwnd, w_rect, wm_params)) {
    move_window(hwnd, w_rect, wm_params);
  }
}

size_t get_window_auto_width(WindowsVector *windows, LPRECT m_rect) {
  size_t m_width = m_rect->right - m_rect->left;
  size_t user_sized_count = 0;
  size_t reserved_width = 0;

  WindowsVector *user_sized_windows = malloc(sizeof(WindowsVector));
  INIT_VECTOR(user_sized_windows);

  for (size_t i = 0; i < windows->size; i++) {
    if (windows->data[i]->user_sized) {
      user_sized_count++;
      reserved_width +=
          windows->data[i]->w_rect.right - windows->data[i]->w_rect.left;
      windows_vec_add(user_sized_windows, windows->data[i]);
    }
  }

  if (reserved_width > m_width || windows->size == user_sized_count) {
    for (size_t i = 0; i < user_sized_windows->size; i++) {
      user_sized_windows->data[i]->user_sized = FALSE;
    }
    reserved_width = 0;
    user_sized_count = 0;
  }

  free(user_sized_windows);

  return (m_width - reserved_width) / (windows->size - user_sized_count);
}

void check_all_windows_user_sized(Workspace *active_workspace, HWND hwnd) {
  size_t user_sized_count = 0;

  for (size_t j = 0; j < active_workspace->windows->size; j++) {
    if (active_workspace->windows->data[j]->user_sized) {
      user_sized_count++;
    }
  }

  if (active_workspace->windows->size == user_sized_count) {
    for (size_t j = 0; j < active_workspace->windows->size; j++) {
      if (active_workspace->windows->data[j]->handle != hwnd) {
        active_workspace->windows->data[j]->user_sized = FALSE;
      }
    }
  }
}

void rearrange_windows(WindowsVector *windows, int w_idx, int s_idx) {
  Window *window = windows->data[w_idx];

  if ((w_idx - s_idx) > 0) {
    for (size_t i = w_idx; i > s_idx; i--) {
      windows->data[i] = windows->data[i - 1];
    }
  } else {
    for (size_t i = w_idx; i < s_idx; i++) {
      windows->data[i] = windows->data[i + 1];
    }
  }

  windows->data[s_idx] = window;
}

void print_last_error() {
  LPVOID lpMsgBuf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf, 0, NULL);
  wprintf(L"Last error: %hs", (LPCTSTR)lpMsgBuf);
  LocalFree(lpMsgBuf);
}