/**
 * The "main" module of qsh.
 */
#include "error.h"
#include "main.h"
#include <dirent.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#ifndef DEBUG
static char prompt[MAXLINE];
static char original_cmd[MAXLINE];
static job_t jobs[MAXARGS];
static pid_t grps[PID_MAX];
#endif

static char cmd[MAXLINE];

#ifndef DEBUG
/**
 * signal - Wrapper for the sigaction function. Reliable version of signal(),
 *          using POSIX sigaction().
 */
static handler_t *mysignal(int signum, handler_t *handler)
{
    struct sigaction action;
    struct sigaction old_action;

    action.sa_handler = handler;
    // block sigs of type being handled
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (signum == SIGALRM) {
#ifdef SA_INTERRUPT
        action.sa_flags |= SA_INTERRUPT;
#endif
    } else {
        // restart syscalls if possible
        action.sa_flags |= SA_RESTART;
    }
    if (sigaction(signum, &action, &old_action) < 0) {
        unix_fatal("sigaction error");
    }
    return old_action.sa_handler;
}
#endif

#ifndef DEBUG
/**
 * get_prompt - Get prompt with current directory.
 */
static void set_prompt(void)
{
    // path_place points to the place for copying current directory name
    static char *path_place = prompt;
    static bool first = true;
#define timelen 64
    static char timebuf[timelen] = ":";

    if (first) {
        path_place += strlen(prompt);
        first = false;
    }
    *path_place = '\0';
    // NOTE: this pointer should be **freed**!
    char *dirname = getcwd(NULL, PATH_MAX);

    if (dirname == NULL) {
        dirname = "";
    }
    strcat(path_place, dirname);
    time_t t;

    if (time(&t) < 0) {
        unix_fatal("time error");
    }
    strftime(timebuf+1, timelen, "%T", localtime(&t));
    strcat(path_place, timebuf);
    strcat(path_place, "> ");
    free(dirname);
    dirname = NULL;
}

#ifndef DEBUG
/**
 * getjob - Get job from the job list.
 */
static job_t *getjob(job_t jobs[], unsigned jid)
{
    if (jid < 1) {
        return NULL;
    }
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].jid == jid) {
            return &jobs[i];
        }
    }
    return NULL;
}
#endif

#ifndef DEBUG
/**
 * fgpid - Find the pid of foreground process.
 */
static pid_t fgpid(const job_t jobs[])
{
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].state == FG) {
            return jobs[i].pid;
        }
    }
    return 0;
}

/**
 * clearjob - Clear entries in a job structure.
 */
static void clearjob(job_t *job)
{
    if (job->num != 0) {
        if (--job->num == 0) {
            if (job->state != UNDEF && job->state != FG && job->state != KILLED) {
                print_job(job, DONE);
            }
            job->name[0] = '\0';
            job->state = UNDEF;
            job->jid = 0;
            job->pid = 0;
        }
    }
}

/**
 * pid2jid - Return jid corresponding to pid.
 */
static unsigned pid2jid(int pid)
{
    if (pid < 1) {
        return 0;
    }
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    }
    return 0;
}
#endif

/**
 * delete_job : Delete a job from the list.
 */
static void delete_job(job_t jobs[], pid_t pid)
{
    if (pid < 1) {
        return;
    }
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            return;
        }
    }
}

/**
 * set_terminal - Modify pgid of a control terminal.
 */
static void set_terminal(pid_t pgid)
{
    if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
        unix_fatal("tcsetpgrp error");
    }
    if (tcsetpgrp(STDOUT_FILENO, pgid) < 0) {
        unix_fatal("tcsetpgrp error");
    }
    if (tcsetpgrp(STDERR_FILENO, pgid) < 0) {
        unix_fatal("tcsetpgrp error");
    }
}

/**
 * sigchld_handler - Handler of SIGCHLD.
 */
static void sigchld_handler(int sig)
{
    int status = 0;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WCONTINUED | WNOHANG | WUNTRACED)) > 0) {
        job_t *job = getjob(jobs, pid2jid(grps[pid]));

        if (WIFSTOPPED(status)) {
            if (job->state == FG) {
                fputs("\n", stdout);
                print_job(job, STOP);
            }
            job->state = STOP;
        } else if (WIFSIGNALED(status)) {
            sig = WTERMSIG(status);
            if (sig == SIGKILL) {
                job->state = KILLED;
                print_job(job, KILLED);
            } else if (sig == SIGINT) {
                fputs("\n", stdout);
            }
            delete_job(jobs, grps[pid]);
        } else if (WIFCONTINUED(status)) {
            if (job->state != BG) {
                print_job(job, CONTINUED);
            }
        } else {
            delete_job(jobs, grps[pid]);
        }
    }
}

/**
 * sigint_handler - Handler of SIGINT.
 */
static void sigint_handler(int sig)
{
    UNUSED(sig);
    fputs("\n", stdout);
    set_prompt();
    fputs(prompt, stdout);
    fflush(stdout);
}
#endif

/**
 * do_dup - Dup oldfd to newfd.
 */
static void do_dup(int oldfd, int newfd)
{
    if (oldfd != newfd) {
        if (dup2(oldfd, newfd) != newfd) {
            unix_fatal("dup2 error");
        }
        if (close(oldfd) < 0) {
            unix_fatal("close error");
        }
    }
}

/**
 * redirect - Do redirect according to content in redirects.
 */
static void redirect(const redirect_t *redirects)
{
    while (redirects->type != NO) {
#ifdef DEBUG
        printf("%d ", redirects->type);
        printf("%s\n", redirects->filename);
        ++redirects;
        continue;
#endif
        // get IN, OUT, or ERR
        int toredirect = get_direction(redirects->type);
        mode_t mode = toredirect == IN ? O_RDONLY : O_WRONLY | O_CREAT | O_APPEND;

        switch (redirect_type(redirects->type)) {
        case CLOSE:
            if (close(type2fd(toredirect)) < 0) {
                unix_fatal("close error");
            }
            break;
        case NO:
            if (toredirect != IN) {
                mode |= O_TRUNC;
            }
            // case NO and APPEND share the next code
        case APPEND: {
            int fd = 0;

            // it seems to have different semantics with normal shell
            if (strcmp(redirects->filename, "&1") == 0) {
                fd = STDOUT_FILENO;
            } else if (strcmp(redirects->filename, "&2") == 0) {
                fd = STDERR_FILENO;
            } else {
                fd = open(redirects->filename, mode, RWRWR);
            }
            if (fd < 0) {
                unix_fatal(redirects->filename);
            }
            int newfd = type2fd(toredirect);

            do_dup(fd, newfd);
            break;
        } default:
            break;
        }
        ++redirects;
    }
}

/**
 * copybuf - Copy a string to an array.
 */
static void copybuf(char *dest, const char *buf, size_t num)
{
    size_t len = strlen(buf);
    size_t n = len > num ? num : len;

    strncpy(dest, buf, n);
    dest[n] = '\0';
}

/**
 * move_delim - Move delimiter and pointer of buffer when parsing command.
 */
static void move_delim(char **buf, char **delim)
{
    while (**buf == ' ' || **buf == '\t') {
        ++*buf;
    }
    switch (**buf) {
    case '\'':
    case '\"':
        ++*buf;
        *delim = strchr(*buf, (*buf)[-1]);
        break;
    default:
        *delim = strchr(*buf, ' ');
        break;
    }
}

/**
 * parseline - Parse the cmdline to return the parameters.
 * 
 * NOTE:
 * Assume the length of a line is less than MAXLINE and there's no string
 * consisting of more than one line. And it cannot deal with situations
 * where parameters are in quotes fully or partly while '-' before them is not,
 * such as `ls -"a"l`. And for redirecting, there should be no space in the item.
 */
static void parseline(char *buf, char **argv, redirect_t *redirects)
{
    buf[strlen(buf)-1] = ' ';
    // the 2d array to store filenames in current directory when a '*' is the parameter
    static char files[MAXARGS][NAME_MAX];
    static char path[PATH_MAX] = {'\0'};
    int argc = 0;
    char *delim = buf;

    move_delim(&buf, &delim);
    size_t redirect_num = 0;

    while (delim != NULL) {
        *delim = '\0';
        enum REDIRECT type;

        switch (*buf) {
        case '~': {
            char *home = getenv("HOME");

            if (home == NULL) {
                unix_error("cannot find home directory");
            }
            copybuf(path, home, PATH_MAX);
            size_t len = strlen(buf+1) + 1;
            size_t n = len > PATH_MAX / 2 ? PATH_MAX / 2 : len;

            strncat(path, buf+1, n);
            argv[argc++] = path;
            break;
        } case '>':
            if (buf[1] == '>') {
                redirects[redirect_num].type = APPEND | OUT;
                copybuf(redirects[redirect_num++].filename, &buf[2], NAME_MAX);
                break;
            } else {
                // NOTE: this branch shares code with case '<'
            }
        case '<':
            type = *buf == '>' ? OUT : IN;
            redirects[redirect_num].type = type;
            copybuf(redirects[redirect_num++].filename, &buf[1], NAME_MAX);
            break;
        case '1':
        case '2':
            type = *buf == '1' ? OUT : ERR;
            if (buf[1] == '>') {
                if (buf[2] == '&' && buf[3] == '-') {
                    redirects[redirect_num].type = CLOSE | type;
                    *redirects[redirect_num++].filename = '\0';
                } else if (buf[2] == '>') {
                    redirects[redirect_num].type = APPEND | type;
                    copybuf(redirects[redirect_num++].filename, &buf[3], NAME_MAX);
                } else {
                    redirects[redirect_num].type = type;
                    copybuf(redirects[redirect_num++].filename, &buf[2], NAME_MAX);
                }
                break;
            }
            goto do_nothing;
        case '*': {
            if (buf[1] == '\0') {
                DIR *dp = opendir(".");

                if (dp == NULL) {
                    unix_fatal("can't get all files and directories");
                }
                struct dirent *dirp = NULL;
                size_t file_number = 0;

                errno = 0;
                while ((dirp = readdir(dp)) != NULL) {
                    if (dirp->d_name[0] != '.') {
                        copybuf(files[file_number], dirp->d_name, NAME_MAX);
                        argv[argc++] = files[file_number++];
                    }
                }
                if (errno != 0) {
                    unix_fatal("readdir error");
                }
                if (closedir(dp) < 0) {
                    unix_fatal("closedir error");
                }
                break;
            }
        } default:
do_nothing:
            argv[argc++] = buf;
            break;
        }
        buf = delim + 1;
        move_delim(&buf, &delim);
    }
    argv[argc] = NULL;
    redirects[redirect_num].type = NO;
}

#ifndef DEBUG
/**
 * addjob - Add a job to the job list.
 */
static void addjob(job_t *jobs, pid_t pid, enum STATE state, const char *cmd, unsigned num)
{
    if (pid < 1) {
        return;
    }
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].pid == 0) {
            jobs[i].num = num;
            jobs[i].pid = pid;
            jobs[i].jid = i + 1;
            jobs[i].state = state;
            copybuf(jobs[i].name, cmd, MAXLINE);
            return;
        }
    }
    printf("Too many jobs now!");
}
#endif

#ifndef DEBUG
/**
 * waitfg - Wait process in foreground to stop.
 */
static void waitfg(const job_t jobs[])
{
    while (fgpid(jobs) != 0) {
        pause();
    }
}

/**
 * listjobs - List present jobs.
 */
static void listjobs(const job_t jobs[])
{
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].pid != 0) {
            print_job(&jobs[i], jobs[i].state);
        }
    }
}

/**
 * do_bgfg - Execute bg of fg command.
 */
static void do_bgfg(char *argv[], job_t jobs[])
{
    int jid = 1;

    if (argv[1] != NULL) {
        if (argv[1][0] != '%') {
            fputs("There must be '%%' before job id.\n", stdout);
            return;
        }
        jid = atoi(argv[1] + 1);
    }
    job_t *job = getjob(jobs, jid);

    if (job == NULL) {
        printf("%%%d: No such job.\n", jid);
        return;
    }
    pid_t pid = job->pid;

    switch (strcmp(argv[0], "bg")) {
    case 0:
        if (job->state == BG) {
            app_error("Job already in background.");
            return;
        }
        job->state = BG;
        if (kill(-pid, SIGCONT) < 0) {
            unix_fatal("kill error");
        }
        print_job(job, CONTINUED);
        break;
    default:
        job->state = FG;
        set_terminal(job->pid);
        if (kill(-pid, SIGCONT) < 0) {
            unix_fatal("kill error");
        }
        waitfg(jobs);
        set_terminal(getpid());
        break;
    }
}
#endif

/**
 * builtin_cmd - Judge whether the command is a builtin command.
 */
static bool builtin_cmd(char **argv)
{
    if (strcmp(*argv, "exit") == 0) {
        exit(0);
    } else if (strcmp(*argv, "cd") == 0) {
        if (argv[1] == NULL) {
            char *home = getenv("HOME");

            argv[1] = home == NULL ? "" : home;
        }
        if (chdir(argv[1]) != 0) {
            switch (errno) {
            case EACCES :
                app_error("cd: Permission denied.");
                break;
            case ENOENT:
                app_error("cd: No such directory.");
                break;
            default:
                unix_error("chdir error");
                break;
            }
        }
        return true;
    } else if (strcmp(*argv, "jobs") == 0) {
#ifndef DEBUG
        listjobs(jobs);
#endif
        return true;
    } else if (strcmp(*argv, "fg") == 0 || strcmp(*argv, "bg") == 0) {
#ifndef DEBUG
        do_bgfg(argv, jobs);
#endif
        return true;
    }
    return false;
}

/**
 * preprocess - Judge whether it's run on background.
 */
static bool preprocess(char *cmdline)
{
    char *tmp = cmdline + strlen(cmdline);

    while (isspace(*--tmp) && tmp >= cmdline) {
        ;
    }
    if (tmp < cmdline) {
        return false;
    }
    if (*tmp == '&') {
        *tmp = ' ';
        return true;;
    }
    return false;
}

/**
 * split - Split the cmdline according to delim.
 */
static int split(char *buf, char delim, char *argv[])
{
    size_t argc = 0;
    char *bond = NULL;

    while ((bond = strchr(buf, delim)) != NULL) {
        *bond = '\0';
        if (!isspace(bond[-1])) {
            app_error("There must be space before a delimiter.");
            return -1;
        }
        argv[argc++] = buf;
        buf = bond + 1;
    }
    argv[argc++] = buf;
    argv[argc] = NULL;
    return argc;
}

#ifndef DEBUG
/**
 * closepipes - Close useless pipes in a specific process.
 */
static void closepipes(int pipes[][2], int pipes_num, int number)
{
    for (int i = 0; i < pipes_num; ++i) {
        if (i != number) {
            close(pipes[i][1]);
        }
        if (i != number - 1) {
            close(pipes[i][0]);
        }
    }
}

/**
 * block_child - Block signal.
 */
static void block_sig(sigset_t *mask)
{
    if (sigemptyset(mask) < 0) {
        unix_fatal("sigemptyset error");
    }
    if (sigaddset(mask, SIGCHLD) < 0) {
        unix_fatal("sigadset error");
    }
    if (sigprocmask(SIG_BLOCK, mask, NULL) < 0) {
        unix_fatal("sigprocmask error");
    }
}

/**
 * unblock_sig : Unblock signals in mask.
 */
static void unblock_sig(sigset_t *mask)
{
    if (sigprocmask(SIG_UNBLOCK, mask, NULL) < 0) {
        unix_fatal("sigprocmask error");
    }
}

/**
 * connect_pipes - Connect stdin and stdout of this process to related pipes.
 */
static void connect_pipes(int number, int pipes[][2], int pipes_num)
{
    if (number < pipes_num) {
        do_dup(pipes[number][1], STDOUT_FILENO);
    }
    if (number > 0) {
        do_dup(pipes[number-1][0], STDIN_FILENO);
    }
}

/**
 * set_group - Call setpgid in the parent process.
 */
static void set_group(const pid_t pids[], int sum)
{
    for (int i = 0; i < sum; ++i) {
        grps[pids[i]] = pids[0];
        if (setpgid(pids[i], pids[0]) < 0 && errno != EACCES) {
            unix_fatal("setpgid error");
        }
    }
}

/**
 * add_newjob - Add a new job in foreground or background.
 */
static void add_newjob(int pid, bool bg, unsigned num, sigset_t *mask)
{
    if (!bg) {
        addjob(jobs, pid, FG, original_cmd, num);
        set_terminal(pid);
        unblock_sig(mask);
        waitfg(jobs);
        set_terminal(getpid());
    } else {
        addjob(jobs, pid, BG, original_cmd, num);
        unblock_sig(mask);
        printf("[%u] %d %s", pid2jid(pid), pid, original_cmd);
    }
}

/**
 * change_ttyio - Change SIGTTIN and SIGTTOU's handling way.
 */
static void change_ttyio(handler_t *handle_way)
{
    mysignal(SIGTTIN, handle_way);
    mysignal(SIGTTOU, handle_way);
}

/**
 * setpgid_pipe - Set pgid for a process in a pipe.
 */
static void setpgid_pipe(pid_t pids[], int number)
{
    if (number == 0) {
        pids[0] = getpid();
    }
    grps[getpid()] = pids[0];
    if (setpgid(getpid(), pids[0]) < 0) {
        unix_fatal("setpgid error");
    }
}

/**
 * eval - Evaluate the cmdline. Parameter firsttime set to judge whether it's
 *          the top parent process.
 *
 * NOTE: There should be no embedded command in a pipe.
 */
static void eval(char *cmdline)
{
    char *cmds[MAXARGS] = {NULL};
    bool bg = preprocess(cmdline);

    copybuf(original_cmd, cmdline, MAXLINE);
    int pipes_num = split(cmdline, '|', cmds) - 1;

    if (pipes_num < 0) {
        return;
    }
    // to judge whether to redirect later
    redirect_t redirects[MAXARGS];
    int pipes[pipes_num][2];
    // to tell which command to execute
    int number = pipes_num + 1;
    pid_t pids[pipes_num+2];

    pids[pipes_num+1] = getpid();
    char *argv[MAXARGS] = {NULL};

    if (pipes_num == 0) {
        parseline(cmds[0], argv, redirects);
        if (argv[0] == NULL || builtin_cmd(argv)) {
            return;
        }
    }
    sigset_t mask;

    block_sig(&mask);
    for (int i = 0; i < pipes_num + 1; ++i) {
        if (i < pipes_num) {
            if (pipe(pipes[i]) < 0) {
                unix_fatal("pipe error");
            }
        }
        if ((pids[i] = fork()) <= 0) {
            number = i;
            break;
        }
    }
    closepipes(pipes, pipes_num, number);
    if (pids[number] < 0) {
        unix_fatal("fork error");
    } else if (pids[number] == 0) {
        setpgid_pipe(pids, number);
        if (number == 0 && !bg) {
            set_terminal(getpid());
        }
        change_ttyio(SIG_DFL);
        if (pipes_num > 0) {
            parseline(cmds[number], argv, redirects);
            if (argv[0] == NULL) {
                app_fatal("no content for pipe");
            }
        }
        unblock_sig(&mask);
        redirect(redirects);
        connect_pipes(number, pipes, pipes_num);
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            exit(3);
        }
    }
    set_group(pids, pipes_num+1);
    add_newjob(pids[0], bg, pipes_num+1, &mask);
}

/**
 * initjobs - Initialize jobs for shell.
 */
static void initjobs(job_t jobs[])
{
    for (size_t i = 0; i < MAXARGS; ++i) {
        jobs[i].num = 1;
        clearjob(&jobs[i]);
    }
}

/**
 * main - The shell's main loop.
 */
int main(void)
{
    const char *name = getenv("LOGNAME");
    char *args[MAXARGS] = {NULL};

    if (name == NULL) {
        name = "";
    }
    strcat(prompt, name);
    strcat(prompt, ":");
    mysignal(SIGINT, sigint_handler);
    mysignal(SIGTSTP, sigint_handler);
    mysignal(SIGCHLD, sigchld_handler);
    change_ttyio(SIG_IGN);

    initjobs(jobs);
    while (true) {
        set_prompt();
        fputs(prompt, stdout);
        if (fgets(cmd, MAXLINE, stdin) == NULL && ferror(stdin)) {
            app_fatal("fgets error");
        }
        if (feof(stdin)) {
            fputs("\n", stdout);
            return 0;
        }
        if (split(cmd, ';', args) > 0) {
            // NOTE: There must be space before ';'
            for (size_t i = 0; args[i] != NULL; ++i) {
                eval(args[i]);
            }
        }
    }
    return 0;
}
#endif
