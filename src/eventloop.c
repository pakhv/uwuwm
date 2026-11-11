#include "common.h"

static HWND g_hidden_wnd = NULL;
WmParams *g_wm_params = NULL;
CRITICAL_SECTION g_lock;

VOID handle_win_events(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG o_id,
                       LONG c_id, DWORD t_id, DWORD time);
BOOL is_known_event(DWORD event);

void register_keys();
void unregister_keys();
void handle_key_presses(WPARAM w_param, WmParams *wm_params);

BOOL WINAPI exit_handler(DWORD c_type);
LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT msg, WPARAM w_param,
                                    LPARAM l_param);
void create_hidden_window(WNDCLASSEX *wc);

void start_event_loop(WmParams *wm_params) {
  InitializeCriticalSection(&g_lock);
  g_wm_params = wm_params;

  WNDCLASSEX wc = {0};
  create_hidden_window(&wc);

  if (!SetConsoleCtrlHandler(exit_handler, TRUE)) {
    printf("Could not set control handler\n");
    return;
  }

  register_keys();
  HWINEVENTHOOK win_hook = SetWinEventHook(
      EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_UNCLOAKED, NULL, handle_win_events,
      0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (msg.message == WM_HOTKEY) {
      handle_key_presses(msg.wParam, wm_params);
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  printf("Successfully exited the event loop\n");

  unregister_keys();
  UnhookWinEvent(win_hook);

  DeleteCriticalSection(&g_lock);
}

void register_keys() {
  for (size_t i = MOD_ALT_1, j = MOD_ALT_SHIFT_1; i <= MOD_ALT_9; i++, j++) {
    if (!RegisterHotKey(NULL, i, MOD_ALT | 0x4000, KEY_0 + i)) {
      printf("Failed to register Alt+%zu", i);
    }
    if (!RegisterHotKey(NULL, j, MOD_ALT | MOD_SHIFT | 0x4000, KEY_0 + i)) {
      printf("Failed to register Alt+Shift+%zu", i);
    }
  }

  if (!RegisterHotKey(NULL, MOD_ALT_SHIFT_Q, MOD_ALT | MOD_SHIFT | 0x4000,
                      KEY_Q)) {
    printf("Failed to register Alt+Shift+Q");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_SHIFT_P, MOD_ALT | MOD_SHIFT | 0x4000,
                      KEY_P)) {
    printf("Failed to register Alt+Shift+P");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_P, MOD_ALT | 0x4000, KEY_P)) {
    printf("Failed to register Alt+P");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_SHIFT_LEFT, MOD_ALT | MOD_SHIFT | 0x4000,
                      VK_LEFT)) {
    printf("Failed to register Alt+Shift+Left");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_SHIFT_RIGHT, MOD_ALT | MOD_SHIFT | 0x4000,
                      VK_RIGHT)) {
    printf("Failed to register Alt+Shift+Right");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_LEFT, MOD_ALT | 0x4000, VK_LEFT)) {
    printf("Failed to register Alt+Left");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_RIGHT, MOD_ALT | 0x4000, VK_RIGHT)) {
    printf("Failed to register Alt+Right");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_M, MOD_ALT | 0x4000, KEY_M)) {
    printf("Failed to register Alt+M");
  }

  if (!RegisterHotKey(NULL, MOD_ALT_SHIFT_M, MOD_ALT | MOD_SHIFT | 0x4000,
                      KEY_M)) {
    printf("Failed to register Alt+M");
  }
}

void unregister_keys() {
  for (size_t i = MOD_ALT_1, j = MOD_ALT_SHIFT_1; i <= MOD_ALT_9; i++, j++) {
    UnregisterHotKey(NULL, i);
    UnregisterHotKey(NULL, j);
  }

  UnregisterHotKey(NULL, MOD_ALT_SHIFT_Q);
  UnregisterHotKey(NULL, MOD_ALT_SHIFT_P);
  UnregisterHotKey(NULL, MOD_ALT_P);

  UnregisterHotKey(NULL, MOD_ALT_SHIFT_LEFT);
  UnregisterHotKey(NULL, MOD_ALT_SHIFT_RIGHT);

  UnregisterHotKey(NULL, MOD_ALT_LEFT);
  UnregisterHotKey(NULL, MOD_ALT_RIGHT);

  UnregisterHotKey(NULL, MOD_ALT_M);
  UnregisterHotKey(NULL, MOD_ALT_SHIFT_M);
}

VOID handle_win_events(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG o_id,
                       LONG c_id, DWORD t_id, DWORD time) {
  if (o_id != OBJID_WINDOW || c_id != CHILDID_SELF) {
    return;
  }

  if (!is_known_event(event)) {
    return;
  }

  EnterCriticalSection(&g_lock);

  if (g_wm_params->pause_wm) {
    goto Release;
  }

  // window opened
  if (event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_UNCLOAKED) {
    if (is_window_tracked(hwnd, g_wm_params)) {
      goto Release;
    }

    Workspace *active_workspace = get_active_workspace(g_wm_params);
    if (!add_window(hwnd, active_workspace->windows,
                    g_wm_params->ignored_windows)) {
      goto Release;
    }

    calculate_and_position_active_workspace(g_wm_params);
    goto Release;
  }

  // window closed
  if (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_CLOAKED) {
    if (!remove_window_with_cleanup(hwnd, g_wm_params->workspaces)) {
      goto Release;
    }

    calculate_and_position_active_workspace(g_wm_params);
    goto Release;
  }

  if (skip_window(g_wm_params, hwnd)) {
    if (event == EVENT_SYSTEM_MOVESIZEEND) {
      save_window_position(hwnd, g_wm_params);
    }
    goto Release;
  }

  // window resized/moved
  if (event == EVENT_SYSTEM_MOVESIZEEND) {
    window_moved_or_resized(hwnd, g_wm_params);
  }

  // TODO:
  if (event == EVENT_OBJECT_FOCUS) {
    set_active_workspace(hwnd, g_wm_params);
  }

  // focused window changed
  // TODO: if there are noticeable perf issues when highlighting
  // then add dedicated function
  if (event == EVENT_SYSTEM_FOREGROUND) {
  }

  calculate_and_position_active_workspace(g_wm_params);

  // focus a window after minimizing or closing a window
  // if all active workspace windows are minimized
  if (event == EVENT_SYSTEM_MINIMIZESTART || event == EVENT_OBJECT_DESTROY ||
      event == EVENT_OBJECT_CLOAKED) {
    focus_window_if_none_focused(g_wm_params, hwnd);
  }

Release:
  LeaveCriticalSection(&g_lock);
}

void handle_key_presses(WPARAM w_param, WmParams *wm_params) {
  EnterCriticalSection(&g_lock);

  if (wm_params->pause_wm && w_param != MOD_ALT_SHIFT_P) {
    goto Release;
  }

  switch (w_param) {
  case MOD_ALT_1:
  case MOD_ALT_2:
  case MOD_ALT_3:
  case MOD_ALT_4:
  case MOD_ALT_5:
  case MOD_ALT_6:
  case MOD_ALT_7:
  case MOD_ALT_8:
  case MOD_ALT_9:
    select_workspace(wm_params, w_param);
    break;
  case MOD_ALT_SHIFT_1:
  case MOD_ALT_SHIFT_2:
  case MOD_ALT_SHIFT_3:
  case MOD_ALT_SHIFT_4:
  case MOD_ALT_SHIFT_5:
  case MOD_ALT_SHIFT_6:
  case MOD_ALT_SHIFT_7:
  case MOD_ALT_SHIFT_8:
  case MOD_ALT_SHIFT_9:
    move_active_window_to_workspace(wm_params, w_param - MOD_ALT_SHIFT_DELTA);
    break;
  case MOD_ALT_SHIFT_Q:
    close_window(wm_params);
    break;
  case MOD_ALT_P:
    toggle_window_pause(wm_params);
    break;
  case MOD_ALT_SHIFT_P:
    toggle_wm_pause(wm_params);
    break;
  case MOD_ALT_SHIFT_LEFT:
  case MOD_ALT_SHIFT_RIGHT:
    swap_windows(wm_params, w_param);
    break;
  case MOD_ALT_LEFT:
  case MOD_ALT_RIGHT:
    move_focus(wm_params, w_param);
    break;
  case MOD_ALT_M:
    minimize_restore(wm_params);
    break;
  default:
    break;
  }

Release:
  LeaveCriticalSection(&g_lock);
}

BOOL is_known_event(DWORD event) {
  return event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_UNCLOAKED ||
         event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_CLOAKED ||
         event == EVENT_SYSTEM_MINIMIZESTART ||
         event == EVENT_SYSTEM_MINIMIZEEND ||
         event == EVENT_SYSTEM_MOVESIZEEND || event == EVENT_SYSTEM_FOREGROUND;
}

void create_hidden_window(WNDCLASSEX *wc) {
  wc->cbSize = sizeof(WNDCLASSEX);
  wc->lpfnWndProc = hidden_window_proc;
  wc->hInstance = GetModuleHandle(NULL);
  wc->lpszClassName = "uwu_hidden_window";
  RegisterClassEx(wc);

  g_hidden_wnd = CreateWindowEx(0, wc->lpszClassName, NULL, 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, wc->hInstance, NULL);
}

BOOL WINAPI exit_handler(DWORD c_type) {
  if ((c_type == CTRL_C_EVENT || c_type == CTRL_CLOSE_EVENT) &&
      g_hidden_wnd != NULL) {
    PostMessage(g_hidden_wnd, WM_CLOSE, 0, 0);
    return TRUE;
  }

  return FALSE;
}

LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT msg, WPARAM w_param,
                                    LPARAM l_param) {
  switch (msg) {
  case WM_CLOSE:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hwnd, msg, w_param, l_param);
}