#include <Windows.h>
#include <shobjidl.h>

#ifndef STRUCTS_H
#define STRUCTS_H

#define HIDDEN_WINDOW_X -3900
#define MAX_EXE_PATH_LENGTH 300
#define MAX_EXE_NAME_LENGTH 100

#define KEY_0 0x30
#define MOD_ALT_1 1
#define MOD_ALT_2 2
#define MOD_ALT_3 3
#define MOD_ALT_4 4
#define MOD_ALT_5 5
#define MOD_ALT_6 6
#define MOD_ALT_7 7
#define MOD_ALT_8 8
#define MOD_ALT_9 9

#define MOD_ALT_SHIFT_DELTA 10
#define MOD_ALT_SHIFT_1 11
#define MOD_ALT_SHIFT_2 12
#define MOD_ALT_SHIFT_3 13
#define MOD_ALT_SHIFT_4 14
#define MOD_ALT_SHIFT_5 15
#define MOD_ALT_SHIFT_6 16
#define MOD_ALT_SHIFT_7 17
#define MOD_ALT_SHIFT_8 18
#define MOD_ALT_SHIFT_9 19

#define KEY_P 0x50
#define KEY_Q 0x51
#define KEY_M 0x4D

// close
#define MOD_ALT_SHIFT_Q 20
// swap left
#define MOD_ALT_SHIFT_LEFT 21
// swap right
#define MOD_ALT_SHIFT_RIGHT 22
// move focus left
#define MOD_ALT_LEFT 23
// move focus right
#define MOD_ALT_RIGHT 24
// pause wm
#define MOD_ALT_SHIFT_P 25
// pause window events
#define MOD_ALT_P 26
// minimize/restore
#define MOD_ALT_M 27
// maximize/restore
#define MOD_ALT_SHIFT_M 28

#define WINDOW_HIGHLIGHT_ON 0x00FF863A
#define WINDOW_HIGHLIGHT_OFF 0x00CCCCCC

#define WP_VISIBLE 0x000001
#define WP_SHUTDOWN 0x000010

#define INIT_VECTOR(p)                                                         \
  do {                                                                         \
    p->size = 0;                                                               \
    p->capacity = 0;                                                           \
    p->data = NULL;                                                            \
  } while (0);

typedef struct {
  HWND handle;
  char *process_name;
  RECT w_rect;
  BOOL user_sized;
  BOOL minimized;
  BOOL events_paused;
} Window;

typedef struct {
  Window **data;
  size_t size;
  size_t capacity;
} WindowsVector;

typedef struct {
  size_t num;
  HMONITOR monitor;
  WindowsVector *windows;
} Workspace;

typedef struct {
  Workspace **data;
  size_t size;
  size_t capacity;
} WorkspacesVector;

typedef struct {
  char **data;
  size_t size;
  size_t capacity;
} StrVector;

typedef struct {
  size_t active_workspace;
  WorkspacesVector *workspaces;
  BOOL active_tracking;
  BOOL active_zorder;
  StrVector *ignored_windows;
  ITaskbarList *taskbar;
  BOOL pause_wm;
} WmParams;

void workspaces_vec_add(WorkspacesVector *vec, Workspace *el);
void windows_vec_add(WindowsVector *vec, Window *el);
void windows_vec_remove(WindowsVector *vec, size_t idx);
void str_vec_add(StrVector *arr, char *el);
void cleanup_window(Window *window);
void cleanup(WmParams *wm_params);

#endif