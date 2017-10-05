#ifndef LOG_H
#define LOG_H

void logcmd(char *fmt, ...);

int init_logfile(char *filename);
int logfile(char *file, int line, char *msg);

#endif // LOG_H
