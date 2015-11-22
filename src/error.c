/**
 * Description: Definitions of functions showing error.
 */
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * app_fatal - Dealing with fatal errors caused by application.
 */
void app_fatal(const char *err_msg)
{
    fprintf(stdout, "%s\n", err_msg);
    exit(2);
}

/**
 * app_error - Dealing with normal errors caused by application.
 */
void app_error(const char *err_msg)
{
    fprintf(stdout, "%s\n", err_msg);
}

/**
 * unix_error - Dealing with normal errors caused by system calls.
 */
void unix_error(const char *err_msg)
{
    perror(err_msg);
}

/**
 * unix_fatal - Dealing with fatal errors caused by system calls.
 */
void unix_fatal(const char *err_msg)
{
    perror(err_msg);
    exit(1);
}
