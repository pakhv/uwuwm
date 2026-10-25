#include "structs.h"

void calculate_and_position_active_workspace(WmParams *params);

void calculate_windows_init(WmParams *wm_params);

void position_workspaces_windows(WmParams *wm_params);

void calculate_workspace_windows(Workspace *workspace);

size_t get_windows_number(WmParams *wm_params);

Workspace *get_active_workspace(WmParams *wm_params);

BOOL add_window(HWND hwnd, WindowsVector *windows);

BOOL remove_window(HWND hwnd, WorkspacesVector *workspaces);

void log_state(WmParams *wm_params);

BOOL is_window_tracked(HWND hwnd, WmParams *wm_params);

void window_moved_or_resized(HWND hwnd, WmParams *wm_params);

void print_last_error();