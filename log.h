
#ifndef LOG_H
#define LOG_H

void init_log();
void log_event(const char* format, ...);
void save_log(const char* filename);

#endif // LOG_H 