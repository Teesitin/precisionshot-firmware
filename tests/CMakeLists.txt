cmake_minimum_required(VERSION 3.16)
project(precisionshot_session_tests LANGUAGES CXX)

enable_testing()
add_executable(session_test session_test.cpp ../main/session.cpp)
target_include_directories(session_test PRIVATE ../main)
target_compile_features(session_test PRIVATE cxx_std_17)
# Assertions are the test checks, including in a Release build.
if(MSVC)
    target_compile_options(session_test PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(session_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME session_and_settings COMMAND session_test)
