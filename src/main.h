#include <Windows.h>
#include <stdio.h>

typedef struct {
  HWND *handle;
  size_t length;
  size_t capacity;
} wnd_array;

typedef struct {
  wnd_array windows;
} wm_params;