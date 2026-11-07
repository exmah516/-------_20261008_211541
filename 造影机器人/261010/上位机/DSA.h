#pragma once
#include <iostream>
#include <Eigen\Dense>
#include "screw.h"
#include <fstream>
#include <cstdlib>
#include <ctime>
#include "ADS/ADSComm1.h"
#include <json.h>
#include <vector>
#include <string>
#include <queue>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <limits>
#include "copleycan.h"
#include <thread>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shell32.lib")

using namespace Json;
using namespace std;
using namespace Eigen;
using namespace std::chrono;

#ifdef PI
#undef PI
#endif
#define PI 3.1415926535897932384626433832795
#define Rad 57.295779513082320876798154819203
#define aver_num 20//抑制NDI抖动
#define max_path 5000//发送给ads的最大轨迹点长度
#define I6 MatrixXd::Identity(6, 6)
#define I7 MatrixXd::Identity(7, 7)
typedef Eigen::Matrix<double, 6, 1> Vector6d;

class homo
{
public:
	//用来放四元数和位置
	double a[7];
};

class DSA
{
	friend CADSComm;

public:

	CopleyCan cc;
	CanFrame frame;//接收到的can信号

	long long m = 0;
	int number = 0;
	double vh = 0;
	double vr = 0;
	double v6 = 0;
	int 手柄把上的按钮 = 0;
	int 手柄盘上的按钮 = 0;
	USHORT control_state = 0;//运动模式标志位，0：刚开机不动，1：手柄微调，2：运输位，3:暂停运输，4：语音控制
	USHORT control_state_read = 0;
	double 启停阶段速度限制的分母 = 30.0;

	std::ofstream File;
	std::chrono::system_clock::time_point start, current_time;
	double 关节编码器值[10] = { 0 };
	double 关节编码器值f[10] = { 0 };


	double u1, u2, u3, u4, u5, u6, u7;//当前关节点
	double u3f = 0;
	double u7f = 0;

	double d1 = 0 / Rad;
	double d2 = 0/ Rad;

	CADSComm a;
	double base_dt = 0.001;//步长

	//目标点
	double begin1 = 0.0;
	double begin2 = 0.0;
	double end1 = 0.0;
	double end2 = 0.0;

	double h = atan2(sin(2 / Rad) * cos(0), cos(2 / Rad)) * Rad;
	double r = atan2(sin(0) * sin(2 / Rad) * cos(h / Rad), cos(2 / Rad)) * Rad;
	double p6 = 0; // 相对补偿轨迹的关节6手动偏移，单位mm
	Eigen::Matrix<double, 6, 2> xi_lower_comp;
	Eigen::MatrixXd xi_upper_comp = Eigen::MatrixXd(6, 7);
	Matrix4d C_field_comp, C_control_comp;
	bool compensation_initialized = false; // 现场回零完成后才建立初始参考
	bool compensation_ready = false; // 平滑到达补偿初始点后才接手柄输入
	bool initial_move_active = false;
	double initial_start_joint[10] = { 0 }, initial_goal_joint[10] = { 0 };
	double initial_move_seconds = 2;
	std::chrono::steady_clock::time_point initial_move_begin;
	USHORT collision_state = 0; // PLC：0正常，1碰撞保持，2缓恢复
	USHORT collision_state_f = 0;
	bool collision_task_latched = false; // 保留初始化/碰撞时的姿态任务，继续用原逆解对齐
	bool collision_wait_neutral = false; // 保护后须先回中立，再接受新输入
	double d1f = 0;
	double d2f = 0;

	double target_joint[10] = { 0 };

	//计算到哪个点了
	int times = 1;

	int path_number;//轨迹点的数量

	Matrix4d target;
	double x, z1, z2;

	//判断走完了没有
	bool isfinished;

	//***************************** 误差补偿 *****************************
	double up_1_vir, up_2_vir, up_3_vir, up_4_vir, up_5_vir, up_6_vir, up_7_vir;
;
	int iterations = 0;

	//记录第一次补偿之前的误差
	double dis_err, theta_err;

	DSA();

	void 设置现场补偿参数();
	void 建立补偿初始参考();
	void 补偿初始点过渡();

	~DSA();

	//计算矩阵伪逆
	MatrixXd pinv(Eigen::MatrixXd A);

	void Open_exe(string s);
	int Close_exe(string s);

	//微分运动学
	void 微分运动学(Matrix4d Target);

	//指数积正运动学
	Matrix4d FK_up();
	Matrix4d FK_down();

	//解析解
	void Solution();
	void Solution_redundant4_change();//论文版本
	void Solution_0522();//0522_真实的机械臂_最真实

	void 计算d1与d2与target与u4(bool 是否限制速度);//下笔关节一速度限制了
	void test_231205_做运动学参数标定();

	//伴随矩阵
	MatrixXd Ad(Matrix4d T);

	//空间雅可比矩阵
	MatrixXd Jacobian();

	//末端偏差
	VectorXd y(Matrix4d Ta, Matrix4d T);

	//标定雅可比矩阵
	MatrixXd Cali_Jacobian();

	//用自定义的符号分割文本
	vector<string> split(const string& s, const string& seperator);

	//NDI测俩远心点在基座坐标系下的位姿
	void NDI(Matrix4d& Ta, Matrix4d& T);

	//理论关节误差辨识验证
	void identify_theory(Matrix4d& Ta, Matrix4d& T);

	//无关变量的正运动学
	Matrix4d Forward_exponential_up_err();//用来误差补偿迭代的

	//无关变量的标定雅可比矩阵
	MatrixXd Cali_Jacobian_err();

	//关节误差值
	double joint_err_value1_5();
	double joint_err_value2_5();
	double joint_err_value3_5();
	double joint_err_value4_5();
	double joint_err_value5_5();
	double joint_err_value6_5();
	double joint_err_value7_5();

	//时间转字符串 年月日时分秒
	std::string format_time_to_string(std::chrono::system_clock::time_point& time_point);

	//时间转字符串 日时分秒毫秒
	std::string format_time_to_string_简易(std::chrono::system_clock::time_point& time_point);

	void 运动至目标体位(double end1, double end2);

	int 手柄微调运动(HANDLE g_hMutex);

	int 手柄信息获取(HANDLE g_hMutex, int 通信周期, double rate_v);

	void Solution_Redundant_输入旋量坐标(Eigen::MatrixXd xi);

};