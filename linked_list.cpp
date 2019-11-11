#include <memory>
#include <random>


typedef uint32_t (*timer_cb)(void*);
struct timer {
  uint32_t deadline;
  void* userp;
  timer_cb callback;
  struct timer* next;
  struct timer* prev;
};

static timer timeouts = { 0, NULL, NULL, &timeouts, &timeouts };

static
timer* add_behind(timer* anchor, uint32_t deadline, timer_cb cb, void* userp)
{
  timer* t = (timer*)malloc(sizeof(timer));
  t->deadline = deadline;
  t->userp = userp;
  t->callback = cb;
  t->prev = anchor;
  t->next = anchor->next;
  anchor->next->prev = t;
  anchor->next = t;
  return t;
}

static bool is_after(uint32_t lh, uint32_t rh)
{
  return lh < rh;
}

timer* schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  timer* iter = timeouts.prev;
  while (iter != &timeouts &&
         is_after(iter->deadline, deadline))
    iter = iter->prev;
  return add_behind(iter, deadline, cb, userp);
}

void cancel_timer(timer* t)
{
  t->next->prev = t->prev;
  t->prev->next = t->next;
  free(t);
}

bool shoot_first()
{
  if (timeouts.next == &timeouts) return false;
  timer* t = timeouts.next;
  t->callback(t->userp);
  cancel_timer(t);
  return true;
}

int main()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist;

  for (int k = 0; k < 10; ++k) {
    timer* prev = nullptr;
    for (int i = 0; i < 20'000; ++i) {
      timer* t = schedule_timer(dist(gen), [](void*){return 0U;}, nullptr);
      if (i & 1) cancel_timer(prev);
      prev = t;
    }
    while (shoot_first())
      ;
  }
}
