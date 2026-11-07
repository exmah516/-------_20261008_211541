#pragma once

#include "Vrep/robot.h"
#include "Vrep/CoppeliaSim.h"
#include <fstream>
#include <iostream>

_simObjectHandle_Type* Body;
_simObjectHandle_Type* JOint[9];
_simSignalHandle_Type* EulerAngle[3];
robot bot;

//数据处理环节
void Usr_Main(double array[], int len)
{
	bot.robot_spin(array, len);
}

//获取机械臂模型
void Usr_ConfigSimulation()
{
	Body = CoppeliaSim->Add_Object("bot", OTHER_OBJECT, { SIM_ORIENTATION | CLIENT_RO, SIM_VELOCITY | CLIENT_RO });
	JOint[sj1] = CoppeliaSim->Add_Object("sj1", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[sj2] = CoppeliaSim->Add_Object("sj2", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[sj3] = CoppeliaSim->Add_Object("sj3", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[sj4] = CoppeliaSim->Add_Object("sj4", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[sj5] = CoppeliaSim->Add_Object("sj5", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[sj6] = CoppeliaSim->Add_Object("sj6", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[sj7] = CoppeliaSim->Add_Object("sj7", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[xj1] = CoppeliaSim->Add_Object("xj1", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
	JOint[xj2] = CoppeliaSim->Add_Object("xj2", JOINT, { SIM_VELOCITY | CLIENT_RW, SIM_POSITION | CLIENT_RW, SIM_FORCE | CLIENT_RO });
}

//发送数据给vrep的模型
void Usr_SendToSimulation()
{
	for (int i = 0; i < 9; i++)
	{
		if (i == 0 || i == 1 || i == 5)
			JOint[i]->obj_Target.angle_f = bot.robot_data.joint_data.out[i] * 0.001;
		else
			JOint[i]->obj_Target.angle_f = bot.robot_data.joint_data.out[i] / 57.296;
		//cout << bot.robot_data.joint_data.out[i] << endl;
	}
}

int simulation(const string& s)
{
	CoppeliaSim_Client* hClient = &CoppeliaSim_Client::getInstance();

	std::cout << "[CoppeliaSim Client] Connecting to server.. \n";
	while (!hClient->Start("127.0.0.1", 5000, 1, true)) {};//id,端口号,通讯周期（ms）大于等于50（控制周期）的话就会一顿一顿的因为来不及平滑处理,true是同步控制，如果用速度控制的话必须要用同步模式
	std::cout << "[CoppeliaSim Client] Successfully connected to server, configuring...\n";
	Usr_ConfigSimulation();
	std::cout << "[CoppeliaSim Client] Configure done, simulation is ready ! \n";

	fstream f7;
	//f7.open(".\\plan\\velupdown.txt", ios::in);//速度控制
	f7.open(s, ios::in);//位置控制
	double after[10];

	while (f7 >> after[0] >> after[1] >> after[2] >> after[3] >> after[4] >> after[5] >> after[6] >> after[7] >> after[8] >> after[9])
	{
		if (hClient->Is_Connected())
			hClient->ComWithServer();//连接成功的话就允许交换数据

		//Usr_ReadFromSimulation();//从vrep的模型读数据
		Usr_Main(after, 10);//数据处理
		Usr_SendToSimulation();//发送数据给vrep的模型
	};
	f7.close();

	return 0;
}


void Vrep_move(double target[])
{
	CoppeliaSim_Client* hClient = &CoppeliaSim_Client::getInstance();
	while (!hClient->Start("127.0.0.1", 5000, 1, true)) {};//id,端口号,通讯周期（ms）大于等于50（控制周期）的话就会一顿一顿的因为来不及平滑处理,true是同步控制，如果用速度控制的话必须要用同步模式
	Usr_ConfigSimulation();

	double after[10];
	for (int i = 0; i < 10; i++)
	{
		if (i == 0)
			after[i] = 0;
		else
			after[i] = target[i - 1];
	}

	if (hClient->Is_Connected())		hClient->ComWithServer();//连接成功的话就允许交换数据
	Usr_Main(after, 10);//数据处理
	Usr_SendToSimulation();//发送数据给vrep的模型
}
