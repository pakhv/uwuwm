#include <Windows.h>
#include <stdio.h>

#ifndef STRUCTS_H
#define STRUCTS_H

#define HIDDEN_WINDOW_X -3900
#define MAX_EXE_PATH_LENGTH 300
#define MAX_EXE_NAME_LENGTH 100

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
  size_t active_workspace;
  WorkspacesVector *workspaces;
  BOOL active_tracking;
} WmParams;

void workspaces_vec_add(WorkspacesVector *vec, Workspace *el);
void windows_vec_add(WindowsVector *vec, Window *el);
void windows_vec_remove(WindowsVector *vec, size_t idx);
void cleanup_window(Window *window);
void cleanup(WmParams *wm_params);

#endif