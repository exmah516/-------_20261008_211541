#pragma once
#include <cmath>
#include <cstdint>

// 仅统计连续手柄跟随段的实际净位移；换手、无效快照和坐标重建只更新基准。
class HandleNetDistance
{
public:
    void sample(double relative_mm, double origin_mm, std::uint64_t sequence,
        bool valid, bool following)
    {
        const double actual_mm = relative_mm + origin_mm;
        valid = valid && std::isfinite(actual_mm) && std::isfinite(origin_mm);
        if (valid && sample_valid_ && sequence <= sequence_) return;
        if (valid && sample_valid_ && following_ && following && origin_mm == origin_mm_)
            net_mm_ += actual_mm_ - actual_mm;
        sample_valid_ = valid;
        actual_mm_ = actual_mm;
        origin_mm_ = origin_mm;
        sequence_ = sequence;
        following_ = valid && following;
    }

    // 本拍触发换手后立即停计，完成夹爪交接后才恢复下一段。
    void set_following(bool following) { following_ = sample_valid_ && following; }
    double net_mm() const { return net_mm_; }

private:
    double net_mm_ = 0.0;
    double actual_mm_ = 0.0;
    double origin_mm_ = 0.0;
    std::uint64_t sequence_ = 0;
    bool sample_valid_ = false;
    bool following_ = false;
};
