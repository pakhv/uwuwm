#include "structs.h"

void workspaces_vec_add(WorkspacesVector *arr, Workspace *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(Workspace *));
  }

  arr->data[arr->size] = el;
  arr->size += 1;
}

void windows_vec_add(WindowsVector *arr, Window *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(Window *));
  }

  arr->data[arr->size] = el;
  arr->size += 1;
}

void str_vec_add(StrVector *arr, char *el) {
  if (arr->capacity <= arr->size) {
    arr->capacity = 2 * (arr->size + 1);
    arr->data = realloc(arr->data, arr->capacity * sizeof(char *));
  }

  char *new_el = malloc(MAX_EXE_NAME_LENGTH);
  strcpy(new_el, el);

  arr->data[arr->size] = new_el;
  arr->size += 1;
}

void windows_vec_remove(WindowsVector *vec, size_t idx) {
  if (idx >= vec->size) {
    return;
  }

  cleanup_window(vec->data[idx]);

  for (size_t i = idx; i < vec->size - 1; i++) {
    vec->data[i] = vec->data[i + 1];
  }

  vec->size -= 1;
}

void cleanup(WmParams *wm_params) {
  WorkspacesVector *workspaces = wm_params->workspaces;
  StrVector *ignored_windows = wm_params->ignored_windows;
  WindowsVector *windows = NULL;

  for (size_t i = 0; i < workspaces->size; i++) {
    windows = workspaces->data[i]->windows;

    for (size_t j = 0; j < windows->size; j++) {
      if (windows->data[j] != NULL) {
        free(windows->data[j]->process_name);
        free(windows->data[j]);
      }
    }

    free(windows);
    free(workspaces->data[i]);
  }

  for (size_t i = 0; i < ignored_windows->size; i++) {
    free(ignored_windows->data[i]);
  }

  free(ignored_windows->data);
  ignored_windows->size = 0;
  ignored_windows->capacity = 0;
  free(ignored_windows);

  free(workspaces->data);
  workspaces->size = 0;
  workspaces->capacity = 0;
  free(workspaces);
}

void cleanup_window(Window *window) {
  free(window->process_name);
  window->handle = NULL;
  window->process_name = NULL;
  free(window);
}