/**
 * Author: Alan Chien
 * Email: upplane1230@gmail.com
 * Language: C
 * Date: Mon Oct 19 08:35:13 CST 2015
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
    exit(1);
}

/**
 * app_error - Dealing with normal errors caused by application.
 */
void app_error(const char *err_msg)
{
    fprintf(stdout, "%s\n", err_msg);
}

/**
 * unix_error - Dealing with errors caused by system calls.
 */
void unix_error(const char *err_msg)
{
    perror(err_msg);
    exit(1);
}
