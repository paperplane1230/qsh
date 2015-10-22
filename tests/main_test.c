#include <check.h>
#include "main.c"

START_TEST(test_parseline)
{
    char *argv[16] = {NULL};

    parseline("ls -l  \"-a\" -bC\n", argv);
    ck_assert_str_eq(argv[0], "ls");
    ck_assert_str_eq(argv[1], "-l");
    ck_assert_str_eq(argv[2], "-a");
    ck_assert_str_eq(argv[3], "-bC");

    parseline("echo \"heeh\"\n", argv);
    ck_assert_str_eq(argv[0], "echo");
    ck_assert_str_eq(argv[1], "heeh");
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

Suite *main_suite(void)
{
  Suite *s = suite_create("main");
  /* Core test case */
  TCase *tc_core = tcase_create("Core");

  tcase_add_test(tc_core, test_parseline);
  tcase_add_test(tc_core, test_builtin_cmd);
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

