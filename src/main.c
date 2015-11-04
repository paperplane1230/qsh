/**
 * Author: Alan Chien
 * Email: upplane1230@gmail.com
 * Language: C
 * Date: Sun Oct 18 09:20:42 CST 2015
 * Description: The "main" module of qsh.
 */

// use get_current_dir_name
#define _GNU_SOURCE

#include "error.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <errno.h>
#include <limits.h>

#define UNUSED(x) (void) (x)

#define MAXLINE 1024
#define MAXARGS 128

typedef void handler_t(int);

static char prompt[MAXLINE];

// judge whether to print prompt when dealing with SIGINT
static bool inchild = false;

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

/**
 * get_prompt - Get prompt with current directory.
 */
static char *get_prompt(void)
{
    static char buf[PATH_MAX];
    static bool first = true;
    // toadd points to the place for copying current directory name
    static char *toadd = buf;

    if (first) {
        strcpy(buf, prompt);
        toadd += strlen(prompt);
        first = false;
    }
    *toadd = '\0';
    // NOTE: this pointer should be **freed**!
    char *dirname = get_current_dir_name();

    if (dirname == NULL) {
        dirname = "";
    }
    strcat(toadd, dirname);
    strcat(toadd, "> ");
    free(dirname);
    dirname = NULL;
    return buf;
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
        fputs(get_prompt(), stdout);
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

/**
 * parseline - Parse the cmdline to return the parameters.
 * 
 * Note:
 * Assume the length of a line is less than MAXLINE and there's no string
 * consisting of more than one line. And it cannot deal with situations
 * where parameters are in quotes fully or partly while '-' before them is not,
 * such as `ls -"a"l`.
 */
static void parseline(const char *cmdline, char **argv)
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
    size_t argc = 0;
    char *delim = buf;

    switch (*buf) {
    case '\'':
        ++buf;
        delim = strchr(buf, '\'');
        break;
    case '\"':
        ++buf;
        delim = strchr(buf, '\"');
        break;
    default:
        delim = strchr(buf, ' ');
        break;
    }
    while (delim != NULL) {
        *delim = '\0';
        argv[argc++] = buf;
        buf = delim + 1;
        while (*buf == ' ') {
            ++buf;
        }
        switch (*buf) {
        case '\'':
            ++buf;
            delim = strchr(buf, '\'');
            break;
        case '\"':
            ++buf;
            delim = strchr(buf, '\"');
            break;
        default:
            delim = strchr(buf, ' ');
            break;
        }
    }
    argv[argc] = NULL;
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
            argv[1] = getenv("HOME");
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
    parseline(cmdline, argv);

    if (argv[0] == NULL) {
        return;
    }
    if (!builtin_cmd(argv)) {
        pid_t pid;

        if ((pid = fork()) == 0) {
            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(2);
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
#endif

#ifndef DEBUG
/**
 * main - The shell's main loop.
 */
int main(void)
{
    strcat(prompt, getenv("LOGNAME"));
    strcat(prompt, ":");
    mysignal(SIGINT, sigint_handler);
    mysignal(SIGTSTP, sigtstp_handler);
    char cmdline[MAXLINE] = {'\0'};

    while (true) {
        fputs(get_prompt(), stdout);
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
