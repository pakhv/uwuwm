#include "structs.h"

void calculate_windows_init(WmParams *params);
void position_windows_init(WmParams *wm_params);

BOOL position_windows(HDWP hdwp, WindowsVector *windows, BOOL visible);
BOOL calculate_windows(WindowsVector *windows, LPRECT m_rect);

BOOL map_to_window_coords(HWND window, LPRECT t_rect);
size_t get_windows_number(WmParams *wm_params);
Workspace *get_active_workspace(WmParams *wm_params);