#include "structs.h"

#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

BOOL init_uiautomation();
void start_event_loop(WmParams *wm_params);
HRESULT get_window_runtime_id(HWND hwnd, SAFEARRAY **rid);
HRESULT compare_runtime_ids(SAFEARRAY *rid1, SAFEARRAY *rid2, BOOL *result);

#endif