## What is `[[no_unique_address]]`

> <!-- .element: class="fragment" -->
The attribute-token no_unique_address specifies that a non-static
data member is a potentially-overlapping subobject. 

---

## Contents

- Practical Examples of using `[[no_unique_address]]` in libc++ (part of LLVM, clang's standard library)
- All Real World Examples
- Lessons We Learned

---

## About Myself

- Hui Xie (GitHub @huixie90)
- <!-- .element: class="fragment" -->
  Software Developer @Qube Research & Technologies
- <!-- .element: class="fragment" -->
  libc++ (clang's STL) contributor
- <!-- .element: class="fragment" -->
  BSI (WG21 UK National Body) member
- <!-- .element: class="fragment" -->
  WG21 SG9 (Committee Ranges study group) regular
- <!-- .element: class="fragment" -->
  Write C++ Proposals

---

## Example 1 : `std::expected`

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
// clang libc++
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
<!-- .element: class="fragment" -->

- <!-- .element: class="fragment" --> What are the benefits?

---

# Example 1 : `std::expected` cont.

```cpp [1-8|10|11]
struct Foo {
    Foo() = default;
  private:
    int i;
    char c;
    bool b;
};
enum class ErrCode : int { Err1, Err2, Err3 };

static_assert(sizeof(Foo) == ?);
static_assert(sizeof(std::expected<Foo, ErrCode>) == ?);
```

---

# Example 1 : `std::expected` cont. 2

```cpp [1-3|5-7]
// gcc libstdc++
static_assert(sizeof(Foo) == 8);
static_assert(sizeof(std::expected<Foo, ErrCode>) == 12);
```

```cpp [1-7 | 1,2 |1,3 |1,4 | 1,5|1,6|1,7]
int3 | int2 | int1 | int0 | char | bool | padd. | padd. | has_val | padd. | padd. | padd.
<-------------- Foo Data --------------->
                                        <- Foo padding ->
<------------------------- Foo ------------------------->
<--------------- std::expected<Foo, ErrCode> Data ---------------->
                                                                  <-- expected padding -->
<----------------------------- std::expected<Foo, ErrCode> ------------------------------>
```
<!-- .element: class="fragment" -->

---

# Example 1 : `std::expected` cont. 3

``` cpp
// clang libc++
static_assert(sizeof(Foo) == 8);
static_assert(sizeof(std::expected<Foo, ErrCode>) == 8);
```

```cpp [1-7 | 1,2 |1,3 |1,4 | 1,5 |1,6|1,7]
int3 | int2 | int1 | int0 | char | bool | has_value | padding
<-------------- Foo Data --------------->
                                        <---- Foo Padding --->
<--------------------------- Foo ---------------------------->
<----- std::expected<Foo, ErrCode> Data ------------>
                                                    <-ex pad->
<------------ std::expected<Foo, ErrCode> ------------------->
```
<!-- .element: class="fragment" -->

- <!-- .element: class="fragment" --> What are the benefits?
- <!-- .element: class="fragment" -->
  [Godbolt](https://godbolt.org/z/nPhT41j3d)

---

# Example 1 : `std::expected` cont. 4

- `std::expected` is likely to be used as the return type

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

# Takeaway 1

- <!-- .element: class="fragment" -->
  Reuse tail paddings with `[[no_unique_address]]`
- <!-- .element: class="fragment" -->
  Cannot apply `[[no_unique_address]]` to members of anonymous `union`s
- <!-- .element: class="fragment" -->
  clang ignores `[[no_unique_address]]` for C `struct`s

---

# Example 2: `std::expected` Bug

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

# Example 2: `std::expected` Bug cont.

```cpp [1-13|10|11|12]
struct Foo {
    Foo() = default;
  private:
    int i;
    char c;
    bool b;
};

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

- <!-- .element: class="fragment" --> 
[Godbolt](https://godbolt.org/z/86azsznPx)

---

# Example 2: `std::expected` Bug cont.2

```cpp
template<class T, class... Args>
  constexpr T* construct_at(T* location, Args&&... args);
```

Effects: Equivalent to:

```cpp [1-4 | 4]
if constexpr (is_array_v<T>)
  return ::new (voidify(*location)) T[1]();
else
  return ::new (voidify(*location)) T(std::forward<Args>(args)...);
```

> <!-- .element: class="fragment" -->
To zero-initialize an object or reference of type T means:
...
if T is a (possibly cv-qualified) non-union class type, its padding bits ([basic.types.general]) are initialized to zero bits and ...

---

# Example 2: `std::expected` Bug cont.4

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


# Example 2: `std::expected` Bug cont.5

![comment](./img/no_unique_address/gh_comments.png)

---

# Example 2: Fix Attempt 1

```cpp [12-16|17]
template <class Val, class Err>
class expected {
    union U {
        [[no_unique_address]] Val val_;
        [[no_unique_address]] Err err_;
    };
    [[no_unique_address]] U u_;
    bool has_val_;

  public:
    expected(const expected& other) {
        if (other.has_val_) {
          std::construct_at(std::addressof(u_.val_), other.u_.val_);
        } else {
          std::construct_at(std::addressof(u_.err_), other.u_.err_);
        }
        has_val_ = other.has_val_;
    }
};
```

---

# Example 2: Fix Attempt 1 cont.

- Make sure `has_val_` is written **after** `construct_at` is called

- <!-- .element: class="fragment" -->
What if the padding bits contain other data ?

---

# Example 2: Fix Attempt 1 cont.

```cpp [1|2]
std::expected<std::expected<Foo, ErrCode>, ErrCode> e = ...;
e.value().emplace(); // the inner expected construct_at will overwrite outer expected bool
```

```cpp [1-7 | 1,2 |1,3 |1,4|1,5|1,6|1,7 | 1,4,8]
int3 | int2 | int1 | int0 | char | bool | has_value_1 | has_value2
<-------------- Foo Data --------------->
                                        <------- Foo padding ---->
<----------------------------- Foo ------------------------------>
<--------------- Inner expected Data ---------------->
<--------------------- Outer expected Data ---------------------->
<-------------- std::expected<Foo, ErrCode> --------------------->
<--- std::expected<std::expected<Foo, ErrCode>, ErrCode> -------->
```
<!-- .element: class="fragment" -->


---

# Example 2: Fix Attempt 1 cont.2

```cpp [1-4| 6|7|8]
struct Bar {
    [[no_unique_address]] std::expected<Foo, ErrCode> e;
    char c;
};

Bar bar = ...;
bar.c = 'c';
bar.e.emplace(); // construct_at will overwrite c
```

```cpp [1-6 | 1,2 |1,3 |1,4|1,5|1,6|1,7|1,4,8]
int3 | int2 | int1 | int0 | char | bool | has_value | char c
<-------------- Foo Data --------------->
                                        <--- Foo padding --->
<----------------------------- Foo ------------------------->
<----------------- expected Data ------------------>
                                                   <ex. pad.>
<-------------- std::expected<Foo, ErrCode> ---------------->
<---------------------------- Bar -------------------------->
```
<!-- .element: class="fragment" -->

---

# Example 2: Fix Attempt 2

```cpp [1-13|3-10|12]
template <class Val, class Err>
class expected {
    struct repr {
        union U {
            [[no_unique_address]] Val val_;
            [[no_unique_address]] Err err_;
        };
        [[no_unique_address]] U u_;
        bool has_val_;
    };

    repr repr_; // no [[no_unique_address]] on this member
};
```

---

# Example 2: Fix Attempt 2 cont.

```cpp
struct Bar {
    [[no_unique_address]] std::expected<Foo, ErrCode> e;
    char c;
};
```

```cpp [1-7 | 1,2 |1,3 |1,4 | 1,5 |1,6|1, 7|1,8|1,4,9|1,10-12]
int3 | int2 | int1 | int0 | char | bool | has_val | pad. | char c | pad. | pad. | pad.
<-------------- Foo Data --------------->
                                        <- Foo Padding -->
<--------------------------- Foo ------------------------>
<-------------------- repr Data ------------------>
                                                  <rep pd>
<-------------------------- repr ------------------------>
<------- std::expected<Foo, ErrCode> Data --------------->
<------------ std::expected<Foo, ErrCode> --------------->
<------------------------- Bar Data ------------------------------>
                                                                  <-- Bar padding --->
<---------------------------------- Bar  -------------------------------------------->
```
<!-- .element: class="fragment" -->

---

# Example 2: Fix Attempt 2 cont.2

- <!-- .element: class="fragment" -->
  `bool has_val_` still in `val_`/`err_`'s padding if they have, to save space

- <!-- .element: class="fragment" -->
  `repr_` can potentially contain paddings

- <!-- .element: class="fragment" -->
  `expected` no longer contains paddings (and same size as `repr`)

- <!-- .element: class="fragment" -->
  `[[no_unique_address]]` is not recursive

- <!-- .element: class="fragment" -->
  `repr_`'s padding cannot be reused to prevent overwriting user data

- <!-- .element: class="fragment" -->
  It fixed the issue, but is it optimal?

---

# Example 2: Fix Attempt 2 cont.3

- If `bool has_val_` is not in `val_`/`err_`'s padding, `construct_at` can never overwrite padding beyond the `bool`

```cpp [1-9 | 1 | 3-5 |6-9]
std::expected<unsigned int, int> e = 0x12345678;

[int byte3, int byte2, int byte1, int byte0, bool has_value, padding, padding, padding]
     78        56          34         12         01            00        00       00
<------------- unsigned int -------------->
<------------------ repr data ------------------------------->
                                                              <----- repr padding ---->
<--------------------------------------- repr ---------------------------------------->
<------------------ std::expected<unsigned int, int> --------------------------------->
```

- <!-- .element: class="fragment" -->
  In this case, `repr`'s padding can be safely reused

---

# Example 2: Fix Attempt 3


```cpp [1-9 | 11-14 | 15 | 17 | 20-21]
template <class Val, class Err>
struct repr {
    union U {
        [[no_unique_address]] Val val_;
        [[no_unique_address]] Err err_;
    };
    [[no_unique_address]] U u_;
    bool has_val_;
};

template <class Val, class Err>
struct expected_base {
    repr<Val, Err> repr_; // no [[no_unique_address]]
};
template <class Val, class Err> requires bool_is_not_in_padding
struct expected_base {
    [[no_unique_address]] repr<Val, Err> repr_;
};

template <class Val, class Err>
class expected : expected_base<Val, Err> {};
```

---

# Takeaway 2

- If you use `[[no_unique_address]]` with `construct_at`/placement `new`, you probably have a bug
- <!-- .element: class="fragment" -->
  `[[no_unique_address]]` is not recursive

---

# Same Issue in `ranges::__movable_box`

```cpp [1-12|3|8]
template <__movable_box_object T>
class __movable_box {
    [[no_unique_address]] T val_;
  public:
    constexpr __movable_box& operator=(__movable_box&& __other) {
      if (this != std::addressof(__other)) {
        std::destroy_at(std::addressof(__val_));
        std::construct_at(std::addressof(__val_), __other.__val_);
      }
      return *this;
    }
};
```

```cpp [1-6|4-5]
template <input_range View, move_constructible Fn>
  requires __transform_view_constraints<View, Fn>
class transform_view : public view_interface<transform_view<_View, _Fn>> {
  [[no_unique_address]] __movable_box<Fn> func_;
  [[no_unique_address]] View base_ = View();
};
```

---

# Same Issue in `__movable_box` cont.

```cpp
int main() {
    auto f = [l = 0.0L, b = false](int i) { return i; };

    auto v1 = std::vector{1, 2, 3, 4, 5} | std::views::transform(f);
    auto v2 = std::vector{1, 2, 3, 4, 5} | std::views::transform(f);

    v1 = std::move(v2);

    for (auto x : v1) {  // SEGV here
        std::cout << x;
    }
}
```

- We had the same problem in `ranges`. [Godbolt](https://godbolt.org/z/K9hYzqcba)

---

# Example 3:  EBO

```cpp [1-7 | 6]
template <class T, class Alloc>
class vector {
  pointer begin_;
  pointer end_;
  pointer end_cap_;
  allocator_type alloc_;
};
```

---

# Example 3:  EBO cont.

```cpp [1-6 | 2]
template <class T, class Alloc>
class vector : private Alloc {
  pointer begin_;
  pointer end_;
  pointer end_cap_;
};
```

---

# Example 3:  EBO cont.2

```cpp [1-6 | 5]
template <class T, class Alloc>
class vector {
  pointer begin_;
  pointer end_;
  __compressed_pair<pointer, allocator_type> end_cap_and_alloc_;
};
```

---

# Example 3:  EBO cont.3

```cpp [1-11 | 1-4 | 6-7 | 9-11]
template <class T, int Idx, bool CanBeEmptyBase = is_empty<T>::value>
struct __compressed_pair_elem {
  T value;
};

template <class T, int Idx>
struct __compressed_pair_elem<T, Idx, true> : private T {};

template <class T1, class T2>
class __compressed_pair : private __compressed_pair_elem<T1, 0>
                        , private __compressed_pair_elem<T2, 1> {};
```

---

# Example 3:  EBO cont.4

```cpp [1-7 | 6]
template <class T, class Alloc>
class vector {
  pointer begin_;
  pointer end_;
  pointer end_cap_;
  [[no_unique_address]] allocator_type alloc_;
};
```

---

# Example 3:  EBO cont.5

```cpp [1-5]
template <class T1, class T2>
class __compressed_pair {
  [[no_unique_address]] T1 first;
  [[no_unique_address]] T2 second;
};
```

---

# Takeaway 3

- <!-- .element: class="fragment" -->
  Replace EBO with `[[no_unique_address]]`
- <!-- .element: class="fragment" -->
  Only one of them can be empty if there are multiple members of the same empty type

---

# Example 4: Conditional Member

```cpp [1-11|6-7|8-10]
template<input_range V, forward_range Pattern>
  requires blah...
class join_with_view : public view_interface<join_with_view<V, Pattern>> {

  V base_ = V();                                       // exposition only
  non-propagating-cache<iterator_t<V>> outer_it_;      // exposition only, present only
                                                       // when !forward_range<V>
  non-propagating-cache<remove_cv_t<InnerRng>> inner_; // exposition only, present only
                                                       // if is_reference_v<InnerRng> is
                                                       // false
};
```
---

# Example 4: Conditional Member cont.

```cpp [1-21|1-2|4-7|9-10|12-15|19-20]
template <class V>
struct outer_it_base {};

template <class V> requires (!forward_range<V>)
struct outer_it_base<V> {
  non_propagating_cache<iterator_t<V>> outer_it_;
};

template <class InnerRng>
struct inner_base {};

template <class InnerRng> requires(!is_reference_v<InnerRng>)
struct inner_base<InnerRng> {
  non_propagating_cache<remove_cv_t<InnerRng>> inner_;
};

template<input_range V, forward_range Pattern> requires blah...
class join_with_view : public view_interface<join_with_view<V, Pattern>>
                     , private outer_it_base<V>
                     , private inner_base<InnerRng> {
};
```
---

# Example 4: Conditional Member cont.2

```cpp [1-11|3|4-5|6-7|9-10]
template<input_range V, forward_range Pattern> requires blah...
class join_with_view : public view_interface<join_with_view<V, Pattern>>{
  struct empty {};
  using OuterCache = conditional_t<!forward_range<V>,
                      non_propagating_cache<iterator_t<V>>, empty>;
  using InnerCache = conditional_t<!is_reference_v<InnerRng>,
                      non_propagating_cache<remove_cv_t<InnerRng>>, empty>;

  [[no_unique_address]] OuterCache outer_it_;
  [[no_unique_address]] InnerCache inner_;
};
```
---

# Example 4: Conditional Member cont.3

```cpp [1-11|3|5|7]
template<input_range V, forward_range Pattern> requires blah...
class join_with_view : public view_interface<join_with_view<V, Pattern>>{
  template <int> struct empty {};
  using OuterCache = conditional_t<!forward_range<V>,
                      non_propagating_cache<iterator_t<V>>, empty<0>>;
  using InnerCache = conditional_t<!is_reference_v<InnerRng>,
                      non_propagating_cache<remove_cv_t<InnerRng>>, empty<1>>;

  [[no_unique_address]] OuterCache outer_it_;
  [[no_unique_address]] InnerCache inner_;
};
```
---

# Takeaway 4

- Use `[[no_unique_address]] conditional_t<Cond, T, empty>` to approximate
  conditional member variable

---

# Bonus Takeaway 

- On MSVC, `[[no_unique_address]]` does nothing, use `[[msvc::no_unique_address]]` instead

---

# Summary

- Reuse tail paddings with `[[no_unique_address]]`
- Be extra care when `[[no_unique_address]]` is used together with `construct_at`/Placement `new`
- Use `[[no_unique_address]]` for empty type optimisation
- Use `[[no_unique_address]]` for conditional member variable

---

# QRT is Hiring

- https://www.qube-rt.com/careers/