/*  
    ADK_Reflect - v0.1
    No warranty implied, use at your own risk.

    Basic reflection facilities.

    The public interface is adk::reflect, adk::reflect::internal should not be accessed
    unless you know what you're doing.
*/

#ifndef ADK_REFLECT_HPP
#define ADK_REFLECT_HPP

#include <cassert>
#include <cstddef>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>

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
 * Produces the type of an enum class item descriptor giving the name of 
 * the enum class and the item name.
 */
#define ADK_INTERNAL_ENUM_CLASS_ITEM_TYPE(enum_name, item_name)                                             \
    adk::reflect::enum_class_item_descriptor<                                                               \
        adk::reflect::enum_class_descriptor<enum_name>,                                                     \
        enum_name::item_name>                                                                               \

/**
 * Introduces the metadata for an individual class member.
 */
#define ADK_INTERNAL_REFLECT_MEMBER(class_name, member_name)                                                \
    template <> struct adk::reflect::member_descriptor<                                                     \
        adk::reflect::class_descriptor<class_name>,                                                         \
        COMPTIME_STRING(#member_name)>                                                                      \
    {                                                                                                       \
        using type = std::decay_t<decltype(class_name::member_name)>;                                       \
        static constexpr std::string_view name = #member_name;                                              \
        static constexpr std::size_t offset = offsetof(class_name, member_name);                            \
    };

/**
 * Introduces the metadata for an individual enum class member.
 */
#define ADK_INTERNAL_REFLECT_ENUM_CLASS_ITEM(enum_name, item_name)                                          \
template <> struct adk::reflect::enum_class_item_descriptor<                                                \
        adk::reflect::enum_class_descriptor<enum_name>,                                                     \
        enum_name::item_name>                                                                               \
    {                                                                                                       \
        using type = std::underlying_type<enum_name>::type;                                                 \
        static constexpr std::string_view truncated_name = #item_name;                                      \
        static constexpr std::string_view name = #enum_name "::" #item_name;                                      \
        static constexpr enum_name enum_value = enum_name::item_name;                                       \
        static constexpr type underlying_value = static_cast<type>(enum_name::item_name);                   \
    };

/**
 * Given an object, a function, and a member descriptor, calls the function with
 * a reference to the type described in the member descriptor contained in the
 * passed object.
 */
template <typename ItemDescriptor, typename Func>
constexpr void call_item_data(Func func)
{
    func(ItemDescriptor::name, ItemDescriptor::enum_value, ItemDescriptor::underlying_value);
}

/**
 * Tuple foreach struct that needs specialized.
 */
template <typename Tuple>
struct call_for_each_item;

/**
 * Calls a function for each item in an enum
 */
template <typename... Args>
struct call_for_each_item<std::tuple<Args...>> 
{
    template <typename Func>
    constexpr void operator()(Func func)
    {
        (call_item_data<Args>(func),...);
    }
};

template <typename EnumName, typename ItemDescriptor>
void init_enum_class_map_pair(std::unordered_map<EnumName, std::string_view>& map)
{
    map[ItemDescriptor::enum_value] = ItemDescriptor::name;
}

/**
 * Tuple foreach struct that needs specialized.
 */
template <typename Tuple>
struct init_enum_class_map_for_each;

/**
 * Calls a function with an object for each argument in a tuple type
 */
template <typename... Args>
struct init_enum_class_map_for_each<std::tuple<Args...>> 
{
    template <typename EnumName>
    constexpr void operator()(std::unordered_map<EnumName, std::string_view>& map)
    {
        (init_enum_class_map_pair<EnumName, Args>(map),...);
    }
};

template <typename EnumName, typename Tuple>
std::unordered_map<EnumName, std::string_view> generate_enum_class_map()
{
    std::unordered_map<EnumName, std::string_view> map;
    init_enum_class_map_for_each<Tuple>()(map); 
    return map;
}

/**
 * Given an object, a function, and a member descriptor, calls the function with
 * a reference to the type described in the member descriptor contained in the
 * passed object.
 */
template <typename MemberDescriptor, typename ClassName, typename Func>
void call_object_member_data(const ClassName& object, Func func)
{
    using member_type = typename MemberDescriptor::type;
    const member_type* value = reinterpret_cast<const member_type*>(static_cast<const char*>(static_cast<const void*>(std::addressof(object))) + MemberDescriptor::offset);
    func(MemberDescriptor::name, *value);
}

/**
 * Tuple foreach struct that needs specialized.
 */
template <typename Tuple>
struct call_for_each_object_member;

/**
 * Calls a function with an object for each argument in a tuple type
 */
template <typename... Args>
struct call_for_each_object_member<std::tuple<Args...>> 
{
    template <typename ClassName, typename Func>
    constexpr void operator()(const ClassName& object, Func func)
    {
        (call_object_member_data<Args>(object, func),...);
    }
};

/**
 * Given a reflectable type and a function, the function is called with
 * the type's name, offset, and a filler value so the caller can obtain the actual type.
 */
template <typename MemberDescriptor, typename Func>
void call_member_data(Func func)
{
    using type = typename MemberDescriptor::type;
    type obj = type();
    func(MemberDescriptor::name, MemberDescriptor::offset, obj);
}

/**
 * Tuple foreach struct that needs specialized.
 */
template <typename Tuple>
struct call_for_each_member;

/**
 * Calls a function for each argument in a tuple type
 */
template <typename... Args>
struct call_for_each_member<std::tuple<Args...>> 
{
    template <typename Func>
    constexpr void operator()(Func func)
    {
        (call_member_data<Args>(func),...);
    }
};

template <typename T>
concept is_complete = requires(T) 
{
    sizeof(T);
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

template <typename T>
concept reflected_class = internal::is_complete<class_descriptor<T>>;

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
    template <reflected_class ClassName, typename Func>
    void for_each_object_member(const ClassName& object, Func func)
    {
        using class_descriptor = class_descriptor<ClassName>;
        internal::call_for_each_object_member<typename class_descriptor::members>()(object, func);
    }

    template <reflected_class ClassName, typename Func>
    void for_each_class_member(Func func)
    {
        using class_descriptor = class_descriptor<ClassName>;
        internal::call_for_each_member<typename class_descriptor::members>()(func);
    }

    /**
     * Metadata for an enum class, must be specialized.
     */
    template <typename EnumName>
    struct enum_class_descriptor;

    /**
     * Metadata for an item of an enum class, must be specialized.
     */
    template <typename EnumDescriptor, EnumDescriptor::type item>
    struct enum_class_item_descriptor;

    template <typename T>
    concept reflected_enum_class = internal::is_complete<enum_class_descriptor<T>>;

/**
 * Introduces compile-time reflection metadata of the provided enum class and items.
 */
#define ADK_REFLECT_ENUM_CLASS(enum_name, ...)                                                              \
    template <> struct adk::reflect::enum_class_descriptor<enum_name>                                       \
    {                                                                                                       \
        using type = enum_name;                                                                             \
        using items = std::tuple<                                                                           \
            ADK_INTERNAL_FOR_EACH_COMMA(                                                                    \
            enum_name,                                                                                      \
            ADK_INTERNAL_ENUM_CLASS_ITEM_TYPE,                                                              \
            __VA_ARGS__)>;                                                                                  \
        static constexpr std::size_t item_count = std::tuple_size_v<items>;                                 \
        static constexpr std::string_view name = #enum_name;                                                \
        inline static const auto map =                                                                      \
            adk::reflect::internal::generate_enum_class_map<enum_name, items>();                            \
    };                                                                                                      \
    ADK_INTERNAL_FOR_EACH(enum_name, ADK_INTERNAL_REFLECT_ENUM_CLASS_ITEM, __VA_ARGS__);

/**
 * Helper function that both declares an enum with a name and items
 * and produces reflection metadata for it. This can only be used
 * for trivial enum classes where values are not set manually.
 */
#define ADK_DECLARE_ENUM_CLASS(enum_name, ...)                                                              \
    enum class enum_name { __VA_ARGS__ };                                                                   \
    ADK_REFLECT_ENUM_CLASS(enum_name, __VA_ARGS__)

    /**
     * Converts an enum class value to a string showing the full enum type name and
     * the value name
     */
    template <reflected_enum_class EnumName>
    std::string_view enum_class_to_string(EnumName value)
    {
        using enum_descriptor = enum_class_descriptor<EnumName>;
        return enum_descriptor::map.at(value);        
    }

    /**
     * Calls passed function with data for each item in an enum
     */
    template <reflected_enum_class EnumName, typename Func>
    constexpr void for_each_enum_class_item(Func func)
    {
        using enum_descriptor = enum_class_descriptor<EnumName>;
        internal::call_for_each_item<typename enum_descriptor::items>()(func);
    }
    
} // namespace adk::reflect

#undef ADK_ASSERT

#endif
