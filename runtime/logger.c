#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* log_file = NULL;

static void __log_close() {
  if (log_file) {
    fclose(log_file);
  }
}

void __log_init() {
  const char* path = getenv("RUNTIME_VALUES_FILE_PATH");
  if (!path)
    path = "assets/runtime_values/runtime_values.txt";

  log_file = fopen(path, "w");
  if (!log_file) {
    fprintf(stderr, "[Logger] ERROR: Failed to open %s\n", path);
    return;
  }

  atexit(__log_close);
}

void __log_value(uint64_t id, uint64_t val) {
  if (!log_file) {
    return;
  }
  fprintf(log_file, "%lu %lu\n", id, val);
}
