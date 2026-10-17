#include "structs.h"

void calculate_windows_init(WmParams *wm_params);

void position_windows_init(WmParams *wm_params);

BOOL position_windows(HDWP hdwp, WindowsVector *windows, BOOL visible);

BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect);

BOOL map_to_window_coords(HWND hwnd, LPRECT t_rect);

size_t get_windows_number(WmParams *wm_params);

Workspace *get_active_workspace(WmParams *wm_params);

BOOL add_window(HWND hwnd, WindowsVector *windows);

void remove_window(SAFEARRAY *rid, WorkspacesVector *workspaces);

void cleanup_workspace_windows(Workspace *workspace);

void log_state(WmParams *wm_params);

BOOL is_window_tracked(HWND hwnd, WmParams *wm_params);