#include "view/util.hpp"

#include <algorithm>
#include <cstring>

#include "ipfix/util.hpp"

void
load_view_value(const ViewField &view_field, fds_drec_field &drec_field, ViewValue &value)
{
    switch (view_field.data_type) {
    case DataType::Unsigned8:
        value.u8 = get_uint(drec_field);
        break;
    case DataType::Unsigned16:
        value.u16 = get_uint(drec_field);
        break;
    case DataType::Unsigned32:
        value.u32 = get_uint(drec_field);
        break;
    case DataType::Unsigned64:
        value.u64 = get_uint(drec_field);
        break;
    case DataType::Signed8:
        value.i8 = get_int(drec_field);
        break;
    case DataType::Signed16:
        value.i16 = get_int(drec_field);
        break;
    case DataType::Signed32:
        value.i32 = get_int(drec_field);
        break;
    case DataType::Signed64:
        value.i64 = get_int(drec_field);
        break;
    case DataType::String128B:
        memset(value.str, 0, 128);
        memcpy(value.str, drec_field.data, std::min<int>(drec_field.size, 128));
        break;
    case DataType::DateTime:
        value.ts_millisecs = get_datetime(drec_field);
        break;
    case DataType::MacAddress:
        memcpy(value.mac, drec_field.data, 6);
        break;
    default: assert(0);
    }
}