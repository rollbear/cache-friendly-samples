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

struct timer
{
  uint32_t deadline;
  uint32_t id;
};

static std::vector<struct timer_data> timeouts;
static uint32_t next_id = 0;

static bool is_after(const timer_data& lh, const timer_data& rh)
{
  return lh.deadline < rh.deadline;
}


timer schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  timer_data element{deadline, next_id, userp, cb};
  auto i = std::lower_bound(timeouts.begin(), timeouts.end(),
                            element, is_after);
  timeouts.insert(i, element);
  return {deadline, next_id++};
}

void cancel_timer(timer t)
{
  timer_data element{t.deadline, t.id, nullptr, nullptr};
  auto [lo, hi] = std::equal_range(timeouts.begin(), timeouts.end(),
                                   element, is_after);
  auto i = std::find_if(lo, hi,
                        [t](const auto& e) { return e.id == t.id; });
  if (i != hi) {
    std::move(i + 1, timeouts.end(), i);
    timeouts.pop_back();
  }
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
