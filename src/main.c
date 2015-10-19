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

const size_t MAXLINE = 1024;
const size_t MAXARGS = 128;

/**
 * parseline - Parse the cmdline to return the parameters.
 * 
 * Note:
 * Assume the length of a line is less than MAXLINE and there's no string
 * consisting of more than one line.
 */
static void parseline(const char *cmdline, char **argv)
{
    char buf[MAXLINE];

    strncpy(buf, cmdline, strlen(cmdline)+1);
}

/**
 * eval - Evaluate the cmdline.
 */
static void eval(const char *cmdline)
{
    char *argv[MAXARGS];

    parseline(cmdline, &argv);
}

int main(void)
{
    char cmdline[MAXLINE];

    while (true) {
        fputs("$ ", stdout);
        if (fgets(cmdline, MAXLINE, stdin) == NULL && ferror(stdin)) {
            unix_error("fgets error");
        }
        if (feof(stdin)) {
            return 0;
        }
        eval(cmdline);
    }
    return 0;
}

