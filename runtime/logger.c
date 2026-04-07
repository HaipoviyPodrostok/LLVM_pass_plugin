#include <stdio.h>
#include <stdint.h>

void __log_value(uint64_t id, uint64_t val) {
    FILE *file = fopen("assets/runtime_values.txt", "a");
    if (file == NULL) {
        return;
    }

    fprintf(file, "%lu %lu\n", id, val);
    fclose(file);
}
