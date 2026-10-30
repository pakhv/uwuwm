#include <Windows.h>
#include <stdio.h>

#ifndef STRUCTS_H
#define STRUCTS_H

#define HIDDEN_WINDOW_X -3900
#define MAX_EXE_PATH_LENGTH 300
#define MAX_EXE_NAME_LENGTH 100

#define MOD_ALT_1 1
#define MOD_ALT_2 2
#define MOD_ALT_3 3
#define MOD_ALT_4 4
#define MOD_ALT_5 5
#define MOD_ALT_6 6
#define MOD_ALT_7 7
#define MOD_ALT_8 8
#define MOD_ALT_9 9

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
  StrVector *ignored_windows;
} WmParams;

void workspaces_vec_add(WorkspacesVector *vec, Workspace *el);
void windows_vec_add(WindowsVector *vec, Window *el);
void windows_vec_remove(WindowsVector *vec, size_t idx);
void str_vec_add(StrVector *arr, char *el);
void cleanup_window(Window *window);
void cleanup(WmParams *wm_params);

#endif