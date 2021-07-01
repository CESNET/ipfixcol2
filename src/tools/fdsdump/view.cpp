#include "view.hpp"

ViewValue *
get_value_by_name(ViewDefinition &view_definition, uint8_t *values, const std::string &name)
{
    ViewValue *value = reinterpret_cast<ViewValue *>(values + view_definition.keys_size);
    for (const auto &field : view_definition.value_fields) {
        if (field.name == name) {
            return value;
        }
        advance_value_ptr(value, field.size);
    }
    return nullptr;
}