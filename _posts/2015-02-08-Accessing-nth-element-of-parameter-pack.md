---
layout: post
title: "Accessing nth element of parameter pack"
date: 2015-02-08 21:38:06 +0100
comments: true
---

Since c++11 [parameter pack][cppreference_parameter_pack] was introduced.
It can be used to store non-type template _values_, type templates,
template templates or function arguments. In this post I will describe
how to access parameter pack elements by index in function call.

###First element

Accessing front is a simple query.
In fact primitive implementation would not even
require variadic template.

``` cpp
namespace fused {
  template <typename Tp>
  constexpr decltype(auto) front(Tp&& t, ...) noexcept;
}
```

First argument `Tp&& t` is of deduced type `Tp&&`.
When `Tp` is a template parameter `Tp&&` it is not just
rvalue reference, but [forwarding reference][n4164]
that can bind to both prvalue/xvalue and lvalue references.
Body of function `fused::front` is a one-liner just
returning `t` by [std::forward][cppreference_forward] cast.

The key thing here is how to pass anything else beside `t`
in a generic way. Passing via c-style [ellipsis][cppreference_ellipsis]
will _swallow_ trailing arguments, but it does not work for
uncopyable types like [std::unique_ptr][cppreference_unique_ptr]:

```
error: cannot pass objects of non-trivially-copyable type %T through ‘...’
```

Following implementation overcomes this problem
with unnamed parameter pack `Us&&...`:

``` cpp
namespace fused {
  template <typename Tp, typename... Us>
  constexpr decltype(auto) front(Tp&& t, Us&&...) noexcept {
    return std::forward<Tp>(t);
  }
}
```

`Us&&...` expands to any number of forwarding references.

``` cpp
// test
// bind rvalue
auto x = fused::front(unsigned{1u}, 2.0, std::make_unique<char>('3'));
assert(x == 1u);

// bind lvalue
unsigned a = 4u;
auto const& y = fused::front(a, 2.0, std::make_unique<char>('3'));
assert(y == 4u);
assert(&a == &y);
```

###second

Accessing second argument is not so trivial as `fused::front`.
Ellipsis or parameter pack must appear last,
so it cannot be used to _swallow_ non-trailing arguments.
To _swallow_ single argument in front `meta::ignore` can be used:

``` cpp
namespace meta {
  struct ignore final {
    template <typename... Ts>
    constexpr ignore(Ts&&...) noexcept {}
  };
}
```

Job of `meta::ignore` is to literally do nothing. It has only
a constructor, that allows to create `meta::ignore` instance
of any arguments without modifying them. It is possible to
implicitly convert any type to `meta::ignore`.

``` cpp
namespace fused {
  template <typename Tp>
  decltype(auto) second(meta::ignore, Tp&& t, ...) {
    return std::forward<Tp>(t);
  }
}
```

``` cpp
// test
// implicitly convert first argument to ignore
auto sec = fused::second(std::make_unique<unsigned>(1u),
  2.0, '3');
assert(sec == 2.0);
```

###back

Accessing last argument of function call requires _swallowing_
all preceding ones. _N_-argument
function signature should consist of _N-1_ `meta::ignore`s
and single trailing argument that will be returned.
Variadic template cannot be followed by a single parameter,
`fused::back` uses a little trick to do so.

``` cpp
namespace fused {
  template <typename Tp, typename... Ts>
  constexpr decltype(auto) back(Tp&& t, Ts&&... ts) noexcept;
}
```

Function `fused::back` expects at least one argument
and potentially zero-length tail. Additional consequence of above
_N_-argument function signature split is that length of tail, `sizeof...(Ts)`,
is exactly equal to required _N-1_.

Wrapping `meta::ignore` in simple template alias allows to _generate_ numerous
implicit conversions regardless of used type.

``` cpp
namespace meta {
  template <typename>
  using eat = ignore;

  template <std::size_t>
  using eat_n = ignore;
}
```

Function `fused::back` internally contains a lambda that
takes _N-1_ `meta::eat`s and a trailing argument of `auto&&` type.
Only last argument is returned.

``` cpp
namespace fused {
  template <typename Tp, typename... Us>
  constexpr decltype(auto) back(Tp&& t, Us&&... us) noexcept {
    return [](meta::eat<Us>..., auto&& x) -> decltype(x) {
      return std::forward<decltype(x)>(x);
    }(std::forward<Tp>(t), std::forward<Us>(us)...);
  }
}
```

Internal lambda is called in place
by forwarding `Tp&& t` and all `Us&&... us` directly from `fused::back`.
The [trick][rsmith_trick] here is to use `fused::back`
arguments split to generate required number of `meta::eat`s.

``` cpp
// test
auto a = 1u;
auto b = 2.0;
auto c = fused::back(1u, 2.0, '3');
assert(c == '3');

auto const& x = fused::back(a, b, c);
assert(x == '3');
assert(&c == &x);

auto y = fused::back(a, b, std::make_unique<char>('4'));
assert(y != nullptr);
assert(*y == '4');
```

###nth

To access argument at any given index _N_ of function call
both techniques (skipping trailing and preceding) must be combined.

``` cpp
namespace detail {
  template<std::size_t... skip>
  struct at {
    template<typename Tp, typename... Us>
    constexpr decltype(auto) operator()(meta::eat_n<skip>...,
      Tp&& x, Us&&...) const noexcept {
      return std::forward<Tp>(x);
    }
  };
}
```

The problem is how to supply required number of values
when instantiating `detail::at` template?
It can be done with use of
[std::make_index_sequence][cppreference_integer_sequence],
default template parameter and partial specialization.

``` cpp
namespace detail {
  template <std::size_t N, typename = std::make_index_sequence<N>>
  struct at;

  template <std::size_t N, std::size_t... skip>
  struct at<N, std::index_sequence<skip...>> {
    template <typename Tp, typename... Us>
    constexpr decltype(auto) operator()(meta::eat_n<skip>..., Tp&& x,
                                        Us&&...) const noexcept {
      return std::forward<Tp>(x);
    }
  };
}
```

Instantiating `detail::at` with single template parameter `N` will
use default value for the second one, _producing_ a list of integers
in range `[0, N)`. This list allows to expand `meta::eat_n`
in function call signature for each index up to _N-1_.
Next argument `Tp&& x`, at index _N_, will be returned.
Following trailing arguments will be _swallowed_.

Generalized access of functiona argument parameter pack
can be done with `fused::nth`.

``` cpp
namespace fused {
  template <std::size_t N, typename... Ts>
  decltype(auto) nth(Ts&&... args) {
    return detail::at<N>{}(std::forward<Ts>(args)...);
  }
}
```

Template parameters of `fused::nth` are partially deduced.
Trailing variadic template `typename... Ts` can be deduced from
given arguments `Ts&&... args` when `fused::nth` is called.
Compiler cannot guess non-type template parameter `std::size_t N`,
user has to pass it explicitly.
[std::make_unique][cppreference_make_unique] or
[std::get][cppreference_get_tuple] work in similar way.


``` cpp
// test
auto a = 1u;
auto b = 2.0;
auto c = '3';

auto const& x = fused::nth<0>(a, b, c);
auto const& y = fused::nth<1>(a, b, c);
auto const& z = fused::nth<2>(a, b, c);

assert(&a == &x);
assert(&b == &y);
assert(&c == &z);

fused::nth<0>(a, b, c) = 7u;
assert(a == 7u);
```

Try this [code][code_sample]!

###References

* [cppreference]
* [n4164]
* [llvm.org][rsmith_trick]

[cppreference]:                        http://en.cppreference.com
[cppreference_parameter_pack]:         http://en.cppreference.com/w/cpp/language/parameter_pack
[cppreference_ellipsis]:               http://en.cppreference.com/w/cpp/language/variadic_arguments
[cppreference_forward]:                http://en.cppreference.com/w/cpp/utility/forward
[cppreference_unique_ptr]:             http://en.cppreference.com/w/cpp/memory/unique_ptr
[cppreference_make_unique]:            http://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique
[cppreference_get_tuple]:              http://en.cppreference.com/w/cpp/utility/tuple/get
[cppreference_integer_sequence]:       http://en.cppreference.com/w/cpp/utility/integer_sequence
[n4164]:                               http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4164.pdf
[rsmith_trick]:                        http://llvm.org/bugs/show_bug.cgi?id=13263
[code_sample]:                         http://coliru.stacked-crooked.com/a/ab5f39be452d9448
