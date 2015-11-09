/**
 * Author: Alan Chien
 * Email: upplane1230@gmail.com
 * Language: C
 * Date: Mon Oct 19 08:29:49 CST 2015
 * Description: Declarations of functions showing error.
 */
#pragma once

void app_fatal(const char *err_msg);
void app_error(const char *err_msg);
void unix_error(const char *err_msg);
void unix_fatal(const char *err_msg);

