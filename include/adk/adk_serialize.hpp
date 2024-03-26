/*  
    ADK_Serialize - v0.1
    No warranty implied, use at your own risk.
    
    Requires adk_reflect.hpp to be in the same directory.

    Serialization for builtin types and structures with reflection metadata
    from adk_reflect.hpp.

    STD containers and types are not supported yet.

    The public interface is adk::serialize, adk::serialize::internal should not be accessed
    unless you know what you're doing.
*/

#ifndef ADK_SERIALIZE_HPP
#define ADK_SERIALIZE_HPP

#include <cassert>
#include <charconv>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include "adk_reflect.hpp"

#ifdef ADK_USE_ASSERTIONS
    #define ADK_ASSERT(condition) assert(condition)
#else
    #define ADK_ASSERT(...) ((void)0);
#endif

namespace adk::serialize::internal
{

struct serialized_field
{
    bool root = false;
    std::string name;

    virtual ~serialized_field() = default;
    virtual void print(std::ostream& os) const = 0;

    friend std::ostream& operator<<(std::ostream& os, const serialized_field* field)
    {
        field->print(os);
        return os; 
    }
};

struct serialized_trivial : public serialized_field
{
    std::string value;
    void print(std::ostream& os) const override
    {
        os << "\"" << name << "\":\"" << value << "\"";
    }
};

struct serialized_structure : public serialized_field
{
    std::string identifier;
    std::unordered_map<std::string, std::unique_ptr<serialized_field>> fields;
    void print(std::ostream& os) const override
    {
        if (!root) {
            os << "\"" << name << "\":";
        }
        os << "{";
        os << "\"identifier\":\"" << identifier << "\",";
        const auto last = fields.size() - 1;
        const std::size_t i = 0;
        for (const auto& [_, field] : fields) {
            field->print(os);
            if (i != last) {
                os << ",";
            }
        }
        os << "}";
    }
};


template <typename T>
inline std::unique_ptr<serialized_field> serialize_basic(const T& data, const std::string& name="", bool root=true)
{
    auto* trivial = new serialized_trivial();
    trivial->name = name;
    trivial->value = std::to_string(data);
    trivial->root = root;
    return std::unique_ptr<serialized_field>(trivial);
}

} // namespace adk::serialize::internal

namespace adk::serialize
{

// Explicitly add std container types that are added here
template <typename T>
concept serializeable = reflect::reflected_class<T> || std::is_fundamental_v<T> 
    || std::is_same_v<T, std::string>; 

template <serializeable T>
std::unique_ptr<internal::serialized_field> serialize(const T& data, const std::string& name = "", bool root = true)
{
    using descriptor = reflect::class_descriptor<T>;
    auto *structure = new internal::serialized_structure();
    structure->name = name;
    structure->identifier = std::string(descriptor::name);
    structure->root = root;
    adk::reflect::for_each_object_member(data, [&structure](auto name, const auto& value){
        const auto alloc_name = std::string(name);
        structure->fields[alloc_name] = std::move(serialize(value, alloc_name, false));
    });
    return std::unique_ptr<internal::serialized_field>(structure);
}

#define ADK_BASIC_SERIAL(type)                                                                                \
    template <>                                                                                             \
    inline std::unique_ptr<internal::serialized_field> serialize(const type& data,                          \
            const std::string& name, bool root)                                                             \
    {                                                                                                       \
        return internal::serialize_basic(data, name, root);                                                 \
    }

ADK_BASIC_SERIAL(std::uint64_t)
ADK_BASIC_SERIAL(std::uint32_t)
ADK_BASIC_SERIAL(std::uint16_t)
ADK_BASIC_SERIAL(std::uint8_t)
ADK_BASIC_SERIAL(std::int64_t)
ADK_BASIC_SERIAL(std::int32_t)
ADK_BASIC_SERIAL(std::int16_t)
ADK_BASIC_SERIAL(std::int8_t)
ADK_BASIC_SERIAL(float)
ADK_BASIC_SERIAL(double)

#undef ADK_BASIC_SERIAL

// std::string
template <>
inline std::unique_ptr<internal::serialized_field> serialize(const std::string& data, const std::string& name, bool root)
{
    auto* trivial = new internal::serialized_trivial();
    trivial->name = name;
    trivial->value = data;
    trivial->root = root;
    return std::unique_ptr<internal::serialized_field>(trivial);
}

// char, when done with the ADK_BASIC_TYPE macro it stores the ascii value as a string so
// we have to specialize it manually
template <>
inline std::unique_ptr<internal::serialized_field> serialize(const char& data, const std::string& name, bool root)
{
    auto* trivial = new internal::serialized_trivial();
    trivial->name = name;
    trivial->value = std::string(1, data);
    trivial->root = root;
    return std::unique_ptr<internal::serialized_field>(trivial);
}

template <serializeable T>
inline T deserialize(internal::serialized_field* field)
{
    internal::serialized_structure* structure = static_cast<internal::serialized_structure*>(field);

    T object;
    char* ptr = static_cast<char*>(static_cast<void*>(&object));
    adk::reflect::for_each_class_member<T>([ptr, structure](auto name, auto offset, auto type_value){
        using type = std::decay_t<decltype(type_value)>;
        const auto alloc_name = std::string(name);
        if (!structure->fields.contains(alloc_name)) {
            return;
        }
        type* casted = reinterpret_cast<type*>(ptr + offset);
        *casted = deserialize<type>(structure->fields[alloc_name].get());
    });

    return object;
}

#define ADK_BASIC_DESERIAL(type)                                                                            \
    template <>                                                                                             \
    inline type deserialize(internal::serialized_field* field)                                              \
    {                                                                                                       \
        internal::serialized_trivial* trivial = static_cast<internal::serialized_trivial*>(field);          \
        type value;                                                                                         \
        std::from_chars(trivial->value.data(), trivial->value.data() + trivial->value.size(), value);       \
        return value;                                                                                       \
    }


ADK_BASIC_DESERIAL(std::uint64_t)
ADK_BASIC_DESERIAL(std::uint32_t)
ADK_BASIC_DESERIAL(std::uint16_t)
ADK_BASIC_DESERIAL(std::uint8_t)
ADK_BASIC_DESERIAL(std::int64_t)
ADK_BASIC_DESERIAL(std::int32_t)
ADK_BASIC_DESERIAL(std::int16_t)
ADK_BASIC_DESERIAL(std::int8_t)
ADK_BASIC_DESERIAL(float)
ADK_BASIC_DESERIAL(double)

#undef ADK_BASIC_DESERIAL

// char
template <>
inline char deserialize(internal::serialized_field* field)
{
    internal::serialized_trivial* trivial = static_cast<internal::serialized_trivial*>(field);
    return trivial->value[0];
}

// std::string
template <>
inline std::string deserialize(internal::serialized_field* field)
{
    internal::serialized_trivial* trivial = static_cast<internal::serialized_trivial*>(field);
    return trivial->value;
}

} // namespace adk::serialize

#undef ADK_ASSERT

#endif
