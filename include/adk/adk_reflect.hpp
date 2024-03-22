/*  
    ADK_Reflect - v0.1
    No warranty implied, use at your own risk.

    Basic reflection facilities.

    The public interface is adk::reflect, adk::reflect::internal should not be accessed
    unless you know what you're doing.
*/

#ifndef ADK_REFLECT_HPP
#define ADK_REFLECT_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>

#ifdef ADK_USE_ASSERTIONS
    #define ADK_ASSERT(condition) assert(condition)
#else
    #define ADK_ASSERT(...) ((void)0);
#endif

namespace adk::reflect::internal
{

/**
 * Compile-time string wrapper so that it can be used as a template argument.
 * This is used for member metadata right now.
 */
template<unsigned N>
struct comptime_string 
{
    char buf[N + 1]{};
    constexpr comptime_string(char const* s) 
    {
        for (unsigned i = 0; i != N; ++i) buf[i] = s[i];
    }
    constexpr operator char const*() const { return buf; }
};
template<unsigned N> comptime_string(char const (&)[N]) -> comptime_string<N - 1>;

/**
 * Helper for the FOR_EACH macros.
 */
#define ADK_INTERNAL_PARENS ()

/**
 * Helpers for the FOR_EACH macros.
 */
#define ADK_INTERNAL_EXPAND(...)                                                                            \
    ADK_INTERNAL_EXPAND4(ADK_INTERNAL_EXPAND4(                                                              \
    ADK_INTERNAL_EXPAND4(ADK_INTERNAL_EXPAND4(__VA_ARGS__))))
#define ADK_INTERNAL_EXPAND4(...)                                                                           \
    ADK_INTERNAL_EXPAND3(ADK_INTERNAL_EXPAND3(                                                              \
    ADK_INTERNAL_EXPAND3(ADK_INTERNAL_EXPAND3(__VA_ARGS__))))
#define ADK_INTERNAL_EXPAND3(...)                                                                           \
    ADK_INTERNAL_EXPAND2(ADK_INTERNAL_EXPAND2(                                                              \
    ADK_INTERNAL_EXPAND2(ADK_INTERNAL_EXPAND2(__VA_ARGS__))))
#define ADK_INTERNAL_EXPAND2(...)                                                                           \
    ADK_INTERNAL_EXPAND1(ADK_INTERNAL_EXPAND1(                                                              \
    ADK_INTERNAL_EXPAND1(ADK_INTERNAL_EXPAND1(__VA_ARGS__))))
#define ADK_INTERNAL_EXPAND1(...) __VA_ARGS__

/**
 * This set of macros iterates over variadic macro arguments and runs the passed macro with the
 * class_name argument and variadic argument.
 */
#define ADK_INTERNAL_FOR_EACH(class_name, macro, ...)                                                       \
    __VA_OPT__(ADK_INTERNAL_EXPAND(ADK_INTERNAL_FOR_EACH_HELPER(class_name, macro, __VA_ARGS__)))

#define ADK_INTERNAL_FOR_EACH_HELPER(class_name, macro, a1, ...)                                            \
    macro(class_name, a1)                                                                                   \
    __VA_OPT__(ADK_INTERNAL_FOR_EACH_AGAIN ADK_INTERNAL_PARENS (class_name, macro, __VA_ARGS__))

#define ADK_INTERNAL_FOR_EACH_AGAIN() ADK_INTERNAL_FOR_EACH_HELPER

/**
 * Literally identical to ADK_INTERNAL_FOR_EACH and similar except it puts commas between the
 * macro calls.
 */
#define ADK_INTERNAL_FOR_EACH_COMMA(class_name, macro, ...)                                                 \
    __VA_OPT__(ADK_INTERNAL_EXPAND(ADK_INTERNAL_FOR_EACH_COMMA_HELPER(class_name, macro, __VA_ARGS__)))

#define ADK_INTERNAL_FOR_EACH_COMMA_HELPER(class_name, macro, a1, ...)                                      \
    macro(class_name, a1)                                                                                   \
    __VA_OPT__(, ADK_INTERNAL_FOR_EACH_COMMA_AGAIN ADK_INTERNAL_PARENS (class_name, macro, __VA_ARGS__))                      \

#define ADK_INTERNAL_FOR_EACH_COMMA_AGAIN() ADK_INTERNAL_FOR_EACH_COMMA_HELPER

#define COMPTIME_STRING(str) adk::reflect::internal::comptime_string(str)

/**
 * Produces the type of a member descriptor giving the name of 
 * the member and the class name.
 */
#define ADK_INTERNAL_MEMBER_TYPE(class_name, member_name)                                                   \
    adk::reflect::member_descriptor<                                                                        \
        adk::reflect::class_descriptor<class_name>,                                                         \
        COMPTIME_STRING(#member_name)>                                                                      \

/**
 * Introduces the metadata for an individual class member.
 */
#define ADK_INTERNAL_REFLECT_MEMBER(class_name, member_name)                                                \
    template <> struct adk::reflect::member_descriptor<                                                     \
        adk::reflect::class_descriptor<class_name>,                                                         \
        COMPTIME_STRING(#member_name)>                                                                      \
    {                                                                                                       \
        using type = decltype(class_name::member_name);                                                     \
        static constexpr std::string_view name = #member_name;                                              \
        static constexpr std::size_t offset = offsetof(class_name, member_name);                            \
    };


/**
 * Given an object, a function, and a member descriptor, calls the function with
 * a reference to the type described in the member descriptor contained in the
 * passed object.
 */
template <typename MemberDescriptor, typename ClassName, typename Func>
void call_member_data(ClassName* object, Func func)
{
    using member_type = typename MemberDescriptor::type;
    member_type* value = reinterpret_cast<member_type*>(static_cast<char*>(static_cast<void*>(object)) + MemberDescriptor::offset);
    func(MemberDescriptor::name, *value);
}

/**
 * Tuple foreach struct that needs specialized.
 */
template <typename Tuple>
struct call_for_each_member;

/**
 * Calls a function with an object for each argument in a tuple type
 */
template <typename... Args>
struct call_for_each_member<std::tuple<Args...>> 
{
    template <typename ClassName, typename Func>
    constexpr void operator()(ClassName* object, Func func)
    {
        (call_member_data<Args>(object, func),...);
    }
};

} // namespace adk::reflect::internal

namespace adk::reflect
{

/**
 * Metadata for a class, must be specialized.
 */
template <typename ClassName>
struct class_descriptor;

/**
 * Metadata for a member of a class, must be specialized.
 */
template <typename ClassDescriptor, internal::comptime_string name>
struct member_descriptor;

/**
 * SFINAE function that checks if class_descriptor is specialized for a
 * certain class.
 */
template <typename ClassName, typename = void>
struct is_class_reflected : std::false_type {};

template <typename ClassName>
struct is_class_reflected<ClassName, decltype(class_descriptor<ClassName>(), void())> : std::true_type {};



/**
 * Introduces compile-time reflection metadata of the provided class and public members.
 */
#define ADK_REFLECT_CLASS(class_name, ...)                                                                  \
    template <> struct adk::reflect::class_descriptor<class_name>                                           \
    {                                                                                                       \
        using members =                                                                                     \
            std::tuple<ADK_INTERNAL_FOR_EACH_COMMA(class_name, ADK_INTERNAL_MEMBER_TYPE, __VA_ARGS__)>;     \
        static constexpr std::size_t member_count = std::tuple_size_v<members>;                             \
        static constexpr std::string_view name = #class_name;                                               \
    };                                                                                                      \
    ADK_INTERNAL_FOR_EACH(class_name, ADK_INTERNAL_REFLECT_MEMBER, __VA_ARGS__);
    
    /**
     * Calls passed function with a reference to each member in passed
     * object.
     */
    template <typename ClassName, typename Func>
    void for_each_member(ClassName* object, Func func)
    {
        static_assert(is_class_reflected<ClassName>::value, "Passed class has no reflection metadata!");
        using class_descriptor = class_descriptor<ClassName>;
        internal::call_for_each_member<typename class_descriptor::members>()(object, func);
    }

} // namespace adk::reflect

#endif
