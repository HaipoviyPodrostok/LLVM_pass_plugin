#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* log_file = NULL;

static void __log_close() {
  if (log_file != NULL) {
    fclose(log_file);
  }
}

void __log_init() {
  const char* path = getenv("RUNTIME_VALS_PATH");
  if (!path)
    path = "assets/runtime_values/runtime_values.txt";

  // Стираем старый лог-файл параметром "w" и открываем поток
  log_file = fopen(path, "w");
  if (!log_file) {
    fprintf(stderr, "[DefUse Logger] ERROR: Failed to open %s\n", path);
    return;
  }

  // Регистрируем хук завершения ОС. Когда программа выйдет из main,
  // ядро автоматически вызовет функцию __log_shutdown.
  atexit(__log_close);
}

// 2. Инжектированный логгер (будет вызван миллионы раз)
void __log_value(uint64_t id, uint64_t val) {
  if (log_file != NULL) {
    // Данные пишутся строго в виртуальную память (буфер С).
    // Zero системных прерываний жесткого диска.
    fprintf(log_file, "%lu %lu\n", id, val);
  }
}
