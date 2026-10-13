#include <Windows.h>
#include <stdio.h>

#define DEFINE_VECTOR(T)                                                       \
  typedef struct {                                                             \
    T **data;                                                                  \
    size_t size;                                                               \
    size_t capacity;                                                           \
  } Vector_##T;

#define INIT_VECTOR(p)                                                         \
  do {                                                                         \
    p->size = 0;                                                               \
    p->capacity = 0;                                                           \
    p->data = NULL;                                                            \
  } while (0);

#define FREE_VECTOR(p)                                                         \
  do {                                                                         \
    for (size_t i = 0; i < p->size; i++) {                                     \
      free(p->data[i]);                                                        \
    }                                                                          \
                                                                               \
    free(p->data);                                                             \
    p->size = 0;                                                               \
    p->capacity = 0;                                                           \
  } while (0);

#define HIDDEN_WINDOW_X -3900

#define MAX_EXE_PATH_LENGTH 300
#define MAX_EXE_NAME_LENGTH 100

typedef struct {
  HWND handle;
  char *process_name;
  RECT w_rect;
} Window;

DEFINE_VECTOR(Window)

typedef struct {
  size_t num;
  HMONITOR monitor;
  Vector_Window *windows;
} Workspace;

DEFINE_VECTOR(Workspace)

typedef struct {
  size_t active_workspace;
  Vector_Workspace *workspaces;
} Wm_params;