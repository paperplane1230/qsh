set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_FLAGS_DEBUG "-fdiagnostics-color=always -std=gnu11 -g -O0 -Wall -Wextra -Winline -fno-common -DDEBUG -lm -lrt -lpthread -lcheck")
set(CMAKE_C_FLAGS_RELEASE "-fdiagnostics-color=always -std=gnu11 -g -O2 -Wall -Wextra -Winline -fno-common -DDEBUG -lm -lrt -lpthread -lcheck")

set(HEADERS ../src/error.h ../src/main.h)
add_executable(qsh_test main_test.c ../src/error.c ${HEADERS})
TARGET_LINK_LIBRARIES(qsh_test check)
TARGET_LINK_LIBRARIES(qsh_test pthread)
TARGET_LINK_LIBRARIES(qsh_test m)
TARGET_LINK_LIBRARIES(qsh_test rt)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
include_directories(../src)

# target_link_libraries(  )
# add_library ( ${DIR_LIB_SRCS})

# enable_testing()
# macro (do_test arg1 arg2 result)
#   add_test (test_${arg1}_${arg2} Demo ${arg1} ${arg2})
#   set_tests_properties (test_${arg1}_${arg2}
#     PROPERTIES PASS_REGULAR_EXPRESSION ${result})
# endmacro (do_test)


