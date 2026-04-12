#include <stdio.h>
#include <stdint.h>

void __log_value(uint64_t id, uint64_t val) {
    FILE *file = fopen("assets/runtime_values/runtime_values.txt", "a"); // FIXME[flops]: Don't use hardcoded path. Leave a possibility to change it. You can use `getenv` or common function to retrieve path to runtime values file
    if (file == NULL) { // FIXME[flops]: Something very bad happened in this case, you should drop a loud warning at least
        return;
    }

    fprintf(file, "%lu %lu\n", id, val);
    fclose(file);
}
