#include <memory>
#include <random>
#include <map>

typedef uint32_t (*timer_cb)(void*);
struct timer_data {
  void* userp;
  timer_cb callback;
};

struct is_after {
  bool operator()(uint32_t lh, uint32_t rh) const
  {
    return lh < rh;
  }
};

using timer_map = std::multimap<uint32_t, timer_data, is_after>;
using timer = timer_map::iterator;

static timer_map timeouts;


timer schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  return timeouts.insert(std::make_pair(deadline, timer_data{userp, cb}));
}

void cancel_timer(timer t)
{
  timeouts.erase(t);
}

bool shoot_first()
{
  if (timeouts.empty()) return false;
  auto i = timeouts.begin();
  i->second.callback(i->second.userp);
  timeouts.erase(i);
  return true;
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
