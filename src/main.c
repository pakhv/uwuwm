#define COBJMACROS

#include "main.h"
#include <Windows.h>
#include <dwmapi.h>
#include <fcntl.h>
#include <io.h>
#include <locale.h>
#include <stdio.h>
#include <uiautomation.h>

void wnd_array_add(Vector_Window *arr, Window *handle);
BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param);
BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param);
void position_windows(Vector_Window *arr);

void init(Wm_params *wm_params) {
  wm_params->active_workspace = 1;
  wm_params->windows = malloc(sizeof(Vector_Window));
  wm_params->workspaces = malloc(sizeof(Vector_Workspace));

  INIT_VECTOR(wm_params->windows)
  INIT_VECTOR(wm_params->workspaces)

  EnumDisplayMonitors(NULL, NULL, enum_monitors_proc,
                      (LPARAM)wm_params->workspaces);
  EnumWindows(enum_windows_proc, (LPARAM)wm_params);
}

void main(void) {
  Wm_params wm_params = {0};
  init(&wm_params);

  Vector_Window *windows = wm_params.windows;

  printf("Windows num: %zu\n", windows->size);
  printf("Workspaces num: %zu\n", wm_params.workspaces->size);

  position_windows(windows);

  free(windows);
  free(wm_params.workspaces);
}

void position_windows(Vector_Window *windows) {
  HDWP hdwp = BeginDeferWindowPos(windows->size);
  if (hdwp == NULL) {
    printf("Failed\n");
    return;
  }

  for (size_t i = 0; i < windows->size; i++) {
    HWND wnd = windows->data[i].handle;

    if (IsZoomed(wnd)) {
      ShowWindow(wnd, SW_RESTORE);
    }

    HDWP def_hdpw = DeferWindowPos(hdwp, wnd, NULL, 10, 10, 400, 400,
                                   SWP_NOZORDER | SWP_NOACTIVATE);

    if (def_hdpw == NULL) {
      printf("Failed: %lu\n", GetLastError());
      return;
    }
  }

  if (!EndDeferWindowPos(hdwp)) {
    printf("Failed\n");
  }
}

void workspace_array_add(Vector_Workspace *arr, Workspace *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(Workspace));
  }

  arr->data[arr->size] = *el;
  arr->size += 1;
}

void wnd_array_add(Vector_Window *arr, Window *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(Window));
  }

  arr->data[arr->size] = *el;
  arr->size += 1;
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

void get_process_name_by_window(HWND h_wnd, char *p_name) {
  DWORD p_id = 0;
  if (!GetWindowThreadProcessId(h_wnd, &p_id)) {
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

BOOL CALLBACK enum_windows_proc(HWND h_wnd, LPARAM l_param) {
  Wm_params *wm_params = (Wm_params *)l_param;
  Vector_Window *windows = wm_params->windows;

  if (!IsWindowVisible(h_wnd)) {
    return TRUE;
  }

  LONG_PTR exStyle = GetWindowLongPtr(h_wnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TOOLWINDOW) {
    return TRUE;
  }

  int cloaked = 0;
  HRESULT hr =
      DwmGetWindowAttribute(h_wnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  if (SUCCEEDED(hr) && cloaked != 0) {
    return TRUE;
  }

  Window *window = malloc(sizeof(Window));
  window->handle = h_wnd;
  // assigning first workspace to all windows
  window->workspace = &wm_params->workspaces->data[0];

  char *p_name = malloc(sizeof(char) * MAX_EXE_NAME_LENGTH);
  get_process_name_by_window(h_wnd, p_name);
  window->process_name = p_name;

  printf("%s\n", window->process_name);

  wnd_array_add(windows, window);

  return TRUE;
}

BOOL CALLBACK enum_monitors_proc(HMONITOR h_monitor, HDC h_dev, LPRECT rect,
                                 LPARAM l_param) {
  Vector_Workspace *workspaces = (Vector_Workspace *)l_param;

  Workspace *workspace = malloc(sizeof(Workspace));
  workspace->monitor = h_monitor;
  workspace->num = workspaces->size + 1;

  workspace_array_add(workspaces, workspace);

  return TRUE;
}