#ifndef __TEST_HPP__
#define __TEST_HPP__

#include <iostream>
#include <cstdint>
#include <ctime>
#include <limits>


/*
OUTSTREAM allows logs to be thrown in specific channel. User can set this as 
macro to any stream. By default it is set to std::cout.

setting ASSERT_HANDLE allows ASSERT_HANDLE method to be called in case of assert failure
user must create a method naming "void ASSERT_HANDLE()". This is for ease of debugging in
GDB. User can set breakpoint on this method and whenever assert error happens it will
hit the breakpoint.
WARNING :You are shooting yourself in foot if ASSERT_HANDLE is not set properly. example
#define ASSERT_HANDLE main // what the fuck is expected ?
#define ASSERT_HANDLE // Where is it supposed to go on error

[ERR/WARN/INFO/LOG]_OUT can be configured to specific std::ostream handler to point
corresponding logs into specific stream
[PASS/FAIL]_OUT to stream PASS FAIL logs

General test case Usage guideline
 * Each test suite/case is a method.
 * Each test sute/case function signature must return TEST_REUSLT
 * A test can have multiple sub tests. If it does it's called a test suite otherwise test case.
 * For test suites do not return status manually. Begin functio with TEST_BEGIN and end with TEST_END
 * A suite is considered failed if any of the test in the suite failed
 * 'ASSERT' should be used for checking correctness
 * Only leaft case(read 3) should ASSERT. A call to assert breaks execution(ref ASSERT_HANDLE)
 * 'CALL'  macro should be used for calling test cases.
 * 'STATUS' macro can be used after each CALL to print status of last call.
 * Consider giving status of a test will STATUS call rather than using [PASS/FAIL]_OUT manually

Example code

#define ASSERT_HANDLE gdb_handle
#include "test.hpp"

void gdb_handle(){
  std::cout << "breakpoint on this method" << std::endl;
}

TEST_RESULT test1() {
  ASSERT(true, "test 1 pass");
  return TEST_PASS;
}
TEST_RESULT test2(int a) {
  ASSERT(false, "test 2 fail");
  return TEST_PASS;
}
TEST_RESULT test_suite1() {
  TEST_BEGIN;
  CALL(test1);
  STATUS("Suite 1 Test 1 with no arguments");
  CALL(test2, 5);
  STATUS("Sute 1 Test 2 with one arguments");
  TEST_END;
}

TEST_RESULT test_suite2() {
  TEST_BEGIN;
  CALL(test1);
  STATUS("Suite 2 Test 1 with no arguments");
  TEST_END;
}
int main(){
  TEST_BEGIN;
  CALL(test_suite1);
  STATUS("test suite 1");
  CALL(test_suite2);
  STATUS("test suite 2");
  TEST_END;
}

Here we have 2 layers(main and test_suite[1,2] of suites and 1 case[1,2]. main and test_suite1 are failed
because at least one leave is failure test_suite2 is passed suite. User can put breakpoint gdb_handle and
all failure will stop there for debugging. This is handy with "meson test --gdb"

*/

//TEST tool configuration end

#ifndef OUTSTREAM
#define OUTSTREAM std::cout
#endif



#define TEST_RESULT int8_t

#define TEST_PASS (0)
#define TEST_FAIL (1)

#ifndef ERR_OUT
#define ERR_OUT   OUTSTREAM << "ERROR    :"
#endif

#ifndef WARN_OUT
#define WARN_OUT  OUTSTREAM << "WARNING  :"
#endif

#ifndef INFO_OUT
#define INFO_OUT  OUTSTREAM << "INFO     :"
#endif

#ifndef LOG_OUT
#define LOG_OUT   OUTSTREAM << "LOG      :"
#endif

#ifndef PASS_OUT
#define PASS_OUT  INFO_OUT << "[PASS]     :"
#endif

#ifndef FAIL_OUT
#define FAIL_OUT  INFO_OUT << "[FAIL]     :"
#endif

#define TEST_BEGIN \
TEST_RESULT __RESULT = TEST_PASS;\
TEST_RESULT __THIS_RESULT = TEST_PASS; \
do { \
  INFO_OUT << "----------------" << __func__  << std::endl; \
}while(0)

#define TEST_END \
do { \
  INFO_OUT << "----------------" << __func__  << std::endl; \
  (void)__THIS_RESULT; \
  return __RESULT; \
}while(0)


#ifdef ASSERT
#undef ASSERT
#endif

#ifdef ASSERT_HANDLE
#define __ASSERT_ABORT do { ASSERT_HANDLE(); } while(0)
#else
#define __ASSERT_ABORT do { } while(0)
#endif

#define ASSERT(expr_result, reason) \
do { \
  if (!(expr_result)) { \
    FAIL_OUT << reason << std::endl; \
    ERR_OUT << "ASSERT :" << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl; \
    __ASSERT_ABORT; \
    return TEST_FAIL; \
  } \
} while (0)

#define CALL(function, ...) \
do { \
  if ( function( __VA_ARGS__)  == TEST_FAIL) { \
    __RESULT = TEST_FAIL; \
    __THIS_RESULT = TEST_FAIL; \
    ERR_OUT << "CALL :" << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl; \
  } else {\
    __THIS_RESULT = TEST_PASS; \
  }\
} while(0)

#define STATUS(status_string) \
do { \
  if (__THIS_RESULT == TEST_PASS){ \
    PASS_OUT << status_string << std::endl; \
  } else { \
    FAIL_OUT << status_string << std::endl; \
  } \
} while(0)

template<typename T>
T random_number(){
  static bool seeded = false;
  if(!seeded){
    std::srand(std::time(0));
    seeded = true;
  }
  auto int_random = std::rand();
  float ratio = static_cast<float>(int_random)/ (float)(RAND_MAX);
  float t_range = static_cast<float>(std::numeric_limits<T>::max()-std::numeric_limits<T>::min());
  T random = std::numeric_limits<T>::min() + static_cast<T>(t_range*ratio);
  return random;
}

#endif
