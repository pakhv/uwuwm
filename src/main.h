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

#define HEDDEN_WINDOW_X -3900

typedef struct {
  size_t num;
  HMONITOR monitor;
} Workspace;

typedef struct {
  HWND handle;
  char *process_name;
  Workspace *workspace;
} Window;

DEFINE_VECTOR(Window)
DEFINE_VECTOR(Workspace)

typedef struct {
  size_t active_workspace;
  Vector_Window *windows;
  Vector_Workspace *workspaces;
} Wm_params;

#define MAX_EXE_PATH_LENGTH 300
#define MAX_EXE_NAME_LENGTH 100