/**
 * Author: Alan Chien
 * Email: upplane1230@gmail.com
 * Language: C
 * Date: Sun Oct 18 09:20:42 CST 2015
 * Description: The "main" module of qsh.
 */
#include "error.h"
#include "main.h"
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <errno.h>

#ifndef DEBUG
static char prompt[MAXLINE];

// judge whether to print prompt when dealing with SIGINT
static bool inchild = false;
#endif

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
        unix_error("sigaction error");
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
    strcat(path_place, "> ");
    free(dirname);
    dirname = NULL;
}

/**
 * sigint_handler - Signal handler for SIGINT.
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
 */
static void sigtstp_handler(int sig)
{
    // the sig parameter is not used
    UNUSED(sig);
}
#endif

/**
 * redirect - Do redirection according to content in redirects.
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
        int toredirect = redirects->type & 7;
        mode_t mode = toredirect == IN ? O_RDONLY : O_WRONLY | O_CREAT | O_APPEND;

        switch (redirects->type & 24) {
        case CLOSE:
            if (close(typetofd(toredirect)) < 0) {
                unix_error("close error");
            }
            break;
        case NO:
            if (toredirect != IN) {
                mode |= O_TRUNC;
            }
        case APPEND: {
            // case NO and APPEND share the next code
            int fd = open(redirects->filename, mode, RWRWR);

            if (fd < 0) {
                unix_error(redirects->filename);
            }
            // toredirect % 4 to convert IN to 0(STDIN_FILENO)
            if (dup2(fd, typetofd(toredirect)) < 0) {
                unix_error("dup2 error");
            }
            // end of shared code
            }
            break;
        default:
            break;
        }
        ++redirects;
    }
}

/**
 * copybuf - Copy content in buf to filename.
 */
static void copybuf(char *filename, const char *buf)
{
    size_t len = strlen(buf);
    size_t n = len > NAME_MAX - 1 ? NAME_MAX - 1 : len;

    strncpy(filename, buf, len);
    filename[n] = '\0';
}

/**
 * parseline - Parse the cmdline to return the parameters.
 * 
 * Note:
 * Assume the length of a line is less than MAXLINE and there's no string
 * consisting of more than one line. And it cannot deal with situations
 * where parameters are in quotes fully or partly while '-' before them is not,
 * such as `ls -"a"l`. And for redirecting, there should be no space in the item.
 */
static void parseline(const char *cmdline, char **argv, redirect_t *redirects)
{
#ifdef DEBUG
    fputs(cmdline, stdout);
#endif
    static char array[MAXLINE] = {'\0'};
    char *buf = array;

    strncpy(buf, cmdline, strlen(cmdline)+1);
    buf[strlen(cmdline)-1] = ' ';
    while (*buf == ' ') {
        ++buf;
    }
    int argc = 0;
    char *delim = buf;

    switch (*buf) {
    case '\'':
    case '\"':
        ++buf;
        delim = strchr(buf, buf[-1]);
        break;
    default:
        delim = strchr(buf, ' ');
        break;
    }
    size_t redirect_num = 0;

    while (delim != NULL) {
        *delim = '\0';
        enum REDIRECT type;

        switch (*buf) {
        case '1':
        case '2':
            type = *buf == '1' ? OUT : ERR;
            if (buf[1] == '>') {
                if (buf[2] == '&' && buf[3] == '-') {
                    redirects[redirect_num].type = CLOSE | type;
                    *redirects[redirect_num++].filename = '\0';
                } else if (buf[2] == '>') {
                    redirects[redirect_num].type = APPEND | type;
                    copybuf(redirects[redirect_num++].filename, &buf[3]);
                } else {
                    redirects[redirect_num].type = type;
                    copybuf(redirects[redirect_num++].filename, &buf[2]);
                }
            }
            break;
        case '>':
            if (buf[1] == '>') {
                redirects[redirect_num].type = APPEND | OUT;
                copybuf(redirects[redirect_num++].filename, &buf[2]);
                break;
            } else {
                // NOTE: this branch shares code with case '<'
            }
        case '<':
            type = *buf == '>' ? OUT : IN;
            redirects[redirect_num].type = type;
            copybuf(redirects[redirect_num++].filename, &buf[1]);
            break;
        default:
            argv[argc++] = buf;
            break;
        }
        buf = delim + 1;
        while (*buf == ' ') {
            ++buf;
        }
        switch (*buf) {
        case '\'':
        case '\"':
            ++buf;
            delim = strchr(buf, buf[-1]);
            break;
        default:
            delim = strchr(buf, ' ');
            break;
        }
    }
    argv[argc] = NULL;
    redirects[redirect_num].type = NO;
}

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
        } else if (argv[1][0] == '~') {
            char path[PATH_MAX] = {'\0'};
            char *home = getenv("HOME");

            if (home == NULL) {
                unix_error("cannot find home directory");
            }
            strcpy(path, home);
            strcat(path, &argv[1][1]);
            argv[1] = path;
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
    }
    return false;
}

#ifndef DEBUG
/**
 * eval - Evaluate the cmdline.
 */
static void eval(const char *cmdline)
{
    char *argv[MAXARGS] = {NULL};
    // to judge whether to redirect later
    redirect_t redirects[MAXARGS];

    parseline(cmdline, argv, redirects);
    if (argv[0] == NULL) {
        return;
    }
    if (!builtin_cmd(argv)) {
        pid_t pid;

        if ((pid = fork()) == 0) {
            redirect(redirects);
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(3);
            }
        } else if (pid < 0) {
            unix_error("fork error");
        } else {
            inchild = true;
            if (wait(NULL) < 0) {
                unix_error("wait error");
            }
        }
    }
}

/**
 * main - The shell's main loop.
 */
int main(void)
{
    char *name = getenv("LOGNAME");

    if (name == NULL) {
        name = "";
    }
    strcat(prompt, name);
    strcat(prompt, ":");
    mysignal(SIGINT, sigint_handler);
    mysignal(SIGTSTP, sigtstp_handler);
    char cmdline[MAXLINE] = {'\0'};

    while (true) {
        set_prompt();
        fputs(prompt, stdout);
        if (fgets(cmdline, MAXLINE, stdin) == NULL && ferror(stdin)) {
            app_fatal("fgets error");
        }
        if (feof(stdin)) {
            fputs("\n", stdout);
            return 0;
        }
        eval(cmdline);
        inchild = false;
    }
    return 0;
}
#endif
