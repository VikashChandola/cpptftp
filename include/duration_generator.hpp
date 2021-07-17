#ifndef ___ASYNC_SLEEP_HPP__
#define ___ASYNC_SLEEP_HPP__
#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <random>

typedef std::chrono::duration<uint64_t, std::milli> ms_duration;

class duration_generator {
private:
protected:
public:
  virtual ms_duration get() = 0;
  virtual ~duration_generator(){};
};
typedef std::shared_ptr<duration_generator> duration_generator_s;

class constant_duration_generator : public duration_generator {
private:
  ms_duration delay;

public:
  constant_duration_generator(const ms_duration &_delay) : delay(_delay) {}
  ms_duration get() override { return delay; }
};
typedef std::shared_ptr<constant_duration_generator> constant_duration_generator_s;

class random_duration_generator : public duration_generator {
private:
  ms_duration lower_limit;
  ms_duration upper_limit;
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<> distrib;

public:
  random_duration_generator(ms_duration lower_limit, ms_duration upper_limit)
      : gen(rd()),
        distrib(lower_limit.count(), upper_limit.count()) {}

  random_duration_generator(const random_duration_generator &original)
      : gen(rd()),
        distrib(original.lower_limit.count(), original.upper_limit.count()) {}

  ms_duration get() { return ms_duration(distrib(rd)); }
};
typedef std::shared_ptr<random_duration_generator> random_duration_generator_s;

#endif
