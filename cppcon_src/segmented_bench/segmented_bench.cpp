#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <deque>
#include <iterator>
#include <numeric>
#include <random>
#include <ranges>

#include <iostream>
#include <unordered_map>
#include <vector>

namespace {

template <class T> T create_data(std::size_t size) {
  std::random_device r;
  std::mt19937 rng(r());
  std::uniform_int_distribution<int> dist(0, 1000);
  T res;
  for (auto i = 0u; i < size; ++i) {
    res.push_back(dist(rng));
  }
  return res;
}

const auto inputs = create_data<std::deque<int>>(65536);
void BM_for_loop(benchmark::State &state) {
  auto deq = create_data<std::deque<int>>(state.range(0));
  std::vector<int> vec(deq.size());
  for (auto _ : state) {
    for (auto i = 0uz; i < deq.size(); ++i) {
      vec[i] = deq[i];
    }
    benchmark::DoNotOptimize(vec);
  }
}

void BM_ranges(benchmark::State &state) {
  auto deq = create_data<std::deque<int>>(state.range(0));
  std::vector<int> vec(deq.size());
  for (auto _ : state) {
    std::ranges::copy(deq, vec.begin());
    benchmark::DoNotOptimize(vec);
  }
}

/*
void BM_for_loop(benchmark::State &state) {
  auto deq = create_data<std::deque<int>>(state.range(0));
  for (auto _ : state) {
    state.PauseTiming();
    auto vec = create_data<std::vector<int>>(state.range(0));
    state.ResumeTiming();
    auto old_size = vec.size();
    vec.resize(old_size + deq.size());
    for (auto i = 0uz; i < deq.size(); ++i) {
      vec[i + old_size] = deq[i];
    }
    benchmark::DoNotOptimize(vec);
  }
}


void BM_ranges(benchmark::State &state) {
  auto deq = create_data<std::deque<int>>(state.range(0));
  for (auto _ : state) {
    state.PauseTiming();
    auto vec = create_data<std::vector<int>>(state.range(0));
    state.ResumeTiming();
    auto old_size = vec.size();
    vec.resize(old_size + deq.size());
    std::ranges::copy(deq, vec.begin() + old_size);
    benchmark::DoNotOptimize(vec);
  }
}
*/

BENCHMARK(BM_for_loop)->Arg(32)->Arg(8192)->Arg(65536);
BENCHMARK(BM_ranges)->Arg(32)->Arg(8192)->Arg(65536);

const auto random = create_data<std::deque<int>>(65536);

auto copy_data(std::size_t size) {
  std::deque<int> res(size);
  std::ranges::copy(random.begin(), random.begin() + size, res.begin());
  return res;
}

void BM_for_each_loop(benchmark::State &state) {
  // auto deq = create_data<std::deque<int>>(state.range(0));
  auto deq = copy_data(state.range(0));
  for (auto _ : state) {
    int result = 0;
    for (int i : deq) {
      result += i;
    }

    benchmark::DoNotOptimize(result);
  }
}

void BM_for_each_algo(benchmark::State &state) {
  // auto deq = create_data<std::deque<int>>(state.range(0));
  auto deq = copy_data(state.range(0));
  for (auto _ : state) {
    int result = 0;
    std::ranges::for_each(deq, [&result](int i) { result += i; });

    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_for_each_loop)->Arg(32)->Arg(8192)->Arg(65536);
BENCHMARK(BM_for_each_algo)->Arg(32)->Arg(8192)->Arg(65536);

void BM_for_each_loop2(benchmark::State &state) {
  // auto deq = create_data<std::deque<int>>(state.range(0));
  auto deq = copy_data(state.range(0));
  for (auto _ : state) {
    for (int& i : deq) {
      i = std::clamp(i, 200, 500);
    }

    benchmark::DoNotOptimize(deq);
  }
}

void BM_for_each_algo2(benchmark::State &state) {
  // auto deq = create_data<std::deque<int>>(state.range(0));
  auto deq = copy_data(state.range(0));
  for (auto _ : state) {
    std::ranges::for_each(deq, [](int& i) { 
      i = std::clamp(i, 200, 500); });

    benchmark::DoNotOptimize(deq);
  }
}

BENCHMARK(BM_for_each_loop2)->Arg(32)->Arg(8192)->Arg(65536);
BENCHMARK(BM_for_each_algo2)->Arg(32)->Arg(8192)->Arg(65536);

std::vector<int> create_random_with_random_length(std::size_t max_size) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<std::size_t> size_dist(0, max_size);
  std::size_t size = size_dist(gen);

  std::uniform_int_distribution<int> value_dist(0, 1000);

  std::vector<int> result;
  result.reserve(size);

  for (std::size_t i = 0; i < size; ++i) {
    result.push_back(value_dist(gen));
  }

  return result;
}

std::vector<std::vector<int>> create_nested(std::size_t n_outer,
                                            std::size_t max_n_inner) {
  std::vector<std::vector<int>> nested;
  nested.reserve(n_outer);
  for (std::size_t i = 0; i < n_outer; ++i) {
    nested.push_back(create_random_with_random_length(max_n_inner));
  }
  return nested;
}

const auto data = [] {
  std::unordered_map<std::size_t, std::vector<std::vector<int>>> data_map;
  data_map[32] = create_nested(100, 32);
  data_map[8192] = create_nested(100, 8192);
  data_map[65536] = create_nested(100, 65536);
  return data_map;
}();

void BM_join_loop(benchmark::State &state) {
  auto input = data.at(state.range(0));
  auto total_size =
      std::accumulate(input.begin(), input.end(), 0uz,
                      [](std::size_t sum, const std::vector<int> &inner) {
                        return sum + inner.size();
                      });
  for (auto _ : state) {
    std::vector<int> result;
    result.reserve(total_size);
    for (const auto &inner : input) {
      for (int i : inner) {
        result.push_back(i);
      }
    }

    benchmark::DoNotOptimize(result);
  }
}

void BM_join_view(benchmark::State &state) {
  auto input = data.at(state.range(0));
  auto total_size =
      std::accumulate(input.begin(), input.end(), 0uz,
                      [](std::size_t sum, const std::vector<int> &inner) {
                        return sum + inner.size();
                      });
  for (auto _ : state) {
    std::vector<int> result(total_size);
    std::ranges::copy(input | std::views::join, result.begin());

    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_join_loop)->Arg(32)->Arg(8192)->Arg(65536);
BENCHMARK(BM_join_view)->Arg(32)->Arg(8192)->Arg(65536);

} // namespace