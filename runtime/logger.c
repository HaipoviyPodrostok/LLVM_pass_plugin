#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* log_file = NULL;

static void log_close() {
  if (log_file) {
    fclose(log_file);
  }
}

void log_init() {
  const char* path = getenv("RUNTIME_VALUES_FILE_PATH");
  if (!path)
    path = "assets/runtime_values/runtime_values.txt";

  log_file = fopen(path, "w");
  if (!log_file) {
    fprintf(stderr, "[Logger] ERROR: Failed to open %s\n", path);
    return;
  }

  atexit(log_close);
}

void log_value(uint64_t id, uint64_t val) {
  assert(log_file);

  fprintf(log_file, "%lu %lu\n", id, val);
}
