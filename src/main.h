 * The header of "main" module of qsh.
#include <unistd.h>
enum STATE { UNDEF, FG, BG, STOP, DONE, };

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

 * type2fd - Map type of redirect to file descriptor.
static inline int type2fd(const enum REDIRECT redirect)

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