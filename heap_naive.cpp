#include <memory>
#include <random>
#include <vector>
#include <algorithm>

extern "C" {
  typedef uint32_t (*timer_cb)(void*);
  using timer = uint32_t;
  struct timer_data {
    uint32_t deadline;
    uint32_t id;
    void* userp;
    timer_cb callback;
  };
  timer next_id = 0;

  bool is_before(const timer_data& lh, const timer_data& rh)
  {
    return lh.deadline < rh.deadline;
  }

  static std::vector<timer_data> timeouts;


timer schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  timeouts.push_back(timer_data{deadline, next_id, userp, cb});
  std::push_heap(timeouts.begin(), timeouts.end(), is_before);
  return next_id++;
}

  void cancel_timer(timer t)
  {
    auto i = std::find_if(timeouts.begin(), timeouts.end(),
                          [t](const auto& e) { return e.id == t; });
    if (i != timeouts.end()) i->callback = nullptr;
  }

  bool shoot_first()
  {
    while (!timeouts.empty() && timeouts.front().callback == nullptr) {
      std::pop_heap(timeouts.begin(), timeouts.end(), is_before);
      timeouts.pop_back();
    }
    if (timeouts.empty()) return false;
    timeouts.front().callback(timeouts.front().userp);
    std::pop_heap(timeouts.begin(), timeouts.end(), is_before);
    timeouts.pop_back();
    return true;
  }
}
int main()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist;

  for (int k = 0; k < 10; ++k) {
    timer prev{};
    for (int i = 0; i < 20'000; ++i) {
      timer t = schedule_timer(dist(gen), [](void*){return 0U;}, nullptr);
      if (i & 1) cancel_timer(prev);
      prev = t;
    }
    while (shoot_first())
      ;
  }
}
