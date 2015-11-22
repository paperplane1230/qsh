#include <stdio.h>
#define PID_MAX 32768
enum STATE { UNDEF, FG, BG, STOP, DONE, KILLED, CONTINUED, };
    char name[MAXLINE];
    unsigned num;
    case FG:
        return "Foreground";
        break;
        break;
    case KILLED:
        return "Killed";
        break;
    case CONTINUED:
        return "Continued";
        break;

/**
 * print_job : Print information of a job.
 */
static inline void print_job(const job_t *job, const enum STATE state)
{
    printf("[%u] (%d) %s %s", job->jid, job->pid, state2str(state), job->name);
}
