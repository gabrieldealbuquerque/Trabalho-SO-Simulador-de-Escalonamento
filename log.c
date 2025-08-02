
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#define LOG_BUFFER_SIZE 4096

char log_buffer[LOG_BUFFER_SIZE];
int log_index = 0;

void init_log() {
    log_index = 0;
    log_buffer[0] = '\0';
}

void log_event(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char temp[256];
    vsnprintf(temp, 256, format, args);
    va_end(args);

    int temp_len = strlen(temp);
    if (log_index + temp_len < LOG_BUFFER_SIZE - 1) {
        strcpy(log_buffer + log_index, temp);
        log_index += temp_len;
    }
}

void save_log(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%s", log_buffer);
        fclose(file);
    }
} 