#include <Windows.h>
#include <stdio.h>

#define DEFINE_VECTOR(T)                                                       \
  typedef struct {                                                             \
    T *data;                                                                   \
    size_t size;                                                               \
    size_t capacity;                                                           \
  } Vector_##T;

typedef struct {
  size_t num;
  HMONITOR monitor;
} Workspace;

typedef struct {
  HWND handle;
  char *process_name;
  Workspace workspace;
} Window;

DEFINE_VECTOR(HWND)
DEFINE_VECTOR(Window)

typedef struct {
  size_t active_workspace;
  Vector_Window *windows;
} Wm_params;

#define MAX_EXE_PATH_LENGTH 300
#define MAX_EXE_NAME_LENGTH 100