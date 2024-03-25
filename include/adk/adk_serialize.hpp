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

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "adk_reflect.hpp"

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
    std::vector<std::unique_ptr<serialized_field>> fields;
    void print(std::ostream& os) const override
    {
        if (!root) {
            os << "\"" << name << "\":";
        }
        os << "{";
        os << "\"identifier\":\"" << identifier << "\",";
        const auto last = fields.size() - 1;
        for (std::size_t i = 0; i < fields.size(); i++) {
            fields[i]->print(os);
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

template <typename T>
std::unique_ptr<internal::serialized_field> serialize(const T& data, const std::string& name = "", bool root = true)
{
    static_assert(reflect::is_class_reflected<T>::value, "Attempting to serialize type that has no reflection data");
    using descriptor = adk::reflect::class_descriptor<T>;
    auto *structure = new internal::serialized_structure();
    structure->name = name;
    structure->identifier = std::string(descriptor::name);
    structure->root = root;
    adk::reflect::for_each_class_member(data, [&structure](auto name, const auto& value){
        structure->fields.push_back(serialize(value, std::string(name), false));
    });
    return std::unique_ptr<internal::serialized_field>(structure);
}

#define ADK_BASIC_TYPE(type)                                                                                \
    template <>                                                                                             \
    inline std::unique_ptr<internal::serialized_field> serialize(const type& data,                          \
            const std::string& name, bool root)                                                             \
    {                                                                                                       \
        return internal::serialize_basic(data, name, root);                                                 \
    }

ADK_BASIC_TYPE(std::uint64_t)
ADK_BASIC_TYPE(std::uint32_t)
ADK_BASIC_TYPE(std::uint16_t)
ADK_BASIC_TYPE(std::uint8_t)
ADK_BASIC_TYPE(std::int64_t)
ADK_BASIC_TYPE(std::int32_t)
ADK_BASIC_TYPE(std::int16_t)
ADK_BASIC_TYPE(std::int8_t)
ADK_BASIC_TYPE(float)
ADK_BASIC_TYPE(double)

#undef ADK_BASIC_TYPE

template <>
inline std::unique_ptr<internal::serialized_field> serialize<std::string>(const std::string& data, const std::string& name, bool root)
{
    auto* trivial = new internal::serialized_trivial();
    trivial->name = name;
    trivial->value = data;
    trivial->root = root;
    return std::unique_ptr<internal::serialized_field>(trivial);
}

template <>
inline std::unique_ptr<internal::serialized_field> serialize(const char& data, const std::string& name, bool root)
{
    auto* trivial = new internal::serialized_trivial();
    trivial->name = name;
    trivial->value = std::string(1, data);
    trivial->root = root;
    return std::unique_ptr<internal::serialized_field>(trivial);
}

} // namespace adk::serialize

#endif
