#include <Windows.h>
#include <stdio.h>

#define DEFINE_VECTOR(T)                                                       \
  typedef struct {                                                             \
    T *data;                                                                   \
    size_t size;                                                               \
    size_t capacity;                                                           \
  } Vector_##T;

DEFINE_VECTOR(HWND)

// DEFINE_VECTOR(workspace)

// typedef struct {
//   size_t num;
//   Vector_HWND *windows;
// } workspace;

typedef struct {
  HWND *handle;
  // workspace workspace;
} Wnd_info;

typedef struct {
  // workspace *workspaces;
  Vector_HWND *windows;
} Wm_params;