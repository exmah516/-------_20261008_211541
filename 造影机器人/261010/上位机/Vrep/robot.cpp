#include "robot.h"

void robot::robot_spin(double array[], int len)
{
    for (int i = 0; i < len-1; i++)
            robot_data.joint_data.out[i] = array[i];
}
robot::robot(/* args */){}
robot::~robot(){}

