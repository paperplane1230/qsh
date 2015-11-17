/**
 * Author: Alan Chien
 * Email: upplane1230@gmail.com
 * Language: C
 * Date: Thu Nov  5 21:33:28 CST 2015
 * Description: The header of "main" module of qsh.
 */
#pragma once

#include <limits.h>

#define UNUSED(x) (void) (x)

#define MAXLINE 1024
#define MAXARGS 128

// set new file mode
#define RWRWR (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)

// to the type of redirecting
enum REDIRECT { NO = 0, OUT = 1, ERR = 2, IN = 4, CLOSE = 8, APPEND = 16, };

typedef struct _redirect_t {
    char filename[NAME_MAX];
    unsigned type;
} redirect_t;

typedef void handler_t(int);

/**
 * typetofd - Map type of redirect to file descriptor.
 * redirect : REDIRECT to be casted.
 */
static inline int typetofd(const enum REDIRECT redirect)
{
    return redirect % 4;
}
