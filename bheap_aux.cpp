#include <memory>
#include <random>
#include <vector>
#include <new>

typedef uint32_t (*timer_cb)(void*);

template <size_t N>
struct align_allocator
{
  static_assert(N > 0);
  static_assert((N & (N - 1)) == 0, "size must be power of two");
  template <typename T>
  struct type {
    static_assert(N >= alignof(T));

    using value_type = T;
    static constexpr std::align_val_t alignment{N};
    T* allocate(size_t n)
    {
      return static_cast<T*>(operator new(n*sizeof(T), alignment));
    }
    void deallocate(T* p, size_t)
    {
      operator delete(p, alignment);
    }
  };
};

class timeout_store
{
public:
  struct timer_data {
    uint32_t deadline;
    uint32_t action_index;
  };
  void push(timer_data);
  const timer_data& top() const;
  void pop();
  bool empty() const;
private:
  static constexpr size_t block_size = 8;
  static constexpr size_t block_mask = block_size - 1;

  static size_t child_of(size_t idx);
  static size_t parent_of(size_t idx);
  static bool is_block_root(size_t idx);
  static size_t block_offset(size_t idx);
  static size_t block_base(size_t idx);
  static bool is_block_leaf(size_t idx);
  static size_t child_no(size_t idx);

  std::vector<timer_data, align_allocator<64>::type<timer_data>> bheap_store;
};

inline size_t timeout_store::child_of(size_t idx)
{
  if (!is_block_leaf(idx)) return idx + block_offset(idx);
  auto base = block_base(idx) + 1;
  return base * block_size + child_no(idx) * block_size * 2 + 1;
}

inline size_t timeout_store::parent_of(size_t idx)
{
  auto const node_root = block_base(idx);
  if (!is_block_root(idx)) return node_root + block_offset(idx) / 2;
  auto parent_base = block_base(node_root / block_size - 1);
  auto child = ((idx - block_size) / block_size - parent_base) / 2;
  return parent_base + block_size / 2 + child;
}

inline bool timeout_store::is_block_root(size_t idx)
{
  return block_offset(idx) == 1;
}

inline size_t timeout_store::block_offset(size_t idx)
{
  return idx & block_mask;
}

inline size_t timeout_store::block_base(size_t idx)
{
  return idx & ~block_mask;
}

inline bool timeout_store::is_block_leaf(size_t idx)
{
  return (idx & (block_size >> 1)) != 0U;
}

inline size_t timeout_store::child_no(size_t idx)
{
  return idx & (block_mask >> 1);
}

inline void timeout_store::push(timer_data d)
{
  if ((bheap_store.size() & block_mask) == 0) {
    bheap_store.emplace_back();
  }
  auto hole_idx = bheap_store.size();
  bheap_store.emplace_back();
  while (hole_idx != 1) {
    auto parent_idx = parent_of(hole_idx);
    auto& parent_data = bheap_store[parent_idx];
    if (parent_data.deadline < d.deadline) break;
    bheap_store[hole_idx] = parent_data;
    hole_idx = parent_idx;
  }
  bheap_store[hole_idx] = d;
}

inline const timeout_store::timer_data& timeout_store::top() const
{
  return bheap_store[1];
}

inline void timeout_store::pop()
{
  size_t idx = 1;
  const auto last_idx = bheap_store.size() - 1;
  for (;;) {
    auto left_child = child_of(idx);
    if (left_child > last_idx) break;
    auto const sibling_offset = is_block_leaf(idx) ? block_size : 1;
    auto right_child = left_child + sibling_offset;
    bool go_right = right_child < last_idx && bheap_store[right_child].deadline <  bheap_store[left_child].deadline;
    auto next_child = left_child + go_right*sibling_offset;
    bheap_store[idx] = bheap_store[next_child];
    idx = next_child;
  }
  if (idx != last_idx) {
    auto last = bheap_store.back();
    while (idx != 1) {
      auto parent_idx = parent_of(idx);
      if (bheap_store[parent_idx].deadline < last.deadline) break;
      bheap_store[idx] = bheap_store[parent_idx];
      idx = parent_idx;
    }
    bheap_store[idx] = last;
  }
  bheap_store.resize(last_idx - ((last_idx & block_mask) == 1));
}

inline bool timeout_store::empty() const
{
  return bheap_store.empty();
}

struct timer_action {
  timer_cb callback;
  void* userp;
};

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

static timeout_store timeouts;
static action_store actions;

using timer = uint32_t;

timer schedule_timer(uint32_t deadline, timer_cb cb, void* userp)
{
  auto action_index = actions.push(cb, userp);
  timeouts.push({deadline, action_index});
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
      auto deadline = dist(gen);
      timer t = schedule_timer(deadline,
                               [](void*) {return 0U;},
                               reinterpret_cast<void*>(deadline));
      if (i & 1) cancel_timer(prev);
      prev = t;
    }
    int i = 0;
    while (shoot_first())
      ++i;
  }
}
