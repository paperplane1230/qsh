/**
 * The header of "main" module of qsh.
 */
#pragma once

#include <limits.h>
#include <unistd.h>

#define UNUSED(x) (void) (x)

#define MAXLINE 1024
#define MAXARGS 128

// set new file mode
#define RWRWR (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)

// to the type of redirecting
enum REDIRECT { NO = 0, OUT = 1, ERR = 2, IN = 4, CLOSE = 8, APPEND = 16, };

enum STATE { UNDEF, FG, BG, STOP, DONE, };

typedef struct _redirect_t {
    char filename[NAME_MAX];
    unsigned type;
} redirect_t;

typedef struct _job_cmd {
    char name[MAXLINE];
    struct _job_cmd *next;
} job_cmd;

typedef struct _job_t {
    job_cmd cmd;
    pid_t pid;
    enum STATE state;
    unsigned jid;
} job_t;

typedef void handler_t(int);

/**
 * type2fd - Map type of redirect to file descriptor.
 * redirect : REDIRECT to be casted.
 */
static inline int type2fd(const enum REDIRECT redirect)
{
    return redirect % 4;
}

/**
 * get_direction - Get direction of redirect.
 * redirect : REDIRECT to be redirected.
 */
static inline int get_direction(const enum REDIRECT redirect)
{
    return redirect & 7;
}

/**
 * redirect_type - Get type of redirect.
 * redirect : REDIRECT to be redirected.
 */
static inline int redirect_type(const enum REDIRECT redirect)
{
    return redirect & 24;
}

/**
 * clearjob - Clear entries in a job structure.
 * job - Job to be cleared.
 */
static inline void clearjob(job_t *job)
{
    job->cmd.name[0] = '\0';
    job->cmd.next = NULL;
    job->state = UNDEF;
    job->pid = 0;
    job->jid = 0;
}

/**
 * state2str - Convert STATE to string.
 * state : State to be converted.
 */
static inline const char *state2str(const enum STATE state)
{
    switch (state) {
    case BG:
        return "Running";
        break;
    case STOP:
        return "Stopped";
        break;
    case DONE:
        return "Done";
    default:
        return NULL;
        break;
    }
}
