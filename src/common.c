#include "common.h"
#include <dwmapi.h>
#include <stdio.h>

BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect);
BOOL position_windows(HDWP hdwp, ITaskbarList *taskbar, WindowsVector *windows,
                      DWORD options);
BOOL is_window_manageable(HWND hwnd);
BOOL window_width_changed(HWND hwnd, RECT w_rect, WmParams *wm_params);
void move_window(HWND hwnd, RECT w_rect, WmParams *wm_params);

void check_all_windows_user_sized(Workspace *active_workspace, HWND hwnd);
BOOL map_to_window_coords(HWND hwnd, LPRECT t_rect);
size_t get_window_auto_width(WindowsVector *windows, LPRECT m_rect);
void rearrange_windows(WindowsVector *windows, int w_idx, int s_idx);
void restore_active_tracking(WmParams *wm_params);
BOOL is_window_ignored(StrVector *ignored_windows, char *p_name);

BOOL get_window_idx(Workspace *workspace, HWND hwnd, size_t *idx);
Workspace *get_workspace(WmParams *wm_params, size_t num);
BOOL active_window_belongs_to_active_workspace(WmParams *wm_params, HWND *hwnd);
void highlight_window(HWND hwnd, BOOL is_active);
void find_next_visible_window(WindowsVector *windows, size_t start_idx,
                              BOOL forward_search, size_t *idx);

void calculate_and_position_active_workspace(WmParams *wm_params) {
  Workspace *active_workspace = get_active_workspace(wm_params);

  calculate_workspace_windows(active_workspace);
  position_workspaces_windows(wm_params, 0);
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

    if (window->events_paused) {
      continue;
    }

    window->minimized = IsIconic(window->handle);

    if (!window->minimized) {
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
  size_t l_shift = m_rect->left;

  for (size_t i = 0; i < windows->size; i++) {
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

void position_workspaces_windows(WmParams *wm_params, DWORD options) {
  options = options & ~WP_VISIBLE;
  size_t w_num = get_windows_number(wm_params);

  HDWP hdwp = BeginDeferWindowPos(w_num);
  if (hdwp == NULL) {
    printf("Failed to position windows\n");
    return;
  }

  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    Workspace *workspace = wm_params->workspaces->data[i];

    if (workspace->num == wm_params->active_workspace) {
      if (!position_windows(hdwp, wm_params->taskbar, workspace->windows,
                            options | WP_VISIBLE)) {
        printf("Failed to position visible windows\n");
        return;
      }

      continue;
    }

    if (!position_windows(hdwp, wm_params->taskbar, workspace->windows,
                          options)) {
      printf("Failed to position invisible windows\n");
      return;
    }
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed to position windows\n");
  }

  log_state(wm_params);
}

BOOL position_windows(HDWP hdwp, ITaskbarList *taskbar, WindowsVector *windows,
                      DWORD options) {
  if (windows->size == 0) {
    return TRUE;
  }

  BOOL visible = options & WP_VISIBLE;
  BOOL shutdown = options & WP_SHUTDOWN;

  HWND f_hwnd = GetForegroundWindow();

  for (size_t i = 0; i < windows->size; i++) {
    if (windows->data[i]->minimized) {
      continue;
    }

    Window *window = windows->data[i];
    RECT w_rect = window->w_rect;

    highlight_window(window->handle, !shutdown && window->handle == f_hwnd);

    if (!map_to_window_coords(window->handle, &w_rect)) {
      printf("Failed to get window gaps %s\n", windows->data[i]->process_name);
      continue;
    }

    if (!visible && !shutdown) {
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

    if (visible) {
      taskbar->lpVtbl->AddTab(taskbar, window->handle);
    } else {
      taskbar->lpVtbl->DeleteTab(taskbar, window->handle);
    }
  }

  return TRUE;
}

void move_active_window_to_workspace(WmParams *wm_params, size_t num) {
  HWND hwnd = NULL;
  if (!active_window_belongs_to_active_workspace(wm_params, &hwnd)) {
    return;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);

  Workspace *new_window_workspace =
      get_or_create_workspace(wm_params, num, active_workspace->monitor);

  if (new_window_workspace->num == wm_params->active_workspace) {
    return;
  }

  Window *window_to_move = NULL;
  remove_window(hwnd, wm_params->workspaces, &window_to_move);
  windows_vec_add(new_window_workspace->windows, window_to_move);
  wm_params->active_workspace = num;

  calculate_and_position_active_workspace(wm_params);
}

void close_window(WmParams *wm_params) {
  HWND hwnd = NULL;
  if (!active_window_belongs_to_active_workspace(wm_params, &hwnd)) {
    return;
  }

  PostMessage(hwnd, WM_CLOSE, 0, 0);
}

void toggle_wm_pause(WmParams *wm_params) {
  wm_params->pause_wm = !wm_params->pause_wm;

  if (!wm_params->pause_wm) {
    Workspace *active_workspace = get_active_workspace(wm_params);
    calculate_workspace_windows(active_workspace);
  }

  position_workspaces_windows(wm_params,
                              wm_params->pause_wm ? WM_SHOWWINDOW : 0);
}

void toggle_window_pause(WmParams *wm_params) {
  HWND hwnd = NULL;
  if (!active_window_belongs_to_active_workspace(wm_params, &hwnd)) {
    return;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);

  size_t idx;
  if (!get_window_idx(active_workspace, hwnd, &idx)) {
    return;
  }

  active_workspace->windows->data[idx]->events_paused =
      !active_workspace->windows->data[idx]->events_paused;

  calculate_and_position_active_workspace(wm_params);
}

void minimize_restore(WmParams *wm_params) {
  HWND hwnd = NULL;
  if (!active_window_belongs_to_active_workspace(wm_params, &hwnd)) {
    return;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);

  size_t idx;
  if (!get_window_idx(active_workspace, hwnd, &idx)) {
    return;
  }

  int command = active_workspace->windows->data[idx]->minimized ? SW_RESTORE
                                                                : SW_MINIMIZE;
  ShowWindow(hwnd, command);

  calculate_and_position_active_workspace(wm_params);

  focus_window_if_none_focused(wm_params, hwnd);
}

void swap_windows(WmParams *wm_params, DWORD direction) {
  if (direction != MOD_ALT_SHIFT_LEFT && direction != MOD_ALT_SHIFT_RIGHT) {
    return;
  }

  HWND hwnd = NULL;
  if (!active_window_belongs_to_active_workspace(wm_params, &hwnd)) {
    return;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);

  size_t idx;
  if (!get_window_idx(active_workspace, hwnd, &idx)) {
    return;
  }

  size_t target_idx;
  find_next_visible_window(active_workspace->windows, idx,
                           direction == MOD_ALT_SHIFT_RIGHT, &target_idx);

  if (target_idx == idx) {
    return;
  }

  Window *tmp_window = active_workspace->windows->data[idx];
  active_workspace->windows->data[idx] =
      active_workspace->windows->data[target_idx];
  active_workspace->windows->data[target_idx] = tmp_window;

  calculate_and_position_active_workspace(wm_params);
}

void move_focus(WmParams *wm_params, DWORD direction) {
  if (direction != MOD_ALT_LEFT && direction != MOD_ALT_RIGHT) {
    return;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);

  HWND hwnd = NULL;
  if (!active_window_belongs_to_active_workspace(wm_params, &hwnd)) {
    if (active_workspace->windows->size > 0) {
      SetForegroundWindow(active_workspace->windows->data[0]->handle);
    }

    return;
  }

  size_t idx;
  if (!get_window_idx(active_workspace, hwnd, &idx)) {
    return;
  }

  size_t target_idx;
  find_next_visible_window(active_workspace->windows, idx,
                           direction == MOD_ALT_RIGHT, &target_idx);

  if (target_idx == idx) {
    return;
  }

  SetForegroundWindow(active_workspace->windows->data[target_idx]->handle);
}

void highlight_window(HWND hwnd, BOOL is_active) {
  COLORREF color = is_active ? WINDOW_HIGHLIGHT_ON : WINDOW_HIGHLIGHT_OFF;
  HRESULT hr =
      DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(color));

  if (FAILED(hr)) {
    printf("Failed to highlight active workspace windows\n");
  }
}

void restore_windows(WmParams *wm_params) {
  position_workspaces_windows(wm_params, WP_SHUTDOWN);
  // restore_active_tracking(wm_params);
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

Workspace *select_workspace(WmParams *wm_params, size_t num) {
  Workspace *active_workspace = get_active_workspace(wm_params);

  if (num == wm_params->active_workspace) {
    return active_workspace;
  }

  Workspace *workspace =
      get_or_create_workspace(wm_params, num, active_workspace->monitor);

  wm_params->active_workspace = num;
  calculate_and_position_active_workspace(wm_params);

  return workspace;
}

Workspace *get_or_create_workspace(WmParams *wm_params, size_t num,
                                   HMONITOR hmonitor) {
  Workspace *workspace = get_workspace(wm_params, num);
  if (workspace != NULL) {
    return workspace;
  }

  Workspace *new_workspace = malloc(sizeof(Workspace));
  workspaces_vec_add(wm_params->workspaces, new_workspace);

  new_workspace->num = num;
  new_workspace->monitor = hmonitor;
  new_workspace->windows = malloc(sizeof(WindowsVector));
  INIT_VECTOR(new_workspace->windows);

  return new_workspace;
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

BOOL add_window(HWND hwnd, WindowsVector *windows, StrVector *ignored_windows) {
  if (!is_window_manageable(hwnd)) {
    return FALSE;
  }

  Window *window = malloc(sizeof(Window));

  window->process_name = malloc(sizeof(char) * MAX_EXE_NAME_LENGTH);
  get_process_name_by_window(hwnd, window->process_name);

  if (is_window_ignored(ignored_windows, window->process_name)) {
    free(window->process_name);
    free(window);
    return TRUE;
  }

  window->handle = hwnd;
  window->user_sized = FALSE;
  window->minimized = FALSE;
  window->events_paused = FALSE;

  windows_vec_add(windows, window);

  if (IsZoomed(window->handle)) {
    ShowWindow(window->handle, SW_RESTORE);
  }

  printf("Added window with process name \"%s\"\n", window->process_name);

  return TRUE;
}

BOOL remove_window(HWND hwnd, WorkspacesVector *workspaces,
                   Window **removed_window) {
  for (size_t i = 0; i < workspaces->size; i++) {
    WindowsVector *windows = workspaces->data[i]->windows;

    for (size_t j = 0; j < windows->size; j++) {
      if (windows->data[j]->handle == hwnd) {
        printf("Removing window \"%s\" from namespace %zu\n",
               windows->data[j]->process_name, workspaces->data[i]->num);

        *removed_window = windows->data[j];
        windows_vec_remove(windows, j);

        return TRUE;
      }
    }
  }

  return FALSE;
}

BOOL remove_window_with_cleanup(HWND hwnd, WorkspacesVector *workspaces) {
  Window *window_to_remove = NULL;
  if (remove_window(hwnd, workspaces, &window_to_remove)) {
    cleanup_window(window_to_remove);

    return TRUE;
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
  if (!SystemParametersInfo(SPI_GETACTIVEWINDOWTRACKING, 0,
                            (void *)&wm_params->active_tracking, 0)) {
    print_last_error();
    return;
  }

  if (!SystemParametersInfo(SPI_SETACTIVEWINDOWTRACKING, 0, (void *)TRUE,
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    print_last_error();
    return;
  }

  if (!SystemParametersInfo(SPI_GETACTIVEWNDTRKZORDER, 0,
                            (void *)&wm_params->active_zorder, 0)) {
    print_last_error();
    return;
  }

  if (!SystemParametersInfo(SPI_SETACTIVEWNDTRKZORDER, 0, (void *)TRUE,
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    print_last_error();
    return;
  }
}

void restore_active_tracking(WmParams *wm_params) {
  if (!SystemParametersInfo(SPI_SETACTIVEWINDOWTRACKING, 0,
                            (void *)(UINT_PTR)wm_params->active_tracking,
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    print_last_error();
    return;
  }

  if (!SystemParametersInfo(SPI_SETACTIVEWNDTRKZORDER, 0,
                            (void *)(UINT_PTR)wm_params->active_zorder,
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

BOOL skip_window(WmParams *wm_params, HWND hwnd) {
  char p_name[MAX_EXE_NAME_LENGTH];
  get_process_name_by_window(hwnd, p_name);

  if (is_window_ignored(wm_params->ignored_windows, p_name)) {
    return TRUE;
  }

  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    size_t idx;
    if (get_window_idx(wm_params->workspaces->data[i], hwnd, &idx)) {
      return wm_params->workspaces->data[i]->windows->data[idx]->events_paused;
    }
  }

  return FALSE;
}

void save_window_position(HWND hwnd, WmParams *wm_params) {
  Workspace *active_workspace = get_active_workspace(wm_params);

  size_t idx;
  if (get_window_idx(active_workspace, hwnd, &idx)) {
    GetWindowRect(hwnd, &active_workspace->windows->data[idx]->w_rect);
  }
}

BOOL is_window_ignored(StrVector *ignored_windows, char *p_name) {
  for (size_t i = 0; i < ignored_windows->size; i++) {
    if (strcmp(ignored_windows->data[i], p_name) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

BOOL get_window_idx(Workspace *workspace, HWND hwnd, size_t *idx) {
  for (size_t i = 0; i < workspace->windows->size; i++) {
    if (workspace->windows->data[i]->handle == hwnd) {
      *idx = i;
      return TRUE;
    }
  }

  return FALSE;
}

Workspace *get_workspace(WmParams *wm_params, size_t num) {
  for (size_t i = 0; i < wm_params->workspaces->size; i++) {
    if (wm_params->workspaces->data[i]->num == num) {
      return wm_params->workspaces->data[i];
    }
  }

  return NULL;
}

BOOL active_window_belongs_to_active_workspace(WmParams *wm_params,
                                               HWND *hwnd) {
  *hwnd = GetForegroundWindow();
  if (hwnd == NULL) {
    printf("No windows are active at the moment\n");
    return FALSE;
  }

  Workspace *active_workspace = get_active_workspace(wm_params);

  size_t idx;
  if (!get_window_idx(active_workspace, *hwnd, &idx)) {
    printf("Active window doesn't belong to the active "
           "workspace\n",
           *hwnd);
    return FALSE;
  }

  return TRUE;
}

void find_next_visible_window(WindowsVector *windows, size_t start_idx,
                              BOOL forward_search, size_t *idx) {
  *idx = start_idx;
  if (forward_search && start_idx >= windows->size - 1 ||
      !forward_search && start_idx == 0) {
    return;
  }

  if (forward_search) {
    for (size_t i = start_idx + 1; i < windows->size; i++) {
      if (!windows->data[i]->minimized || !windows->data[i]->events_paused) {
        *idx = i;
        return;
      }
    }

    return;
  }

  for (int i = start_idx - 1; i >= 0; i--) {
    if (!windows->data[i]->minimized || !windows->data[i]->events_paused) {
      *idx = i;
      return;
    }
  }
}

void focus_window_if_none_focused(WmParams *wm_params, HWND last_focused_hwnd) {
  Workspace *active_workspace = get_active_workspace(wm_params);

  if (active_workspace->windows->size == 0) {
    return;
  }

  for (size_t i = 0; i < active_workspace->windows->size; i++) {
    if (!active_workspace->windows->data[i]->minimized ||
        !active_workspace->windows->data[i]->events_paused) {
      return;
    }
  }

  size_t next_window_idx;
  if (!get_window_idx(active_workspace, last_focused_hwnd, &next_window_idx)) {
    next_window_idx = 0;
  } else {
    next_window_idx = next_window_idx + 1 >= active_workspace->windows->size
                          ? 0
                          : next_window_idx + 1;
  }

  SetForegroundWindow(active_workspace->windows->data[next_window_idx]->handle);
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