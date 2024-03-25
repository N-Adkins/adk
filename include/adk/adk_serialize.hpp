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

#include <memory>
#include <string>
#include <vector>
#include "adk_reflect.hpp"

namespace adk::serialize::internal
{

} // namespace adk::serialize::internal

namespace adk::serialize
{

struct serialized_field
{
    bool root = false;
    std::string name;
    virtual ~serialized_field() = default;
};

struct serialized_trivial : public serialized_field
{
    std::string value;
};

struct serialized_structure : public serialized_field
{
    std::string identifier;
    std::vector<std::unique_ptr<serialized_field>> fields;
};

template <typename T>
std::unique_ptr<serialized_field> serialize(const std::string& name, const T& data)
{
    static_assert(reflect::is_class_reflected<T>::value, "Attempting to serialize type that has no reflection data");
    using descriptor = adk::reflect::class_descriptor<T>;
    auto *structure = new serialized_structure();
    structure->name = name;
    structure->identifier = std::string(descriptor::name);
    adk::reflect::for_each_class_member(data, [&structure](auto name, const auto& value){
        structure->fields.push_back(serialize(std::string(name), value));
    });
    return std::unique_ptr<serialized_field>(structure);
}

template <>
inline std::unique_ptr<serialized_field> serialize<int>(const std::string& name, const int& data)
{
    auto* trivial = new serialized_trivial();
    trivial->name = name;
    trivial->value = std::to_string(data);
    return std::unique_ptr<serialized_field>(trivial);
}

} // namespace adk::serialize

#endif
