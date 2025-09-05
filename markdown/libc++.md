# What is libc++

- <!-- .element: class="fragment" -->
  A Standard Library Implementation
- <!-- .element: class="fragment" -->
  Part of LLVM Project
- <!-- .element: class="fragment" -->
  Ships with `clang`

---

## Contents

- <!-- .element: class="fragment" -->
  Various Optimisations in the Library Implementation
  - <!-- .element: class="fragment" -->
    Space Optimisations
  - <!-- .element: class="fragment" -->
    Algorithm Optimisations
- <!-- .element: class="fragment" -->
  Testing

---

## About Me

- Hui Xie (GitHub @huixie90)
- <!-- .element: class="fragment" -->
  Software Developer @Qube Research & Technologies
- <!-- .element: class="fragment" -->
  libc++ contributor
- <!-- .element: class="fragment" -->
  BSI (WG21 UK National Body) member
- <!-- .element: class="fragment" -->
  WG21 (C++ Standard Committee) member
- <!-- .element: class="fragment" -->
  Active in SG9 (Ranges Study Group)
- <!-- .element: class="fragment" -->
  Write C++ Proposals

---

<!-- TODO: Add a short example to introduce what std::expected is, along with an intuition for how it might be implemented. -->

---

## `std::expected`

```cpp
class Foo {
    int i;
    char c;
    bool b;
};

enum class ErrCode : int { Err1, Err2, Err3 };
```
<!-- .element: class="fragment" data-fragment-index="0" -->

<div class="r-stack">

```cpp
static_assert(sizeof(Foo) == ?);
```
<!-- .element: class="fragment fade-in" data-fragment-index="0" -->

```cpp
static_assert(sizeof(Foo) == 8); // on most implementations
```
<!-- .element: class="fragment fade-in" data-fragment-index="1" -->

</div>

```cpp
static_assert(sizeof(std::expected<Foo, ErrCode>) == ?);
```
<!-- .element: class="fragment fade-in" data-fragment-index="2" -->

---

## `std::expected` in libstdc++

```cpp
// gcc libstdc++
template <class Val, class Err>
class expected {
    union {
        Val val_;
        Err err_;
    };
    bool has_val_;
};
```

```cpp
static_assert(sizeof(std::expected<Foo, ErrCode>) == ?);
```

---

## `std::expected` in libstdc++

```cpp [1-3|5-7]
// gcc libstdc++
static_assert(sizeof(std::expected<Foo, ErrCode>) == 12);
```

```cpp [1-6 | 1,2 |1,3 |1,4 | 1,5|1,6]
int3 | int2 | int1 | int0 | char | bool | padd. | padd. | has_val | padd. | padd. | padd.
<-------------- Foo Data --------------->
                                        <- Foo padding ->
<--------------- std::expected<Foo, ErrCode> Data ---------------->
                                                                  <-- expected padding -->
<----------------------------- std::expected<Foo, ErrCode> ------------------------------>
```
<!-- .element: class="fragment" -->

---

## `std::expected` in libc++

```cpp
// clang libc++  simplified version
template <class Val, class Err>
class expected {
    union U {
        [[no_unique_address]] Val val_;
        [[no_unique_address]] Err err_;
    };
    [[no_unique_address]] U u_;
    bool has_val_;
};
```

```cpp
static_assert(sizeof(std::expected<Foo, ErrCode>) == ?);
```

> <!-- .element: class="fragment" -->
The attribute-token no_unique_address specifies that a non-static
data member is a potentially-overlapping subobject.

---

## `std::expected` in libc++

```cpp [1-6 | 1,2 |1,3 |1,4 | 1,5 |1,6]
int3 | int2 | int1 | int0 | char | bool | has_value | padding
<-------------- Foo Data --------------->
                                        <---- Foo Padding --->
<----- std::expected<Foo, ErrCode> Data ------------>
                                                    <-ex pad->
<------------ std::expected<Foo, ErrCode> ------------------->
```

``` cpp
// clang libc++
static_assert(sizeof(std::expected<Foo, ErrCode>) == 8);
```
<!-- .element: class="fragment" -->

- <!-- .element: class="fragment" --> What are the benefits?

---

## `sizeof(std::expected)` Smaller, Better ?

- <!-- .element: class="fragment" --> Smaller Memory Footprint
- <!-- .element: class="fragment" --> Better Cache Locality
- <!-- .element: class="fragment" -->
  `std::expected` is likely to be used as a return type

---

## `std::expected` `return` Type

```cpp
std::expected<Foo, ErrCode> compute() { return Foo{}; }
```

```x86asm
# gcc libstdc++
compute():
        mov     DWORD PTR [rsp-24], 0
        xor     eax, eax
        mov     BYTE PTR [rsp-24], 1
        mov     ecx, DWORD PTR [rsp-24]
        mov     rdx, rcx
        ret
```
<!-- .element: class="fragment" -->

```x86asm
# clang libc++
compute():
        movabs  rax, 281474976710656
        ret
```
<!-- .element: class="fragment" -->

---

## Takeaway 1

- <!-- .element: class="fragment" -->
  Reuse tail padding with `[[no_unique_address]]`
  - <!-- .element: class="fragment" -->
    Including the padding of an empty type

---

## A Nasty Bug

```cpp [1-19|11-18|12|13|14|16]
template <class Val, class Err>
class expected {
    union U {
        [[no_unique_address]] Val val_;
        [[no_unique_address]] Err err_;
    };
    [[no_unique_address]] U u_;
    bool has_val_;

  public:
    expected(const expected& other)
        : has_val_(other.has_val_) {
        if (has_val_) {
          std::construct_at(std::addressof(u_.val_), other.u_.val_);
        } else {
          std::construct_at(std::addressof(u_.err_), other.u_.err_);
        }
    }
};
```

---

## A Nasty Bug

```cpp [1-5|2|3|4]
int main() {
    std::expected<Foo, int> e1(Foo{});
    std::expected e2(e1);
    assert(e2.has_value());
}
```

```bash
output.s: /app/example.cpp:15: int main(): Assertion `e2.has_value()' failed.
Program terminated with signal: SIGSEGV
```
<!-- .element: class="fragment" -->

---

## Zero-Initialisation

> <!-- .element: class="fragment" -->
To zero-initialize an object or reference of type T means:
...
if T is a (possibly cv-qualified) non-union class type, its padding bits ([basic.types.general]) are initialized to zero bits and ...

---

## A Nasty Bug

```cpp [12|14|4|8]
template <class Val, class Err>
class expected {
    union U {
        [[no_unique_address]] Val val_;
        [[no_unique_address]] Err err_;
    };
    [[no_unique_address]] U u_;
    bool has_val_;

  public:
    expected(const expected& other)
        : has_val_(other.has_val_) {
        if (has_val_) {
          std::construct_at(std::addressof(u_.val_), other.u_.val_);
        } else {
          std::construct_at(std::addressof(u_.err_), other.u_.err_);
        }
    }
};
```

---

## Takeaway 2

- Don't mix manual lifetime management (`union`, `construct_at`, etc.) and `[[no_unique_address]]`

<!-- TODO: Clarify that you don't mean manual MEMORY management, such as new/delete. I don't know how to best clarify that. Maybe saying it out loud is enough. -->

---

## Another Example: `stop_token`

<--! TODO: Add an example of what that thing is. It will be important to give an intuition about how that thing is implemented, like some kind of shared state and whatever else. -->

---

## Under The Hood

```cpp [1-16 | 1-4 |6-9 | 11-16]
class stop_token {
private:
  std::shared_ptr<__stop_state> state_;
};

class stop_source {
private:
  std::shared_ptr<__stop_state> state_;
};

template <class Callback>
class stop_callback : private stop_callback_base {
private:
  [[no_unique_address]] Callback callback_;
  std::shared_ptr<__stop_state> state_;
};
```

---

## But What About The Shared State?

- What member variables do we need in the "shared state"
  - <!-- .element: class="fragment" -->
    `stop_requested()`: needs a flag to hold whether a stop was requested
  - <!-- .element: class="fragment" -->
    `stop_possible()`: needs to count how many `stop_source`s exist
  - <!-- .element: class="fragment" -->
    `stop_callback`: needs to store references to all the `stop_callback`s
  - <!-- .element: class="fragment" -->
    `stop_callback`: needs to synchronise the list of callbacks
    - <!-- .element: class="fragment" -->
      major requirement is that the whole thing is `noexcept`, ruling out `mutex`
  - <!-- .element: class="fragment" -->
     The state needs a ref count to manage its lifetime
  
---

## A Naive Implementation

<!-- TODO: Add a code snippet showing a naive implementation of `__stop_state` using `std::list` and whatever. -->

---

## libc++'s Implementation

<!-- TODO: Do some slide magic to hide and show the ref_count_, and explain that it allows you to use an intrusive shared ptr instead of a real one. -->

```cpp [1-15|2-7|9-11|13]
class __stop_state {
  // The "callback list locked" bit implements a 1-bit lock to guard
  // operations on the callback list
  //
  //       31 - 2          |  1                   |    0           |
  //  stop_source counter  | callback list locked | stop_requested |
  atomic<uint32_t> state_ = 0;

  // Reference count for stop_token + stop_callback + stop_source
  atomic<uint32_t> ref_count_ = 0;

  // Lightweight intrusive non-owning list of callbacks to call when a
  // stop is requested
  __intrusive_list_view<stop_callback_base> callback_list_;
};
```

---

## Takeaway 3

- Sometimes we can reuse unused bits to save space

---

## `segmented_iterator`: `for_each`

```cpp [1-11 | 3-6 | 8 -11]
std::deque<int> d = ...

// 1
for (int& i : d) {
    i = std::clamp(i, 200, 500);
}

// 2
std::ranges::for_each(d, [](int& i) {
    i = std::clamp(i, 200, 500);
});
```

---

## `segmented_iterator`: `for_each` benchmark

```bash
Benchmark                             for loop     for_each

[for_loop vs. for_each]/32             12.5 ns      3.69 ns
[for_loop vs. for_each]/8192           2973 ns       259 ns
[for_loop vs. for_each]/65536         24221 ns      3327 ns
```

---

## `std::deque`

![deque](./img/libc++/deque.png)

---

## `std::deque` `for` Loop

```cpp [1-6|3]
deque_iterator begin = d.begin();
deque_iterator end = d.end();
for ( ; begin != end; ++begin ) {
  int& i = *begin;
  i = std::clamp(i, 200, 500);
}
```

```cpp [1-7 | 2 | 3-4]
deque_iterator& operator++() {
  if (++elem_ptr_ - *block_iter_ == block_size) {
    ++block_iter_;
    elem_ptr_ = *block_iter_;
  }
  return *this;
}
```
<!-- .element: class="fragment" -->

---

## Segmented Iterators Concept

- [Segmented Iterators and Hierarchical Algorithms, Matt Austern](http://lafstern.org/matt/segmented.pdf)
- @philnik777 introduced `Segmented Iterator` concept into libc++
- <!-- .element: class="fragment" -->
  Iterators over ranges of sub-ranges
- <!-- .element: class="fragment" -->
  Allows algorithms to operate over these multi-level iterators natively

---

## `for_each_segment`

```cpp [1-13 | 4-5 | 8-11 | 9]
template <class SegmentedIterator, class Func>
void __for_each_segment(SegmentedIterator first, SegmentedIterator last, Func func) {
  using Traits = SegmentedIteratorTraits<SegmentedIterator>;
  auto seg_first = Traits::segment(first);
  auto seg_last  = Traits::segment(last);
  // some code handles the first segment ...
  // iterate over the segments
  while (seg_first != seg_last) {
    func(Traits::begin(seg_first), Traits::end(seg_first));
    ++seg_first;
  }
  // some code handles the last segment ...
}
```

```cpp [1-6|4]
template <class SegmentedIterator, class Func> requires is_segmented_iterator<SegmentedIterator>
void for_each(SegmentedIterator first, SegmentedIterator last, Func func) {
  std::__for_each_segment(first, last, [&](auto inner_first, auto inner_last) {
    std::for_each(inner_first, inner_last, func);
  });
}
```
<!-- .element: class="fragment" -->

---

## Code Generation Comparison

![deque_godbolt](./img/libc++/deque_godbolt.png)

---

## `segmented_iterator` `copy`

```cpp [1-14 | 4-10 | 12-14]
std::vector<std::vector<int>> v = ...;
std::vector<int> out;

// 1
out.reserve(total_size);
for (const auto& inner : v) {
  for (int i: inner) {
    out.push_back(i);
  }
}

// 2
out.resize(total_size);
std::ranges::copy(v | std::views::join, out.begin());
```

---

## `segmented_iterator`: `copy` benchmark

```bash
Benchmark                         for loop         copy

[for_loop vs. copy]/32              490 ns       322 ns
[for_loop vs. copy]/8192         160632 ns     63372 ns
[for_loop vs. copy]/65536       1066403 ns    208083 ns
```

---

## `join_view::iterator` is a Segmented Iterator

```cpp [1-7 | 3 | 4]
template <class Iter, class OutIter> requires is_segmented_iterator<Iter>
pair<Iter, OutIter> __copy(Iter first, Iter last, OutIter result) const {
  std::__for_each_segment(first, last, [&](auto inner_first, auto inner_last) {
      result = std::__copy(inner_first, inner_last, std::move(result)).second;
  });
  return std::make_pair(last, std::move(result));
}
```

```cpp [1-5 | 3]
template <class In, class Out> requires can_lower_copy_assignment_to_memmove<In, Out>
pair<In*, Out*> __copy(In* first, In* last, Out* result) const {
  std::__constexpr_memmove(result, first, last - first);
  return std::make_pair(last, result + n);
}
```
<!-- .element: class="fragment" -->

---

## Takeaway 4

- <!-- .element: class="fragment" -->
  We constantly add optimisations to algorithms, please use them

- <!-- .element: class="fragment" -->
  We try to categorise iterators/ranges in algorithms

- <!-- .element: class="fragment" -->
  We reuse existing optimisations

---

## `flat_map` Insertion

```cpp [1-7 | 4-7]
std::flat_map<int, double> m1 = ...;
std::flat_map<int, double> m2 = ...;

// Insert the elements of m2 into m1
for (const auto& [key, val] : m2) {
  m1.emplace(key, val);
}
```

What is the time complexity?
<!-- .element: class="fragment" -->

---

## `flat_map::insert_range`

```cpp
template<container-compatible-range<value_type> R>
constexpr void insert_range(R&& rg);
```

- Complexity: `N + MlogM`, where `N` is `size()` before the operation and `M` is `ranges​::​distance(rg)`.

```cpp [1-5 | 5]
std::flat_map<int, double> m1 = ...;
std::flat_map<int, double> m2 = ...;

// Insert the elements of m2 into m1
m1.insert_range(m2);
```
<!-- .element: class="fragment" -->

---

## `flat_map::insert_range` Implementation

```cpp [1-13 | 4 | 6-7 | 9 | 11-12 | 4 ]
template<container-compatible-range<value_type> R>
constexpr void insert_range(R&& rg) {

  __append(ranges::begin(rg), ranges::end(rg)); // O(M)

  auto zv = ranges::views::zip(keys_, values_);
  ranges::sort(zv.begin() + old_size, zv.end()); // O(MLogM)

  ranges::inplace_merge(zv.begin(), zv.begin() + old_size, zv.end()); // O(M+N)

  auto dup_start = ranges::unique(zv).begin(); // O(M+N)
  __erase(dup_start); // O(M+N)
}
```

---

## How to `__append` ?

```cpp [1-8 | 4 | 5-6]
template <class InputIterator, class Sentinel>
void __append(InputIterator first, Sentinel last) {
  for (; first != last; ++first) {
    std::pair<Key, Val> kv = *first;
    keys_.insert(keys_.end(), std::move(kv.first));
    values_.insert(values_.end(), std::move(kv.second));
  }
}
```

- <!-- .element: class="fragment" -->
  Missed existing optimisations in `vector::insert(pos, first, last)`
- <!-- .element: class="fragment" -->
  But we were given a range of `pair-like`

---

## Introducing a Concept `product_iterator`

- iterators that contain two or more underlying iterators
- `flat_map::iterator` is `product_iterator`

```cpp [1-11 | 1 | 4-6 | 8-10]
template <class Iter> requires is_product_iterator_of_size<Iter, 2>
void __append(Iter first, Iter last)
{
  keys_.insert(keys_.end(),
      product_iterator_traits<Iter>::template get_iterator_element<0>(first),
      product_iterator_traits<Iter>::template get_iterator_element<0>(last));

  values_.insert(values_.end(),
      product_iterator_traits<Iter>::template get_iterator_element<1>(first),
      product_iterator_traits<Iter>::template get_iterator_element<1>(last));
}
```
<!-- .element: class="fragment" -->

---

## `product_iterator` benchmark

```bash
Benchmark                 insert_pair  product_iterator

[insert_range]/32              149 ns             74 ns
[insert_range]/8192          26682 ns           2995 ns
[insert_range]/65536        226235 ns          27844 ns
```

---

## Is there any other `product_iterator`?

```cpp [1-5]
std::flat_map<int, double> m = ...;
std::vector<int> newKeys = ...;
std::vector<double> newValues = ...;

// Insert newKeys and newValues into m
```

```cpp
m.insert_range(std::views::zip(newKeys, newValues));
```
<!-- .element: class="fragment" -->

`zip_view::iterator` is also a `product_iterator`
<!-- .element: class="fragment" -->

---

## Takeaway 5

- <!-- .element: class="fragment" -->
  We try to categorise iterators/ranges in container operations too

- <!-- .element: class="fragment" -->
  We reuse existing optimisations

---

## Testing

- <!-- .element: class="fragment" -->
  Testing almost every single word in the standard

- <!-- .element: class="fragment" -->
  Our tests are also used by other implementations (msvc-stl)

---

## What Shall We Test?

```cpp
constexpr expected(expected&& rhs) noexcept(see below);
```

<div class="small-bullets">

- Constraints:
  - `is_move_constructible_v<T>` is `true` and
  - `is_move_constructible_v<E>` is `true`.

- Effects: If `rhs.has_value()` is `true`, direct-non-list-initializes val with `std​::​move(*rhs)`. Otherwise, direct-non-list-initializes `unex` with `std​::​move(rhs.error())`.

- Postconditions: `rhs.has_value()` is unchanged; `rhs.has_value() == this->has_value()` is true.

- Throws: Any exception thrown by the initialization of `val` or `unex`.

- Remarks: The exception specification is equivalent to `is_nothrow_move_constructible_v<T> && is_nothrow_move_constructible_v<E>`.

- This constructor is trivial if
  - `is_trivially_move_constructible_v<T>` is `true` and
  - `is_trivially_move_constructible_v<E>` is `true`.

</div>

---

## Testing `constexpr`

```cpp [1-19 | 1 | 2-8 | 6-7 | 15-16]
constexpr bool test() {
  // Test point 1
  {
    std::expected<MoveOnly, int> e1 = ...;
    std::expected<MoveOnly, int> e2 = std::move(e1);
    assert(e2.has_value());
    assert(e2.value() == ... ); 
  }
  // more test points

  return true;
}

int main(int argc, char** argv) {
  test();
  static_assert(test());

  return 0;
}
```

- <!-- .element: class="fragment" -->
  Share compile time and run time tests

---

## Testing Constraints

```cpp
constexpr expected(expected&& rhs) noexcept(see below);
```

- Constraints: `is_move_constructible_v<T>` is `true` and `is_move_constructible_v<E>` is `true`.

- <!-- .element: class="fragment" -->
  Constraints translate to `requires`

```cpp [1-6 | 1 | 3-6]
struct Foo { Foo(Foo&&) = delete;};

static_assert(std::is_move_constructible_v<std::expected<int,int>>);
static_assert(!std::is_move_constructible_v<std::expected<Foo,int>>);
static_assert(!std::is_move_constructible_v<std::expected<int,Foo>>);
static_assert(!std::is_move_constructible_v<std::expected<Foo,Foo>>);
```
<!-- .element: class="fragment" -->

---

## Testing Mandates

```cpp
template<class U = remove_cv_t<T>> constexpr T value_or(U&& v) const &;
```

- Mandates: `is_copy_constructible_v<T>` is `true` and `is_convertible_v<U, T>` is true.

- <!-- .element: class="fragment" -->
  Mandates translate to `static_assert`

- <!-- .element: class="fragment" -->
  Test that usage that violates the mandate should not compile

```cpp
const std::expected<NonCopyable, int> f1{5};
f1.value_or(5); // expected-note{{in instantiation of function template specialization 'std::expected<NonCopyable, int>::value_or<int>' requested here}}
// expected-error-re@*:* {{static assertion failed {{.*}}value_type has to be copy constructible}}
```
<!-- .element: class="fragment" -->

```bash
-Xclang -verify
```
<!-- .element: class="fragment" -->

---

## Testing {Hardened} Preconditions

```cpp
constexpr const T* operator->() const noexcept;
constexpr T* operator->() noexcept;
```

- Hardened preconditions: `has_value()` is `true`.

- <!-- .element: class="fragment" -->
  Preconditions/Hardened preconditions translate to `assert`/`contract_assert`/other assert macros

```cpp
std::expected<int, int> e{std::unexpect, 5};
TEST_LIBCPP_ASSERT_FAILURE(e.operator->(),
    "expected::operator-> requires the expected to contain a value");
```
<!-- .element: class="fragment" -->

- <!-- .element: class="fragment" -->
  `TEST_LIBCPP_ASSERT_FAILURE` installs assertion handler, fork the process
   and match the child process stderr with the expected errors

---

## Takeaway 6

- `constexpr` tests are shared with normal runtime tests
- We test the standard details extensively, including negative tests
- `-Xclang -verify` is very useful


---

## We Are Hiring

- https://www.qube-rt.com/careers/

