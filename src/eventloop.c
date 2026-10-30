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
      EVENT_SYSTEM_MOVESIZEEND, EVENT_OBJECT_UNCLOAKED, NULL, handle_win_events,
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
  for (size_t i = MOD_ALT_1; i <= MOD_ALT_9; i++) {
    if (!RegisterHotKey(NULL, i, MOD_ALT | 0x4000, 0x30 + i)) {
      printf("Failed to register Alt+%zu", i);
    }
  }
}

void unregister_keys() {
  for (size_t i = MOD_ALT_1; i <= MOD_ALT_9; i++) {
    UnregisterHotKey(NULL, i);
  }
}

VOID handle_win_events(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG o_id,
                       LONG c_id, DWORD t_id, DWORD time) {
  EnterCriticalSection(&g_lock);

  if (o_id != OBJID_WINDOW || c_id != CHILDID_SELF) {
    goto Release;
  }

  if (!is_known_event(event)) {
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
  }

  // window closed
  if (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_CLOAKED) {
    if (!remove_window(hwnd, g_wm_params->workspaces)) {
      goto Release;
    }
  }

  // window minimized/maximized
  if (event == EVENT_SYSTEM_MINIMIZESTART ||
      event == EVENT_SYSTEM_MINIMIZEEND) {
  }

  // window resized/moved
  if (event == EVENT_SYSTEM_MOVESIZEEND) {
    window_moved_or_resized(hwnd, g_wm_params);
  }

  // TODO:
  if (event == EVENT_OBJECT_FOCUS) {
    set_active_workspace(hwnd, g_wm_params);
  }

  calculate_and_position_active_workspace(g_wm_params);

Release:
  LeaveCriticalSection(&g_lock);
}

void handle_key_presses(WPARAM w_param, WmParams *wm_params) {
  EnterCriticalSection(&g_lock);

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
  default:
    break;
  }

  LeaveCriticalSection(&g_lock);
}

BOOL is_known_event(DWORD event) {
  return event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_UNCLOAKED ||
         event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_CLOAKED ||
         event == EVENT_SYSTEM_MINIMIZESTART ||
         event == EVENT_SYSTEM_MINIMIZEEND || event == EVENT_SYSTEM_MOVESIZEEND;
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