#include <memory>
#include <random>
#include <queue>

typedef uint32_t (*timer_cb)(void*);
struct timer_data {
  uint32_t deadline;
  uint32_t action_index;
};

struct timer_action {
  timer_cb callback;
  void* userp;
};

struct is_after {
  bool operator()(const timer_data& lh, const timer_data& rh) const
  {
    return lh.deadline < rh.deadline;
  }
};


static
std::priority_queue<timer_data, std::vector<timer_data>, is_after> timeouts;

struct action_store
{
  union timer_store
  {
    uint32_t next_free;
    timer_action cb;
  };
  uint32_t push(timer_cb cb, void* userp)
  {
    if (first_free == data.size()) {
      timer_store st;
      st.cb.callback = cb;
      st.cb.userp = userp;

      data.push_back(st);
      return std::exchange(first_free, first_free+1);
    }
    else
    {
      auto idx = std::exchange(first_free, data[first_free].next_free);
      data[idx].cb.callback = cb;
      data[idx].cb.userp = userp;
      return idx;
    }
  }
  void remove(uint32_t idx)
  {
    data[idx].next_free = first_free;
    first_free = idx;
  }
  void cancel(uint32_t idx)
  {
    data[idx].cb.callback = nullptr;
  }
  const timer_action& operator[](uint32_t idx) const
  {
    return data[idx].cb;
  }
  uint32_t first_free = 0;
  std::vector<timer_store> data;
};

static action_store actions;

using timer = uint32_t;

timer schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  auto action_index = actions.push(cb, userp);
  timeouts.push(timer_data{deadline, action_index});
  return action_index;
}

void cancel_timer(timer t)
{
  actions.cancel(t);
}

bool shoot_first()
{
  while (!timeouts.empty()) {
    auto& t = timeouts.top();
    auto& action = actions[t.action_index];
    if (action.callback) break;
    actions.remove(t.action_index);
    timeouts.pop();
  }
  if (timeouts.empty()) return false;
  auto& t = timeouts.top();
  auto& action = actions[t.action_index];
  action.callback(action.userp);
  actions.remove(t.action_index);
  timeouts.pop();
  return true;
}

int main()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist;

  for (int k = 0; k < 1000; ++k) {
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
