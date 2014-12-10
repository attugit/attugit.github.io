#include <utility>

namespace meta {
template <template <typename...> class MetaFunction, typename... Args>
struct apply {
  using type = typename MetaFunction<Args...>::type;
};

template <template <typename...> class MetaFunction, typename... Args>
using apply_t = typename apply<MetaFunction, Args...>::type;

namespace detail {
  template <typename...>
  struct voider {
    using type = void;
  };
}

template <typename... Ts>
using void_t = apply_t<detail::voider, Ts...>;

template <template <typename> class, typename, typename = meta::void_t<>>
struct has_member : std::false_type {};

template <template <typename> class Member, typename Tp>
struct has_member<Member, Tp, meta::void_t<Member<Tp>>> : std::true_type {};
}

template <typename Tp>
using ValueType = typename Tp::value_type;

template <typename Tp>
using has_value_type = meta::has_member<ValueType, Tp>;

#include <vector>

// test
static_assert(has_value_type<std::vector<int>>::value, "");
static_assert(!has_value_type<std::pair<int, int>>::value, "");
static_assert(!has_value_type<int>::value, "");

namespace detail {
template <typename Tp>
using InspectDataProperty = decltype(std::declval<Tp>().data);
}

template <typename Tp>
using has_data_property = meta::has_member<detail::InspectDataProperty, Tp>;

struct TypeWithPublicData {
  int data;
};

struct TypeWithPrivateData {
private:
  int data;
};

// test
static_assert(has_data_property<TypeWithPublicData>::value, "");
static_assert(!has_data_property<std::vector<int>>::value, "");
static_assert(!has_data_property<TypeWithPrivateData>::value, "");
static_assert(!has_data_property<int>::value, "");

namespace detail {
template <typename Tp>
using InspectReserve = decltype(
    std::declval<Tp>().reserve(std::declval<typename Tp::size_type>()));
}

template <typename Tp>
using has_reserve = meta::has_member<detail::InspectReserve, Tp>;

#include <array>
// test
static_assert(has_reserve<std::vector<int>>::value, "");
static_assert(!has_reserve<std::array<int, 10>>::value, "");

namespace detail {
template <typename Tp>
using InspectCopyAssignable =
    decltype(std::declval<Tp>() = std::declval<Tp const&>());
}

template <typename Tp>
using is_copy_assignable = meta::has_member<detail::InspectCopyAssignable, Tp>;

#include <memory>
// test
static_assert(is_copy_assignable<std::vector<int>>::value, "");
static_assert(is_copy_assignable<std::pair<int, int>>::value, "");
static_assert(!is_copy_assignable<std::unique_ptr<int>>::value, "");

// static_assert(is_copy_assignable<int>::value, ""); //fails
// using int_t = decltype(std::declval<int>() = std::declval<int const&>());

int main() { return 0; }
