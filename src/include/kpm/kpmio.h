#ifndef KPM_IO_H
#define KPM_IO_H

typedef enum
{
    KPM_VERBOSITY_DEBUG,
    KPM_VERBOSITY_INFO,
    KPM_VERBOSITY_WARN,
    KPM_VERBOSITY_ERROR
} KPMIO_Verbosity;

typedef void KPMStream(char c);
typedef void KPMLog(KPMIO_Verbosity, const char *format, ...) __attribute__((format(printf, 2, 3)));
typedef void KPMLogProgress(unsigned int progress, const char *format, ...) __attribute__((format(printf, 2, 3)));
typedef bool KPMGetInput(const char *format, ...) __attribute__((format(printf, 1, 2)));

typedef struct
{
    KPMLog *log;
    KPMStream *stream;
    KPMLogProgress *logProgress;
    KPMGetInput *getInput;
} KPMIO;

#endif