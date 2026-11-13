// 文件职责说明：
// 1) 实现 ADS 符号定义与 PLC I/O 封装。
// 2) 通信服务启动后，将运行期专项读写排入后台线程；启动前保留兼容直连。
// 3) 不承载控制状态机，只提供数据访问与低频状态缓存能力。
#include "plc_io.h"

#include "ads_communication.h"

#include <array>
#include <cstring>
#include <iostream>
#include <string>

namespace AdsSymbol
{
	const char* refer = "G.refer";
	const char* act_pos = "G.Act_pos";
	const char* init_pos = "G.init_pos";
	const char* leftlimit = "G.leftlimit";
	const char* act_pos_from_left = "G.act_pos_from_left";
	const char* refer_from_left = "G.refer_from_left";
	const char* cylinder1_value = "G.cyl_cmd[1]";
	const char* cylinder2_value = "G.cyl_cmd[2]";
	const char* cylinder3_value = "G.cyl_cmd[3]";
	const char* cylinder4_value = "G.cyl_cmd[4]";
	const char* cylinder5_cmd = "G.cylinder5_cmd";
	const char* cylinder5_press_req = "G.cylinder5_press_req";
	const char* cylinder5_value = "G.cylinder5_value";
	const char* self_check_done = "G.self_check_done";
	const char* startup_loading_ready = "G.startup_loading_ready";
	const char* handle_reinit_req = "G.handle_reinit_req";
	const char* estop_hold_req = "G.estop_hold_req";
	const char* ft_1_value = "G.ft_1_value";
	const char* fn_1_value = "G.fn_1_value";
	const char* fn_2_value = "G.fn_2_value";
	const char* ft_2_value = "G.ft_2_value";
	const char* axis4_fwd_req = "G.axis4_fwd_req";
	const char* axis4_rev_req = "G.axis4_rev_req";
	const char* inject_push_req = "G.inject_push_req";
	const char* inject_pull_req = "G.inject_pull_req";
	const char* axis4_manual_busy = "G.axis4_manual_busy";
	const char* axis4_manual_done = "G.axis4_manual_done";
	const char* axis4_manual_error = "G.axis4_manual_error";
	const char* axis4_manual_error_id = "G.axis4_manual_error_id";
	const char* gen_state = "G.gen_state";
	const char* host_session_id = "G.host_session_id";
	const char* host_heartbeat_sequence = "G.host_heartbeat_sequence";
	const char* host_recover_req = "G.host_recover_req";
	const char* host_comm_timeout = "G.host_comm_timeout";
	const char* app_name = "TwinCAT_SystemInfoVarList._AppInfo.AppName";


}

namespace
{
	bool ads_read(AppContext& ctx, const char* symbol, unsigned long length, void* output)
	{
		if (ctx.ads_service != nullptr)
		{
			return ctx.ads_service->read(symbol, length, output);
		}
		return ctx.ads != nullptr && ctx.ads->ADSRead(symbol, length, output);
	}

	bool ads_write(AppContext& ctx, const char* symbol, unsigned long length, const void* input)
	{
		if (ctx.ads_service != nullptr)
		{
			return ctx.ads_service->write(symbol, length, input);
		}
		return ctx.ads != nullptr &&
			ctx.ads->ADSWrite(symbol, length, const_cast<void*>(input));
	}

	bool ads_read_sum(
		AppContext& ctx,
		const char* const* symbols,
		const unsigned long* lengths,
		void* const* outputs,
		unsigned long count)
	{
		if (ctx.ads_service != nullptr)
		{
			return ctx.ads_service->read_sum(symbols, lengths, outputs, count);
		}
		return ctx.ads != nullptr && ctx.ads->ADSReadSum(symbols, lengths, outputs, count);
	}

	bool ads_write_sum(
		AppContext& ctx,
		const char* const* symbols,
		const unsigned long* lengths,
		const void* const* inputs,
		unsigned long count)
	{
		if (ctx.ads_service != nullptr)
		{
			return ctx.ads_service->write_sum(symbols, lengths, inputs, count);
		}
		return ctx.ads != nullptr && ctx.ads->ADSWriteSum(symbols, lengths, inputs, count);
	}

	std::string ads_error_text(const AppContext& ctx)
	{
		if (ctx.ads == nullptr) return "ADS 对象不可用。\n";
		const std::string error = ctx.ads->GetLastErrorCopy();
		return error.empty() ? "后台 ADS 请求失败。\n" : error;
	}


}

namespace plc_io
{
	bool read_plc_state(AppContext& ctx)
	{
		if (ctx.ads_service != nullptr)
		{
			AdsFastSnapshot snapshot{};
			if (!ctx.ads_service->latest_snapshot(snapshot) || !snapshot.position_valid)
			{
				return false;
			}
			copy_positions(snapshot.act_pos_rel, ctx.plc_act_pos, 7);
			copy_positions(snapshot.act_pos_from_left, ctx.plc_act_pos_from_left, 7);
			copy_positions(snapshot.init_pos, ctx.plc_init_pos, 7);
			copy_positions(snapshot.leftlimit, ctx.plc_leftlimit, 7);
			ctx.plc_act_velocity_mm_s[0] = snapshot.axis1_act_velocity_mm_s;
			ctx.plc_act_velocity_mm_s[2] = snapshot.axis3_act_velocity_mm_s;
			ctx.plc_act_velocity_mm_s[4] = snapshot.axis5_act_velocity_mm_s;
			ctx.plc_act_velocity_mm_s[5] = snapshot.axis6_act_velocity_mm_s;
			ctx.plc_snapshot_qpc_ticks = snapshot.qpc_ticks;
			ctx.plc_snapshot_sequence = snapshot.attempt_sequence;
			return true;
		}

		LARGE_INTEGER qpc_before{};
		LARGE_INTEGER qpc_after{};
		QueryPerformanceCounter(&qpc_before);
		const char* symbols[] = {
			AdsSymbol::act_pos,
			AdsSymbol::init_pos,
			AdsSymbol::leftlimit,
			"G.axis[1].NcToPlc.ActVelo",
			"G.axis[3].NcToPlc.ActVelo",
			"G.axis[5].NcToPlc.ActVelo",
			"G.axis[6].NcToPlc.ActVelo"
		};
		const unsigned long lengths[] = {
			static_cast<unsigned long>(sizeof(double) * 7),
			static_cast<unsigned long>(sizeof(double) * 7),
			static_cast<unsigned long>(sizeof(double) * 7),
			sizeof(double), sizeof(double), sizeof(double), sizeof(double)
		};
		void* outputs[] = {
			ctx.plc_act_pos,
			ctx.plc_init_pos,
			ctx.plc_leftlimit,
			&ctx.plc_act_velocity_mm_s[0],
			&ctx.plc_act_velocity_mm_s[2],
			&ctx.plc_act_velocity_mm_s[4],
			&ctx.plc_act_velocity_mm_s[5]
		};
		const bool success = ads_read_sum(ctx, symbols, lengths, outputs, 7);
		QueryPerformanceCounter(&qpc_after);
		if (success)
		{
			ctx.plc_snapshot_qpc_ticks = qpc_before.QuadPart + (qpc_after.QuadPart - qpc_before.QuadPart) / 2;
			++ctx.plc_snapshot_sequence;
		}
		return success;
	}

	bool write_refer(AppContext& ctx)
	{
		// 将当前 pos[7] 写回 PLC 参考位数组 G.refer。
		return ads_write(ctx, AdsSymbol::refer, sizeof(double) * 7, ctx.pos);
	}

	bool write_startup_loading_ready(AppContext& ctx, bool ready)
	{
		return ads_write(ctx, AdsSymbol::startup_loading_ready, sizeof(ready), &ready);
	}

	void load_pos_from_actual(AppContext& ctx)
	{
		// 以当前实际位置为基线重建一帧 refer，避免直接使用旧参考引发跳变。
		copy_positions(ctx.plc_act_pos, ctx.pos, 7);
	}

	bool write_axis4_manual_requests(AppContext& ctx, bool forward_req, bool reverse_req)
	{
		// Axis4 点动请求由 PLC 侧状态机执行，本处仅负责写入请求位。
		const char* symbols[] = { AdsSymbol::axis4_fwd_req, AdsSymbol::axis4_rev_req };
		const unsigned long lengths[] = { sizeof(forward_req), sizeof(reverse_req) };
		const void* inputs[] = { &forward_req, &reverse_req };
		return ads_write_sum(ctx, symbols, lengths, inputs, 2);
	}

	void clear_plc_reinit_req(AppContext& ctx)
	{
		bool clear_val = false;
		(void)ads_write(ctx, AdsSymbol::handle_reinit_req, sizeof(clear_val), &clear_val);
	}
}
