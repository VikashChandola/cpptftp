#define ASSERT_HANDLE gdb_handle
#include "test.hpp"

void gdb_handle(){
  std::cout << "gdb handle" << std::endl;
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


