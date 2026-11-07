#pragma once
#include <string>

using namespace std;

enum
{
	yaw = 0,
	pitch,
	roll
};
enum
{
	sj1 = 0,
	sj2,
	sj3,
	sj4,
	sj5,
	sj6,
	sj7,
	xj1,
	xj2
};
typedef struct JointData//用来存放一次迭代的所有关节参数
{
	float curr_vel[9];
	float curr_pos[9];
	float curr_torque[9];
	float tar_vel[9];
	float tar_pos[9];
	float tar_torque[9];

	float out[9];
}JointDataTypedef;


typedef struct robotData
{
	float roll, pitch, yaw;
	float ox, oy, oz;
	JointDataTypedef joint_data;
}robotDataTypedef;

class  robot
{
private:
	/* data */
public:
	robot(/* args */);
	~robot();
	void robot_spin(double array[], int len);
	robotDataTypedef robot_data;
};

int CountLines(string filename);//获取文件的总行数
string ReadLine(string filename, int line);//获取文件某一行的内容