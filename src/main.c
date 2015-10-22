/*
 * Author: Alan Chien
 * Email: upplane1230@gmail.com
 * Language: C
 * Date: Sun Oct 18 09:20:42 CST 2015
 * Description: The "main" module of qsh.
 */
#include "error.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXLINE 1024
#define MAXARGS 128

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

    if (*buf == '\'') {
        ++buf;
        delim = strchr(buf, '\'');
    } else if (*buf == '\"') {
        ++buf;
        delim = strchr(buf, '\"');
    } else {
        delim = strchr(buf, ' ');
    }
    while (delim != NULL) {
        *delim = '\0';
        argv[argc++] = buf;
        buf = delim + 1;
        while (*buf == ' ') {
            ++buf;
        }
        if (*buf == '\'') {
            ++buf;
            delim = strchr(buf, '\'');
        } else if (*buf == '\"') {
            ++buf;
            delim = strchr(buf, '\"');
        } else {
            delim = strchr(buf, ' ');
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
        if (chdir(argv[1]) != 0) {
            unix_error("chdir error");
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
            if (wait(NULL) < 0) {
                unix_error("wait error");
            }
        }
    }
}
#endif

#ifndef DEBUG
int main(void)
{
    char cmdline[MAXLINE] = {'\0'};

    while (true) {
        fputs("> ", stdout);
        if (fgets(cmdline, MAXLINE, stdin) == NULL && ferror(stdin)) {
            app_error("fgets error");
        }
        if (feof(stdin)) {
            fputs("\n", stdout);
            return 0;
        }
        eval(cmdline);
    }
    return 0;
}
#endif
