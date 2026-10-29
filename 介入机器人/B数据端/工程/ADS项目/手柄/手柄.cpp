#include "Handle.h"
#include "remote_handle_bridge.h"
#include <stdio.h>

// 允许短暂网络抖动；真正断流超过3秒才进入既有重连流程。
static constexpr ULONGLONG kRemoteMaxAgeMs = 3000;

Handle::Handle(DWORD serial)
	: serial_number_(serial)
{
	clear_cache();
}

bool Handle::init()
{
	return init(serial_number_);
}

bool Handle::init(DWORD serial)
{
	serial_number_ = serial;
	if (iID1 >= 0)
	{
		return true;
	}

	// 主端送来有效采样才算接入；主循环每秒重试时不输出失败日志。
	remote_handle_bridge::Sample sample;
	if (!remote_handle_bridge::get_sample(remote_handle_bridge::slot_of(serial_number_), sample, kRemoteMaxAgeMs))
	{
		return false;
	}
	iID1 = 0;
	printf(u8"已接入远程手柄（物理 SN %lu）。\n", static_cast<unsigned long>(serial_number_));
	return true;
}

bool Handle::poll()
{
	if (iID1 < 0)
	{
		return false;
	}

	remote_handle_bridge::Sample sample;
	if (!remote_handle_bridge::get_sample(remote_handle_bridge::slot_of(serial_number_), sample, kRemoteMaxAgeMs))
	{
		return false;
	}
	if (last_sample_rx_ms_ != 0 && sample.rx_ms > last_sample_rx_ms_ &&
		sample.rx_ms - last_sample_rx_ms_ > 250) rebase_requested_ = true;
	last_sample_rx_ms_ = sample.rx_ms;
	for (int i = 0; i < 2; ++i)
	{
		fVels2[i] = GetTickCount64() - sample.rx_ms > 250 ? 0.0 : static_cast<double>(sample.vels[i]);
		fJoints2[i] = static_cast<double>(sample.joints[i]);
		encoders2[i] = static_cast<long>(sample.encoders[i]);
	}
	buttons2 = sample.buttons;
	objData.buttons2 = buttons2;
	objData.encoders2[0] = encoders2[0];
	objData.encoders2[1] = encoders2[1];
	return true;
}

void Handle::clear_cache()
{
	last_sample_rx_ms_ = 0;
	rebase_requested_ = false;
	objData.buttons2 = 0;
	objData.encoders2[0] = 0;
	objData.encoders2[1] = 0;
	fVels2[0] = 0.0;
	fVels2[1] = 0.0;
	fJoints2[0] = 0.0;
	fJoints2[1] = 0.0;
	buttons2 = 0;
	encoders2[0] = 0;
	encoders2[1] = 0;
}

void Handle::close()
{
	iID1 = -1;
	clear_cache();
	// 关闭时撤销经 Zenoh force Topic 回传主端的力目标，重复关闭也保持清零。
	remote_handle_bridge::set_output(remote_handle_bridge::slot_of(serial_number_), remote_handle_bridge::Output());
}

void Handle::setforce(double F, double N)
{
	setforce_axis(F, 0, N);
}

void Handle::setforce_axis(double F, int axis, double N)
{
	if (iID1 < 0)
	{
		return;
	}

	remote_handle_bridge::Output out;
	out.enable = true;
	out.axis = axis < 0 ? 0 : (axis > 2 ? 2 : axis);
	out.force = F;
	out.torque = N;
	remote_handle_bridge::set_output(remote_handle_bridge::slot_of(serial_number_), out);
}
