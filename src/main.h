/**
 * The header of "main" module of qsh.
 */
#pragma once

#include <limits.h>
#include <unistd.h>
#include <stdio.h>

#define UNUSED(x) (void) (x)

#define MAXLINE 1024
#define PID_MAX 32768
#define MAXARGS 128

// set new file mode
#define RWRWR (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)

// to the type of redirecting
enum REDIRECT { NO = 0, OUT = 1, ERR = 2, IN = 4, CLOSE = 8, APPEND = 16, };

enum STATE { UNDEF, FG, BG, STOP, DONE, KILLED, CONTINUED, };

typedef struct _redirect_t {
    char filename[NAME_MAX];
    unsigned type;
} redirect_t;

typedef struct _job_t {
    char name[MAXLINE];
    pid_t pid;
    enum STATE state;
    unsigned jid;
    unsigned num;
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
    if (job->num != 0) {
        if (--job->num == 0) {
            job->name[0] = '\0';
            job->state = UNDEF;
            job->jid = 0;
            job->pid = 0;
        }
    }
}

/**
 * state2str - Convert STATE to string.
 * state : State to be converted.
 */
static inline const char *state2str(const enum STATE state)
{
    switch (state) {
    case FG:
        return "Foreground";
        break;
    case BG:
        return "Running";
        break;
    case STOP:
        return "Stopped";
        break;
    case DONE:
        return "Done";
        break;
    case KILLED:
        return "Killed";
        break;
    case CONTINUED:
        return "Continued";
        break;
    default:
        return NULL;
        break;
    }
}

/**
 * print_job : Print information of a job.
 */
static inline void print_job(const job_t *job, const enum STATE state)
{
    printf("[%u] (%d) %s %s", job->jid, job->pid, state2str(state), job->name);
}

