# set(CMAKE_BUILD_TYPE "Debug")
# set(CMAKE_C_FLAGS_DEBUG "-fdiagnostics-color=always -std=gnu11 -g -O0 -Wall -Wextra -Winline -fno-common")
# set(CMAKE_C_FLAGS_RELEASE "-fdiagnostics-color=always -std=gnu11 -g -O2 -Wall -Wextra -Winline -fno-common")

cmake_minimum_required (VERSION 2.8)

project (qsh)

set(Demo_VERSION_MAJOR 0)
set(Demo_VERSION_MINOR 4)

aux_source_directory(. DIR_SRCS)
add_subdirectory(src)
add_subdirectory(tests)
# SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# target_link_libraries(  )
# add_library ( ${DIR_LIB_SRCS})

# enable_testing()
# macro (do_test arg1 arg2 result)
#   add_test (test_${arg1}_${arg2} Demo ${arg1} ${arg2})
#   set_tests_properties (test_${arg1}_${arg2}
#     PROPERTIES PASS_REGULAR_EXPRESSION ${result})
# endmacro (do_test)

