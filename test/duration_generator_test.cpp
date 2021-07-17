#define BOOST_TEST_MODULE duration generator test
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>
#include <unordered_map>

#include "duration_generator.hpp"

namespace bdata = boost::unit_test::data;

BOOST_DATA_TEST_CASE(constant_sleep_duration_test, bdata::make({1, 2, 3, 4, 5}), sample) {
  ms_duration second(sample);
  constant_duration_generator cdg(second);
  duration_generator &gen = cdg;

  // Geneartor must always return same duration
  BOOST_TEST(gen.get().count() == sample);
  BOOST_TEST(gen.get().count() == sample);
  BOOST_TEST(gen.get().count() == sample);
  BOOST_TEST(gen.get().count() == sample);
}

BOOST_TEST_DONT_PRINT_LOG_VALUE(ms_duration)
BOOST_DATA_TEST_CASE(random_sleep_duration_test,
                     bdata::make({ms_duration(1000)}) ^ bdata::make({ms_duration(2000)}),
                     lower,
                     upper) {
  const uint32_t invokation_count = 1000;
  class ms_duration_hasher {
  public:
    std::size_t operator()(const ms_duration &key) const noexcept { return key.count(); }
  };

  std::unordered_map<ms_duration, uint32_t, ms_duration_hasher> map;

  random_duration_generator rdg(lower, upper);
  duration_generator &gen = rdg;

  for (uint32_t i = 0; i < invokation_count; i++) {
    ms_duration duration = gen.get();
    // duration must be in between requested range
    BOOST_TEST((duration <= upper && duration >= lower));
    map[duration] += 1;
  }
  // To check that there are limited repeatation. In case generator start to return same duration.
  BOOST_TEST(map.size() > invokation_count / 10);
}
