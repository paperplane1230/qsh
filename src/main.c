/**
 * The "main" module of qsh.
 */
#include "error.h"
#include "main.h"
#include <dirent.h>
#include <stdio.h>
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

// judge whether to print prompt when dealing with SIGINT
static bool inchild = false;
#endif

static char cmd[MAXLINE] = {'\0'};
job_t jobs[MAXARGS];

#ifndef DEBUG
/**
 * signal - Wrapper for the sigaction function. Reliable version of signal(),
 *          using POSIX sigaction().
 * signum : Signal to be handled.
 * handler : Function to be called.
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

/**
 * sigint_handler - Signal handler for SIGINT.
 * sig : Signal number.
 */
static void sigint_handler(int sig)
{
    // the sig parameter is not used
    UNUSED(sig);
    fputs("\n", stdout);
    if (!inchild) {
        set_prompt();
        fputs(prompt, stdout);
        fflush(stdout);
    }
}

/**
 * sigtstp_handler - Signal handler for SIGTSTP.
 * sig : Signal number.
 */
static void sigtstp_handler(int sig)
{
    // the sig parameter is not used
    UNUSED(sig);
}
#endif

/**
 * do_dup - Dup oldfd to newfd.
 * oldfd : Old file descriptor.
 * newfd : New file descriptor.
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
 * redirects : To store result of redirect.
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
 * dest : Array to store string.
 * buf : String to be copied.
 * num : Number of bytes to be copied.
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
 * buf : Pointer of buffer.
 * delimiter : Delimiter to be moved.
 */
static void move_delim(char **buf, char **delim)
{
    while (**buf == ' ') {
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
 * buf : Line to be parsed.
 * argv : Store parameters.
 * redirects : Store result of redirects.
 * 
 * NOTE:
 * Assume the length of a line is less than MAXLINE and there's no string
 * consisting of more than one line. And it cannot deal with situations
 * where parameters are in quotes fully or partly while '-' before them is not,
 * such as `ls -"a"l`. And for redirecting, there should be no space in the item.
 */
static bool parseline(char *buf, char **argv, redirect_t *redirects)
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
    if (argc == 0) {
        return false;
    }
    bool bg = false;

    if ((bg = (*argv[argc-1] == '&'))) {
        argv[--argc] = NULL;
    }
    return bg;
}

#ifndef DEBUG
/**
 * addjob - Add a job to the job list.
 * job : Job to be added.
 * pid : Pid of the job.
 * state : Background or foreground.
 * cmd : Name of command.
 */
static void addjob(job_t *jobs, pid_t pid, enum STATE state, const char *cmd)
{
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].jid = i + 1;
            jobs[i].state = state;
            copybuf(jobs[i].cmd.name, cmd, MAXLINE);
            return;
        }
    }
    printf("Too many jobs now!");
}
#endif

/**
 * listjobs - List present jobs.
 * jobs - Jobs to be printed.
 */
static void listjobs(const job_t jobs[])
{
    for (size_t i = 0; i < MAXARGS; ++i) {
        if (jobs[i].pid != 0) {
            printf("[%d]%-30s%-s", jobs[i].jid, state2str(jobs[i].state),
                    jobs[i].cmd.name);
            const job_cmd *c = &(jobs[i].cmd);

            while ((c = c->next) != NULL) {
                printf("\t\t\t\t\t\t\t\t%-30s", c->name);
            }
        }
    }
}

/**
 * builtin_cmd - Judge whether the command is a builtin command.
 * argv : Command to be judged.
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
        listjobs(jobs);
        return true;
    }
    return false;
}

/**
 * split - Split the cmdline according to delim.
 * buf : The source string to be split.
 * delim : The character as the delimiter.
 * argv : Store results of spliting.
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
 * pipes : Pipes to be closed.
 * pipes_num : Number of pipes.
 * number : The number of specific process.
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
 * eval - Evaluate the cmdline. Parameter firsttime set to judge whether it's
 *          the top parent process.
 * cmdline : The command to be evaluated.
 *
 * NOTE: There should be no embedded command in a pipe.
 */
static void eval(char *cmdline)
{
    char *cmds[MAXARGS] = {NULL};
    int pipes_num = split(cmdline, '|', cmds) - 1;

    if (pipes_num < 0) {
        return;
    }
    // to judge whether to redirect later
    redirect_t redirects[MAXARGS];
    int pipes[pipes_num][2];
    // to tell which command to execute
    int number = pipes_num + 2;
    pid_t pid;
    char *argv[MAXARGS] = {NULL};
    bool bg = false;

    if (pipes_num == 0) {
        bg = parseline(cmds[0], argv, redirects);
        if (argv[0] == NULL || builtin_cmd(argv)) {
            return;
        }
    }
    for (int i = 0; i < pipes_num + 1; ++i) {
        if (i < pipes_num) {
            if (pipe(pipes[i]) < 0) {
                unix_fatal("pipe error");
            }
        }
        if ((pid = fork()) <= 0) {
            number = i;
            break;
        }
    }
    closepipes(pipes, pipes_num, number);
    if (pid < 0) {
        unix_fatal("fork error");
    } else if (pid == 0) {
        if (pipes_num > 0) {
            bg = parseline(cmds[number], argv, redirects);
            if (argv[0] == NULL) {
                app_fatal("no content for pipe");
            }
        }
        redirect(redirects);
        if (number < pipes_num) {
            do_dup(pipes[number][1], STDOUT_FILENO);
        }
        if (number > 0) {
            do_dup(pipes[number-1][0], STDIN_FILENO);
        }
        if (execvp(argv[0], argv) < 0) {
            printf("%s: Command not found.\n", argv[0]);
            exit(3);
        }
    }
    inchild = true;
    while (wait(NULL) > 0) {
        ;
    }
}

/**
 * initjobs - Initialize jobs for shell.
 * jobs : Jobs to be initialized.
 */
static void initjobs(job_t jobs[])
{
    for (size_t i = 0; i < MAXARGS; ++i) {
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
    mysignal(SIGTSTP, sigtstp_handler);

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
        inchild = false;
    }
    return 0;
}
#endif
