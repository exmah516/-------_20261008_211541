#pragma once

#include "zenoh_protocol.h"
#include <cmath>
#include <limits>

namespace vi_protocol
{
    namespace detail
    {
        inline bool array(const msgpack::object& o, std::size_t count)
        { return o.type == msgpack::type::ARRAY && o.via.array.size == count; }
        inline bool uint(const msgpack::object& o, std::uint64_t maximum)
        { return o.type == msgpack::type::POSITIVE_INTEGER && o.via.u64 <= maximum; }
        inline bool sint(const msgpack::object& o, std::int64_t minimum, std::int64_t maximum)
        {
            return (o.type == msgpack::type::POSITIVE_INTEGER && o.via.u64 <= static_cast<std::uint64_t>(maximum)) ||
                (o.type == msgpack::type::NEGATIVE_INTEGER && o.via.i64 >= minimum);
        }
        inline bool i32(const msgpack::object& o)
        { return sint(o, (std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()); }
        inline bool boolean(const msgpack::object& o) { return o.type == msgpack::type::BOOLEAN; }
        inline bool f32(const msgpack::object& o) { return o.type == msgpack::type::FLOAT32 && std::isfinite(o.via.f64); }
        inline bool f64(const msgpack::object& o) { return o.type == msgpack::type::FLOAT64 && std::isfinite(o.via.f64); }
        inline bool text(const msgpack::object& o, std::size_t maximum = 4096)
        { return o.type == msgpack::type::STR && o.via.str.size <= maximum; }
        inline bool bin(const msgpack::object& o, std::size_t size)
        { return o.type == msgpack::type::BIN && o.via.bin.size == size; }
        inline bool optional_bin(const msgpack::object& o, std::size_t size)
        { return bin(o, 0) || bin(o, size); }
        inline bool direction(const msgpack::object& o) { return sint(o, -1, 1); }
        inline bool version(const msgpack::object& o) { return uint(o, kVersion) && o.via.u64 == kVersion; }
        inline bool identity(const msgpack::object* p)
        { return version(p[0]) && uint(p[1], UINT64_MAX) && p[1].via.u64 > 0 && uint(p[2], UINT64_MAX) && p[2].via.u64 > 0; }
    }

    inline bool validate_object(const msgpack::object& o, const HandleFrame*)
    {
        if (!detail::array(o, kHandleFields)) return false;
        const auto p = o.via.array.ptr;
        if (!detail::identity(p) || !detail::uint(p[3], UINT64_MAX)) return false;
        for (const int offset : { 4, 12 })
        {
            if (!detail::uint(p[offset], UINT8_MAX) || !detail::boolean(p[offset + 1]) ||
                !detail::i32(p[offset + 2]) || !detail::i32(p[offset + 3])) return false;
            for (int i = offset + 4; i < offset + 8; ++i) if (!detail::f32(p[i])) return false;
        }
        return detail::direction(p[20]) && detail::direction(p[21]) && detail::direction(p[22]) && detail::optional_bin(p[23], 16);
    }

    inline bool validate_object(const msgpack::object& o, const CommandFrame*)
    {
        if (!detail::array(o, kCommandFields)) return false;
        const auto p = o.via.array.ptr;
        if (!detail::version(p[0]) || !detail::uint(p[1], UINT64_MAX) || !detail::uint(p[2], UINT64_MAX) || p[2].via.u64 == 0 ||
            !detail::uint(p[3], UINT64_MAX) || !detail::uint(p[4], INT32_MAX) || !detail::uint(p[5], 17) ||
            !detail::i32(p[6]) || !detail::i32(p[7]) || !detail::f64(p[8]) || !detail::f64(p[9]) || !detail::text(p[10]) ||
            !detail::optional_bin(p[11], 16) || !detail::optional_bin(p[12], 16) ||
            !detail::optional_bin(p[13], 32) || !detail::optional_bin(p[14], 16)) return false;
        if (p[5].via.u64 == static_cast<std::uint8_t>(Command::Hello))
            return p[1].via.u64 == 0 && detail::bin(p[11], 16) && detail::bin(p[12], 0) && detail::bin(p[13], 0) && detail::bin(p[14], 0);
        if (p[5].via.u64 == static_cast<std::uint8_t>(Command::Authenticate))
            return p[1].via.u64 != 0 && detail::bin(p[11], 16) && detail::bin(p[12], 16) && detail::bin(p[13], 32) && detail::bin(p[14], 0);
        return p[1].via.u64 != 0 && detail::bin(p[11], 0) && detail::bin(p[12], 0) && detail::bin(p[13], 0);
    }

    inline bool validate_object(const msgpack::object& o, const CommandResult*)
    {
        if (!detail::array(o, kCommandResultFields)) return false;
        const auto p = o.via.array.ptr;
        return detail::uint(p[0], INT32_MAX) && p[0].via.u64 > 0 && detail::uint(p[1], 1) && detail::text(p[2]);
    }

    inline bool validate_object(const msgpack::object& o, const StateFrame*)
    {
        if (!detail::array(o, kStateFields)) return false;
        const auto p = o.via.array.ptr;
        if (!detail::version(p[0]) || !detail::uint(p[1], UINT64_MAX) || !detail::uint(p[2], UINT64_MAX) || p[2].via.u64 == 0 ||
            !detail::uint(p[3], 2) || !detail::optional_bin(p[4], 16) || !detail::uint(p[5], UINT64_MAX) || !detail::uint(p[6], UINT32_MAX)) return false;
        if (p[3].via.u64 != 0 && (p[1].via.u64 == 0 || !detail::bin(p[4], 16))) return false;
        for (int i = 7; i <= 10; ++i) if (!detail::i32(p[i])) return false;
        for (int i = 11; i <= 14; ++i) if (!detail::uint(p[i], UINT16_MAX)) return false;
        if (!detail::uint(p[15], UINT8_MAX)) return false;
        for (int i = 16; i <= 18; ++i) if (!detail::direction(p[i])) return false;
        for (int i = 19; i <= 38; ++i) if (!detail::f32(p[i])) return false;
        if (!detail::boolean(p[39]) || !detail::f64(p[40]) || !detail::f64(p[41]) || !detail::uint(p[42], 3) ||
            !detail::uint(p[43], INT32_MAX) || !detail::uint(p[44], 3) || !detail::uint(p[45], INT32_MAX) ||
            p[46].type != msgpack::type::ARRAY || p[46].via.array.size > kMaxResults || !detail::text(p[47]) || !detail::optional_bin(p[48], 16)) return false;
        for (std::uint32_t i = 0; i < p[46].via.array.size; ++i)
            if (!validate_object(p[46].via.array.ptr[i], static_cast<const CommandResult*>(nullptr))) return false;
        return true;
    }

    inline bool validate_object(const msgpack::object& o, const ForceFrame*)
    {
        if (!detail::array(o, kForceFields)) return false;
        const auto p = o.via.array.ptr;
        return detail::identity(p) && detail::uint(p[3], UINT64_MAX) &&
            detail::boolean(p[4]) && detail::uint(p[5], 2) && detail::f32(p[6]) && detail::f32(p[7]) &&
            detail::boolean(p[8]) && detail::uint(p[9], 2) && detail::f32(p[10]) && detail::f32(p[11]) && detail::optional_bin(p[12], 16);
    }

    template <class T> inline bool decode(const std::vector<std::uint8_t>& bytes, T& value)
    {
        if (bytes.empty() || bytes.size() > 65536) return false;
        try
        {
            std::size_t offset = 0;
            const auto object = msgpack::unpack(reinterpret_cast<const char*>(bytes.data()), bytes.size(), offset);
            if (offset != bytes.size() || !validate_object(object.get(), static_cast<const T*>(nullptr))) return false;
            object.get().convert(value);
            return true;
        }
        catch (const std::exception&) { return false; }
    }

    template <class T> inline std::vector<std::uint8_t> encode(const T& frame)
    {
        msgpack::sbuffer buffer;
        msgpack::pack(buffer, frame);
        return std::vector<std::uint8_t>(buffer.data(), buffer.data() + buffer.size());
    }
}
