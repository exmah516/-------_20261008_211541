#pragma once
#include "async_csv_writer.h"
#include "手柄/remote_handle_bridge.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <mmsystem.h>

// 只读接收缓存；独立100Hz采样和既有异步写盘均不进入控制计算。
class HandleVelocityRecorder
{
public:
    struct Row
    {
        ULONGLONG elapsed_ms = 0, rx_ms[2] = {};
        bool valid[2] = {};
        double received[4] = {}, joint_rate[4] = {};
    };
    bool start(const std::filesystem::path& path)
    {
        if (!writer_.start(path.wstring(),
            "elapsed_ms,rx582_ms,valid582,rx587_ms,valid587,"
            "sdk582_0,sdk582_1,sdk587_0,sdk587_1,"
            "joint582_0_per_s,joint582_1_per_s,joint587_0_per_s,joint587_1_per_s\n", write_row)) return false;
        start_ms_ = GetTickCount64();
        stop_ = false;
        sampler_ = std::thread([this] {
            const bool precise = timeBeginPeriod(1) == TIMERR_NOERROR;
            auto next = std::chrono::steady_clock::now();
            while (!stop_.load())
            {
                const Row row = sample(GetTickCount64());
                (void)writer_.try_enqueue(row);
                next += std::chrono::milliseconds(10);
                std::this_thread::sleep_until(next);
            }
            if (precise) timeEndPeriod(1);
        });
        return true;
    }
    ~HandleVelocityRecorder() { stop(); }
    void stop()
    {
        stop_ = true;
        if (!sampler_.joinable()) return;
        sampler_.join();
        writer_.stop();
        for (int slot = 0; slot < 2; ++slot) for (int joint = 0; joint < 2; ++joint)
        {
            const int i = slot * 2 + joint;
            std::cout << "[HANDLE-SPEED] SN=" << (slot == 0 ? 582 : 587) << " joint=" << joint
                << " max_abs_received_sdk=" << received_max_[i]
                << " max_abs_joint_per_s=" << joint_max_[i] << '\n';
        }
        std::cout << "[HANDLE-SPEED] dropped=" << writer_.dropped_count()
            << " writer_error=" << writer_.last_error() << std::endl;
    }
    Row sample(ULONGLONG now)
    {
        Row row;
        row.elapsed_ms = now - start_ms_;
        for (int slot = 0; slot < 2; ++slot)
        {
            remote_handle_bridge::Sample input;
            row.valid[slot] = remote_handle_bridge::get_sample(slot, input, 5000);
            row.rx_ms[slot] = input.rx_ms;
            for (int joint = 0; joint < 2; ++joint)
            {
                const int i = slot * 2 + joint;
                row.received[i] = row.valid[slot] ? input.vels[joint] : std::numeric_limits<double>::quiet_NaN();
                row.joint_rate[i] = std::numeric_limits<double>::quiet_NaN();
                if (row.valid[slot])
                {
                    received_max_[i] = (std::max)(received_max_[i], std::abs(row.received[i]));
                    if (previous_[slot].rx_ms != 0 && input.rx_ms > previous_[slot].rx_ms)
                    {
                        // SDK未注明getEncVel单位，同时记录相邻接收关节量/接收间隔，供位移公式直接使用。
                        row.joint_rate[i] = (input.joints[joint] - previous_[slot].joints[joint]) * 1000.0 /
                            static_cast<double>(input.rx_ms - previous_[slot].rx_ms);
                        joint_max_[i] = (std::max)(joint_max_[i], std::abs(row.joint_rate[i]));
                    }
                }
            }
            if (row.valid[slot]) previous_[slot] = input;
        }
        return row;
    }
    const std::array<double, 4>& received_max() const { return received_max_; }
    const std::array<double, 4>& joint_max() const { return joint_max_; }
private:
    static bool write_row(std::FILE* fp, const Row& r)
    {
        return std::fprintf(fp, "%llu,%llu,%d,%llu,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
            r.elapsed_ms, r.rx_ms[0], r.valid[0] ? 1 : 0, r.rx_ms[1], r.valid[1] ? 1 : 0,
            r.received[0], r.received[1], r.received[2], r.received[3],
            r.joint_rate[0], r.joint_rate[1], r.joint_rate[2], r.joint_rate[3]) >= 0;
    }
    AsyncCsvWriter<Row> writer_;
    std::thread sampler_;
    std::atomic<bool> stop_{ true };
    ULONGLONG start_ms_ = 0;
    remote_handle_bridge::Sample previous_[2];
    std::array<double, 4> received_max_{}, joint_max_{};
};
