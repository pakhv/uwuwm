#include "common.h"

static HWND g_hidden_wnd = NULL;
WmParams *g_wm_params = NULL;

VOID handle_win_events(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG o_id,
                       LONG c_id, DWORD t_id, DWORD time);

BOOL WINAPI exit_handler(DWORD c_type);
LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT msg, WPARAM w_param,
                                    LPARAM l_param);
void create_hidden_window(WNDCLASSEX *wc);

void start_event_loop(WmParams *wm_params) {
  WNDCLASSEX wc = {0};
  create_hidden_window(&wc);

  if (!SetConsoleCtrlHandler(exit_handler, TRUE)) {
    printf("Could not set control handler\n");
    return;
  }

  g_wm_params = wm_params;

  HWINEVENTHOOK win_hook = SetWinEventHook(
      EVENT_SYSTEM_MOVESIZEEND, EVENT_OBJECT_UNCLOAKED, NULL, handle_win_events,
      0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  printf("Successfully exited the event loop\n");

  UnhookWinEvent(win_hook);
}

VOID handle_win_events(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG o_id,
                       LONG c_id, DWORD t_id, DWORD time) {
  // if (o_id != OBJID_WINDOW || c_id != CHILDID_SELF) {
  //   return;
  // }

  if (event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_UNCLOAKED) {
    Workspace *active_workspace = get_active_workspace(g_wm_params);

    if (!is_window_tracked(hwnd, g_wm_params)) {
      if (!add_window(hwnd, active_workspace->windows)) {
        return;
      }

      calculate_workspace_windows(active_workspace);
      position_workspaces_windows(g_wm_params);
    }
  }

  if (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_CLOAKED) {
    if (!remove_window(hwnd, g_wm_params->workspaces)) {
      return;
    }

    Workspace *active_workspace = get_active_workspace(g_wm_params);
    calculate_workspace_windows(active_workspace);
    position_workspaces_windows(g_wm_params);
  }

  if (event == EVENT_SYSTEM_MINIMIZESTART ||
      event == EVENT_SYSTEM_MINIMIZEEND || event == EVENT_SYSTEM_MOVESIZEEND) {
    Workspace *active_workspace = get_active_workspace(g_wm_params);
    calculate_workspace_windows(active_workspace);
    position_workspaces_windows(g_wm_params);
  }
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