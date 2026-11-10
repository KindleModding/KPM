#ifndef KPM_IO_H
#define KPM_IO_H

typedef enum
{
    KPM_VERBOSITY_DEBUG,
    KPM_VERBOSITY_INFO,
    KPM_VERBOSITY_WARN,
    KPM_VERBOSITY_ERROR
} KPMIO_Verbosity;

typedef void KPMIO_Stream(char c);
typedef void KPMIO_Log(KPMIO_Verbosity, const char *format, ...) __attribute__((format(printf, 2, 3)));
typedef void KPMIO_LogProgress(unsigned int progress, const char *format, ...) __attribute__((format(printf, 2, 3)));
typedef bool KPMIO_GetInput(const char *format, ...) __attribute__((format(printf, 1, 2)));

typedef struct
{
    KPMIO_Log *log;
    KPMIO_Stream *stream;
    KPMIO_LogProgress *logProgress;
    KPMIO_GetInput *getInput;
} KPMIO;

#endif