#include <memory>
#include <random>
#include <vector>
#include <algorithm>

typedef uint32_t (*timer_cb)(void*);
struct timer_data {
  uint32_t deadline;
  uint32_t id;
  void* userp;
  timer_cb callback;
};

typedef uint32_t timer;

static std::vector<struct timer_data> timeouts;
static uint32_t next_id = 0;

static bool is_after(uint32_t lh, uint32_t rh)
{
  return lh < rh;
}

timer schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  auto idx = timeouts.size();
  timeouts.push_back({});
  while (idx > 0 && is_after(timeouts[idx-1].deadline, deadline)) {
      timeouts[idx] = std::move(timeouts[idx-1]);
      --idx;
  }
  timeouts[idx] = timer_data{deadline, next_id++, userp, cb };
  return next_id;
}

void cancel_timer(timer t)
{
  auto i = std::find_if(timeouts.begin(), timeouts.end(),
                        [t](const auto& e) { return e.id == t; });
  timeouts.erase(i);
}

bool shoot_first()
{
  if (timeouts.empty()) return false;
  timeouts.front().callback(timeouts.front().userp);
  timeouts.erase(timeouts.begin());
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
