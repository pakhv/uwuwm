#include "structs.h"

Workspace *get_or_create_workspace(WmParams *wm_params, size_t num,
                                   HMONITOR hmonitor);

void calculate_and_position_active_workspace(WmParams *params);

void calculate_windows_init(WmParams *wm_params);

void position_workspaces_windows(WmParams *wm_params, DWORD options);

void calculate_workspace_windows(Workspace *workspace);

void move_active_window_to_workspace(WmParams *wm_params, size_t num);

void swap_windows(WmParams *wm_params, DWORD direction);

void move_focus(WmParams *wm_params, DWORD direction);

size_t get_windows_number(WmParams *wm_params);

Workspace *get_active_workspace(WmParams *wm_params);

Workspace *select_workspace(WmParams *wm_params, size_t num);

BOOL add_window(HWND hwnd, WindowsVector *windows, StrVector *ignored_windows);

BOOL remove_window(HWND hwnd, WorkspacesVector *workspaces,
                   Window **removed_window);

BOOL remove_window_with_cleanup(HWND hwnd, WorkspacesVector *workspaces);

void close_window(WmParams *wm_params);

void toggle_pause(WmParams *wm_params);

void log_state(WmParams *wm_params);

BOOL is_window_tracked(HWND hwnd, WmParams *wm_params);

void window_moved_or_resized(HWND hwnd, WmParams *wm_params);

void set_active_workspace(HWND hwnd, WmParams *wm_params);

void set_active_tracking(WmParams *wm_params);

void restore_windows(WmParams *wm_params);

void print_last_error();