/**
 * Description: Declarations of functions showing error.
 */
#pragma once

void app_fatal(const char *err_msg);
void app_error(const char *err_msg);
void unix_error(const char *err_msg);
void unix_fatal(const char *err_msg);

