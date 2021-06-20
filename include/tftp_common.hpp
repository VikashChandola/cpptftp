#ifndef __TFTP_COMMON_HPP__
#define __TFTP_COMMON_HPP__

#include <boost/asio.hpp>

typedef boost::asio::chrono::duration<uint64_t, std::milli> ms_duration;

static const ms_duration default_network_timeout(1000);
static const uint16_t default_max_retry_count(3);

class Waiter {
private:
  uint64_t index = 0;
  std::function<ms_duration()> gen_wait_time;
  ms_duration lower_limit;
  ms_duration upper_limit;
  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<> distrib;
  ms_duration step_size;

public:
  // Constant duration delay
  Waiter(ms_duration delay) {
    this->gen_wait_time = [delay]() { return delay; };
  }

  // Random delay constructor. Generate duration in range lower_limit to upper_limit
  Waiter(ms_duration lower_limit, ms_duration upper_limit)
      : gen(rd()),
        distrib(lower_limit.count(), upper_limit.count()) {
    this->gen_wait_time = [=]() { return ms_duration(distrib(rd)); };
  }

  // Increases delay in size of step_size, Generates duration stepwise from lower_limit to upper_limit then
  // again from lower_limit
  Waiter(ms_duration lower_limit, ms_duration upper_limit, ms_duration step_size)
      : lower_limit(lower_limit),
        upper_limit(upper_limit),
        step_size(step_size) {
    this->gen_wait_time = [=]() {
      ms_duration new_duration = this->lower_limit + this->index * this->step_size;
      if (new_duration > this->upper_limit) {
        this->index  = 0;
        new_duration = this->lower_limit + this->index * this->step_size;
      }
      this->index++;
      return new_duration;
    };
  }

  ms_duration operator()() { return this->gen_wait_time(); }
};

#if 0
//test code for waiter
int main() {
  ms_duration one_sec(1000);

  Waiter punctual_waiter_1(one_sec);
  Waiter punctual_waiter_2(5 * one_sec);
  std::cout << punctual_waiter_1().count() << "\t" << punctual_waiter_2().count() << std::endl;
  std::cout << punctual_waiter_1().count() << "\t" << punctual_waiter_2().count() << std::endl;
  std::cout << punctual_waiter_1().count() << "\t" << punctual_waiter_2().count() << std::endl;
  std::cout << punctual_waiter_1().count() << "\t" << punctual_waiter_2().count() << std::endl;
  std::cout << punctual_waiter_1().count() << "\t" << punctual_waiter_2().count() << std::endl;
 
  std::cout << std::endl;
  Waiter random_waiter_1(one_sec, 5 * one_sec);
  Waiter random_waiter_2(5 * one_sec, 9 * one_sec);
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  std::cout << random_waiter_1().count() << "\t" << random_waiter_2().count() << std::endl;
  
  std::cout << std::endl;
  Waiter steady_waiter_1(one_sec, 5 * one_sec, one_sec);
  Waiter steady_waiter_2(10 * one_sec, 20 * one_sec, 2 * one_sec);
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  std::cout << steady_waiter_1().count() << "\t" << steady_waiter_2().count() << std::endl;
  return 0;
}
#endif

#endif
