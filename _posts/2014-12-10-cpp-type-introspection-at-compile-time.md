---
layout: post
title: "C++ type introspection at compile-time"
date: 2014-12-10 12:28:06 +0100
comments: true
---

###Template metaprogramming

At first glance template metaprogramming may look very difficult,
but in fact it is rather challenging than complicated.
Sometimes TMP is abused to write unmaintainable code.
On the other hand simple utilities can make your project much more
flexible, generic and elegant. So what should we have in our toolbox?

###apply

Let's start with something really simple: template metafunction `apply`
that takes another metafunction and applies
[parameter pack][cppreference_parameter_pack] to it.

``` cpp
namespace meta {
  template <template <typename...> class MetaFunction, typename... Args>
  struct apply {
    using type = typename MetaFunction<Args...>::type;
  };

  template <template <typename...> class MetaFunction, typename... Args>
  using apply_t = typename apply<MetaFunction, Args...>::type;
}
```

What is a template metafunction?
It is simply a template that translates one
or more parameters into types or values.
What can be an argument of such metafunction?
There are three possibilities:

* non-type template parameter (like `v`
  in [std::integral_constant][cppreference_integral_constant])
* type template parameter (like `T`
  in [std::add_pointer][cppreference_add_pointer])
* template template parameter (like `MetaFunction` in example above)

Metafunction _returns_ via named member types or values.
According to common practice return member type
should be named `typename T::type` or `T::value` if it is non-type.
Since c++14 standard library has helper
[alias templates][cppreference_alias_templates]
(see [std::remove\_reference\_t][cppreference_remove_reference_t])
with _\_t_ suffix that allows to skip `typename` and `::type`.
In similar way [variable templates][cppreference_variable_templates]
can be used to skip `::value`. There is no limitation
for number of _returns_, but it is a good practice to have exactly one,
still there are useful counterexamples like
[std::iterator_traits][cppreference_iterator_traits].

###void_t

This year at [cppcon] Walter E. Brown had a great talk
titled _Modern Template Metaprogramming: A Compendium_
[Part I][cppcon_walter_e_brown_compendium_part_i]
& [Part II][cppcon_walter_e_brown_compendium_part_ii].
He introduced `void_t` - simple but powerfull metaprogramming tool
(official proposal to the standard is [N3911]).

``` cpp
namespace meta {
  namespace detail {
    template <typename...>
    struct voider {
      using type = void;
    };
  }

  template <typename... Ts>
  using void_t = apply_t<detail::voider, Ts...>;
}
```

Usage of `void_t` does not directly rely on member
alias `using type = void`. The trick is in template argument list.
It can contain only well-formed types, otherwise entire
`void_t` is ill-formed. This property can be detected
with [SFINAE][cppreference_sfinae]

###Type introspection

Type introspection is a possibility to query properties of a given type.
In c++11 and c++14 it can be done at compile-time with
[type_traits][cppreference_type_traits] or at runtime
with [RTTI][cppreference_rtti]. It is expected that in c++17
there will be more advanced facility called [Concepts Lite][N4205].

####has_member

`void_t` allows to write type trait for user-defined properties.
Starting point is a struct `has_member` which exploits SFINAE.

``` cpp
namespace meta {
  template <template <typename> class, typename, typename = meta::void_t<>>
  struct has_member : std::false_type {};

  template <template <typename> class Member, typename Tp>
  struct has_member<Member, Tp, meta::void_t<Member<Tp>>> : std::true_type {};
}
```

`has_member` takes three parameters.
First is _template template_ metafunction that works like a _getter_.
Second is a type to introspect.
Third parameter (unnamed parameter with default value)
is a technical trick that allows selecting implementation.
There are two cases of `has_member`:

* basic case, potentially handling any type, returns `value == false`,
* refined case, handling only well-formed `void_t<Member<Tp>>`, returns `value == true`.

Refined case will be prefered as it is
[more specialized][cppreference_partial_specialization],
so basic case will be instantiated only when
`void_t<Member<Tp>>` is ill-formed.

####ValueType

Following example presents how to write a trait that checks
whether given type `Tp` has a member `typename Tp::value_type`.

``` cpp
template <typename Tp>
using ValueType = typename Tp::value_type;

template <typename Tp>
using has_value_type = meta::has_member<ValueType, Tp>;
```

Metafunction _getter_ `ValueType` tries to extract `value_type` from `Tp`.
If it will succeed `void_t<Member<Tp>>` is well-formed.
Refined case of `has_member` is valid and its implementation is instantiated.

``` cpp
// test
static_assert(has_value_type<std::vector<int>>::value, "");
static_assert(!has_value_type<std::pair<int, int>>::value, "");
static_assert(!has_value_type<int>::value, "");
```

####Public data members

Usage of `has_member` is not only limited to querying for member types.
Next example shows how to write a trait that checks if a given type
has accessible data member.

``` cpp
namespace detail {
  template <typename Tp>
  using InspectDataProperty = decltype(std::declval<Tp>().data);
}

template <typename Tp>
using has_data_property = meta::has_member<detail::InspectDataProperty, Tp>;
```

All the work is done by `InspectDataProperty` alias for [decltype][cppreference_decltype].
Expression inside `decltype` parenthesis is in unevaluated context.
This means that it is never executed. Compiler only _simulates_ expression
to obtain its type. To _create_ an instance in unevaluated context
simply constructor `Tp{}` can be called, but this works only
when `Tp` is default constructible. More generic solution is to use
[std::declval][cppreference_declval] instead.

Finally `InspectDataProperty` is equivalent to the type of the `Tp` member
called `data`. When `InspectDataProperty` is able to obtain `data` then `has_member`
template argument is well-formed and trait return value is `true`.
Otherwise, when there is no `data` in a given type, `has_member` returns `false`.

``` cpp
// test
static_assert(has_data_property<TypeWithPublicData>::value, "");
static_assert(!has_data_property<std::vector<int>>::value, "");
static_assert(!has_data_property<TypeWithPrivateData>::value, "");
static_assert(!has_data_property<int>::value, "");
```

####Member functions

Not only types or data members but also member functions
can be detected by `has_member`.
To check if any generic container supports memory reservation
`has_reserve` trait can be used.

``` cpp
namespace detail {
  template <typename Tp>
  using InspectReserve = decltype(
      std::declval<Tp>().reserve(std::declval<typename Tp::size_type>()));
}

template <typename Tp>
using has_reserve = meta::has_member<detail::InspectReserve, Tp>;
```

`InspectReserve` helper _simulates_ in `decltype` unevaluated context
call of `reserve` member function.
Similar to [std::vector][cppreference_vector_reserve] `reserve` should take
`size_type` number of elements. Like in previous example,
when helper is well-formed then `has_reserve` trait returns `true`.

``` cpp
// test
static_assert(has_reserve<std::vector<int>>::value, "");
static_assert(!has_reserve<std::array<int, 10>>::value, "");
```

####Operators

Finally `has_member` can be used to the check presence of operators,
since operators can be implemented as a member functions.
This time unevaluated context in helper `InspectCopyAssignable`
_simulates_ assigning `Tp const&` to `Tp&&`.

``` cpp
namespace detail {
  template <typename Tp>
  using InspectCopyAssignable =
      decltype(std::declval<Tp>() = std::declval<Tp const&>());
}

template <typename Tp>
using is_copy_assignable = meta::has_member<detail::InspectCopyAssignable, Tp>;
```

It works for both copyable (returns `true`)
and non-copyable types (return `false`);

``` cpp
// test
static_assert(is_copy_assignable<std::vector<int>>::value, "");
static_assert(is_copy_assignable<std::pair<int, int>>::value, "");
static_assert(!is_copy_assignable<std::unique_ptr<int>>::value, "");
```

But it fails for `int`.

``` cpp
// static_assert(is_copy_assignable<int>::value, ""); //fails
```

I leave it to the readers as a puzzle how to fix `InspectCopyAssignable`
so that it work also for fundamental types.
Here is a small hint... take a look at
[reference collapsing][cppreference_reference_collapsing] and this build
error message:

```
error: using xvalue (rvalue reference) as lvalue
using int_t = decltype(std::declval<int>() = std::declval<int const&>());
                                           ^
```

Try this [code][code_sample].

###References

* [N3911]
* [cppreference]
* [cppcon]
* [WG21]

[cppreference]:                        http://en.cppreference.com
[cppreference_parameter_pack]:         http://en.cppreference.com/w/cpp/language/parameter_pack
[cppreference_integral_constant]:      http://en.cppreference.com/w/cpp/types/integral_constant
[cppreference_add_pointer]:            http://en.cppreference.com/w/cpp/types/add_pointer
[cppreference_alias_templates]:        http://en.cppreference.com/w/cpp/language/type_alias
[cppreference_iterator_traits]:        http://en.cppreference.com/w/cpp/iterator/iterator_traits
[cppreference_remove_reference_t]:     http://en.cppreference.com/w/cpp/types/remove_reference
[cppreference_variable_templates]:     http://en.cppreference.com/w/cpp/language/variable_template
[cppreference_sfinae]:                 http://en.cppreference.com/w/cpp/language/sfinae
[cppreference_type_traits]:            http://en.cppreference.com/w/cpp/header/type_traits
[cppreference_rtti]:                   http://en.cppreference.com/w/cpp/language/typeid
[cppreference_partial_specialization]: http://en.cppreference.com/w/cpp/language/partial_specialization
[cppreference_decltype]:               http://en.cppreference.com/w/cpp/language/decltype
[cppreference_declval]:                http://en.cppreference.com/w/cpp/utility/declval
[cppreference_vector_reserve]:         http://en.cppreference.com/w/cpp/container/vector/reserve
[cppreference_reference_collapsing]:   http://en.cppreference.com/w/cpp/language/reference

[cppcon]: http://cppcon.org/
[cppcon_walter_e_brown_compendium_part_i]:  http://www.youtube.com/watch?v=Am2is2QCvxY&list=UUMlGfpWw-RUdWX_JbLCukXg
[cppcon_walter_e_brown_compendium_part_ii]: http://www.youtube.com/watch?v=a0FliKwcwXE&list=UUMlGfpWw-RUdWX_JbLCukXg

[code_sample]: http://goo.gl/XFTkeH

[WG21]:  http://www.open-std.org/jtc1/sc22/wg21
[N3911]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3911.pdf
[N4205]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4205.pdf
