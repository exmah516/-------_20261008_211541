#pragma once
#include <cstdint>

constexpr std::uint32_t kHandoffInterfaceVersion = 1;
// 与PLC DUT的pack_mode=1逐字节对应，request_id必须独立最后提交。
#pragma pack(push, 1)
struct HandoffCommand
{
    std::uint8_t leg_mask = 0;
    double target_abs[2] = {}, velocity[2] = {}, acc[2] = {}, dec[2] = {}, jerk[2] = {};
    std::uint16_t cyl_clamp[4] = {}, cyl_open[4] = {}, wait_ms[2][4] = {};
    std::uint32_t request_id = 0;
};
struct HandoffStatus
{
    std::uint32_t ack_id = 0;
    std::int16_t phase = 0;
    std::uint32_t phase_seq = 0, done_id = 0;
    std::int16_t result = 0;
    std::uint32_t error_code = 0;
    std::int16_t error_axis = 0;
    std::uint32_t nc_error_code = 0;
    std::int16_t reject_reason = 0;
    std::uint8_t leg_mask = 0;
    std::uint32_t phase_ms[8] = {};
};
#pragma pack(pop)
static_assert(sizeof(HandoffCommand) == 117, "PLC换手命令布局不一致");
static_assert(sizeof(HandoffStatus) == 61, "PLC换手状态布局不一致");
