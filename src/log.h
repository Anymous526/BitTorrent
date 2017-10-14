#ifndef LOG_H
#define LOG_H

//用户记录程序的行为
void logcmd(char *fmt, ...);

//打开日志文件
int init_logfile(char *filename);
//将程序 运行日志记录到文件
int logfile(char *file, int line, char *msg);

#endif // LOG_H
