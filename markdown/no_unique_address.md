## What is `[[no_unique_address]]`

> <!-- .element: class="fragment" -->
The attribute-token no_unique_address specifies that a non-static
data member is a potentially-overlapping subobject. 

---

## Contents

- Practical Examples of using `[[no_unique_address]]` in libc++ (part of LLVM, clang's standard library)
- Lessons We Learned

---

## About Myself

---

## Example 1 : `std::expected`

```cpp [1-10|11-20]
// libstdc++
template <class Val, class Err>
class expected {
    union {
        Val val_;
        Err err_;
    };
    bool has_val_;
};

// libc++
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
// gcc
static_assert(sizeof(Foo) == 8);
static_assert(sizeof(std::expected<Foo, ErrCode>) == 12);

// clang
static_assert(sizeof(Foo) == 8);
static_assert(sizeof(std::expected<Foo, ErrCode>) == 8);
```

- [Godbolt](https://godbolt.org/z/nPhT41j3d)
- <!-- .element: class="fragment" --> What are the benefits?

---

# Example 1 : `std::expected` cont. 3

- `std::expected` is likely to be used as the return type

```cpp
std::expected<Foo, ErrCode> compute() {
    return Foo{};
}
```

```x86asm [1-8|10-13]
# gcc
compute():
        mov     DWORD PTR [rsp-24], 0
        xor     eax, eax
        mov     BYTE PTR [rsp-24], 1
        mov     ecx, DWORD PTR [rsp-24]
        mov     rdx, rcx
        ret

# clang
compute():
        movabs  rax, 281474976710656
        ret
```

---

# Takeaway 1

- Reuse tail padding with `[[no_unique_address]]`
- Cannot apply `[[no_unique_address]]` to members of anonymous `union`s
- clang ignores `[[no_unique_address]]` for C `struct`s

---

# Example 2: `std::expected` Bug

```cpp [1-19|11-18]
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

```cpp[1-13|10|11|12]
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

```
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

```cpp
std::expected<std::expected<Foo, ErrCode>, ErrCode> e = ...;
e.value().emplace(); // the inner expected construct_at will overwrite outer expected bool
```

```cpp
struct Bar {
    [[no_unique_address]] std::expected<Foo, ErrCode> e;
    char c;
};

Bar bar = ...;
bar.c = 'c';
bar.e.emplace(); // construct_at will overwrite c
```
<!-- .element: class="fragment" -->

---

# Bonus Takeaway 

- On MSVC, `[[no_unique_address]]` does nothing, use `[[msvc::no_unique_address]]` instead