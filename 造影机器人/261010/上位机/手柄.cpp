#include "DSA.h"
#include "可视化.hpp"

DSA dsa;
HANDLE g_hMutex = NULL;     //互斥量

DWORD WINAPI 手柄线程(LPVOID lpParameter)
{
	int err = dsa.cc.Open(0, COPLEYCAN_BITRATE_500000);
	if (err)
	{
		printf("Error 0x%04x opening CAN port\n", err);
		return 1;
	}
	CanCardInfo info;
	err = dsa.cc.ReadCardInfo(&info);
	if (err)
		printf("Error 0x%04x reading information about card\n", err);
	while (1)
	{
		int temp = dsa.手柄信息获取(g_hMutex, 200, 8000.0);//通信周期，速度比例
		if (temp == 0)
			break;
		else if (temp == 1)
			continue;
	}
	return 0;
}

DWORD WINAPI Vrep线程(LPVOID lpParameter)
{
	CoppeliaSim_Client* hClient = &CoppeliaSim_Client::getInstance();
	while (!hClient->Start("127.0.0.1", 5000, 1, true)) {};
	Usr_ConfigSimulation();
	while (1)
	{
		if (hClient->Is_Connected())	hClient->ComWithServer();
		Usr_Main(dsa.关节编码器值, 10);
		Usr_SendToSimulation();
	}
	return 0;
}

int main()
{
	g_hMutex = CreateMutex(NULL, FALSE, NULL);
	CreateThread(NULL, 0, 手柄线程, NULL, 0, NULL);
	//CreateThread(NULL, 0, Vrep线程, NULL, 0, NULL);

	while (1)
	{
		int err = 1;

		// 先读 PLC 状态，再决定是否接受盘上的新运动指令。
		if (dsa.手柄盘上的按钮 != 0 && !dsa.a.ADSRead((char*)"G.collision_state", sizeof(dsa.collision_state), &dsa.collision_state))
			continue;

		// 保护期间盘上按钮也不能另起运动；统一回到手柄流程处理锁存任务。
		if ((dsa.control_state == 1 && !dsa.compensation_ready) || dsa.collision_state != 0 || dsa.collision_wait_neutral)
			err = dsa.手柄微调运动(g_hMutex);
		else
		switch (dsa.手柄盘上的按钮)
		{
			//手柄微调
		case 0:
			err = dsa.手柄微调运动(g_hMutex);
			break;

		case 57:
			//从开机变为手柄微调  左上角的两个按钮同时按下

			if (dsa.control_state == 0)
			{
				dsa.control_state = 1;
				for (int i = 0; i < 60; i++)
					dsa.a.ADSWrite((char*)"G.control_state", sizeof(dsa.control_state), &dsa.control_state);
							}
			//从手柄微调变为语音控制，结束时恢复手柄微调 
			else
			{
				dsa.control_state = 4;
				dsa.a.ADSWrite((char*)"G.control_state", sizeof(dsa.control_state), &dsa.control_state);

				dsa.运动至目标体位(0, 0);

				dsa.control_state = 1;
				dsa.a.ADSWrite((char*)"G.control_state", sizeof(dsa.control_state), &dsa.control_state);
			}
			break;

		case 48:
			//从手柄微调变为运输位或者从运输位变为运输暂停 右上角的两个按钮同时按下
			if (2 == dsa.control_state)
				dsa.control_state = 3;
			else 
				dsa.control_state = 2;
			for (int i = 0; i < 60; i++)
				dsa.a.ADSWrite((char*)"G.control_state", sizeof(dsa.control_state), &dsa.control_state);
			break;
		}

		if (!err)			break;
	}
	return 0;
}
