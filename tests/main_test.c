#include <check.h>
#include "main.c"

START_TEST(test_parseline)
{
    char *cmdline = "ls -l  >>test.txt 2>/dev/null <test.txt 2>&- \"-a\" -bC\n";

    copybuf(cmd, cmdline, MAXLINE);
    char *argv[MAXARGS] = {NULL};
    // to judge whether to redirect later
    redirect_t redirects[MAXARGS];
    parseline(cmd, argv, redirects);

    ck_assert_ptr_ne(redirects, NULL);
    fputs("\n", stdout);
    fputs("Redirects begin:\n", stdout);
    redirect(redirects);
    fputs("Redirects end:\n\n", stdout);
    ck_assert_str_eq(argv[0], "ls");
    ck_assert_str_eq(argv[1], "-l");
    ck_assert_str_eq(argv[2], "-a");
    ck_assert_str_eq(argv[3], "-bC");
    ck_assert_ptr_eq(argv[4], NULL);
    ck_assert_str_eq(redirects[0].filename, "test.txt");
    ck_assert_int_eq(redirects[0].type, OUT | APPEND);
    ck_assert_str_eq(redirects[1].filename, "/dev/null");
    ck_assert_int_eq(redirects[1].type, ERR);
    ck_assert_str_eq(redirects[2].filename, "test.txt");
    ck_assert_int_eq(redirects[2].type, IN);
    ck_assert_str_eq(redirects[3].filename, "");
    ck_assert_int_eq(redirects[3].type, ERR | CLOSE);
    ck_assert_int_eq(redirects[4].type, NO);

    copybuf(cmd, "cd ~ &\n", MAXLINE);
    parseline(cmd, argv, redirects);
    ck_assert_str_eq(argv[0], "cd");
    ck_assert_str_eq(argv[1], "/home/qyl");

    copybuf(cmd, "ls 234 >a\n", MAXLINE);
    parseline(cmd, argv, redirects);
    fputs("\n", stdout);
    fputs("Redirects begin:\n", stdout);
    redirect(redirects);
    fputs("Redirects end:\n\n", stdout);
    ck_assert_str_eq(redirects[0].filename, "a");
    ck_assert_int_eq(redirects[0].type, OUT);
}
END_TEST

START_TEST(test_split)
{
    char *cmdline = "ls -l  >>test.txt 2>/dev/null <test.txt 2>&- \"-a\" -bC |less <a.txt |sort -b\n";

    copybuf(cmd, cmdline, MAXLINE);
    char *argv[MAXARGS] = {NULL};

    split(cmd, '|', argv);
    ck_assert_str_eq(argv[0], "ls -l  >>test.txt 2>/dev/null <test.txt 2>&- \"-a\" -bC ");
    ck_assert_str_eq(argv[1], "less <a.txt ");
    ck_assert_str_eq(argv[2], "sort -b\n");
    ck_assert_ptr_eq(argv[3], NULL);

    copybuf(cmd, "cat", MAXLINE);
    split(cmd, ';', argv);
    ck_assert_str_eq(argv[0], "cat");
    ck_assert_ptr_eq(argv[1], NULL);
}
END_TEST

START_TEST(test_builtin_cmd)
{
    char *argv1[] = {"ls"};
    char *argv3[] = {"exit"};

    ck_assert_msg(!builtin_cmd(argv1), "ls is not built-in command");
    ck_assert_msg(builtin_cmd(argv3), "exit is built-in command");
}
END_TEST

START_TEST(test_preprocess)
{
    char s[256] = "ls |cat &";

    ck_assert_msg(preprocess(s), "s is on background");
    ck_assert_str_eq(s, "ls |cat  ");
    char b[32] = "cat lse";

    ck_assert_msg(!preprocess(b), "s is on foreground");
    ck_assert_str_eq(b, "cat lse");
}
END_TEST

Suite *main_suite(void)
{
    Suite *s = suite_create("main");
    /* Core test case */
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_split);
    tcase_add_test(tc_core, test_parseline);
    tcase_add_test(tc_core, test_builtin_cmd);
    tcase_add_test(tc_core, test_preprocess);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = main_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

