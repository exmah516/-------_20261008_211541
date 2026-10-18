// Math4wpf.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <msclr\marshal_cppstd.h>
using namespace System;
using namespace System::IO;
//using namespace std;

#include "Eigen/Dense"

#include "CalcCall.h"

//#include "./eigen/include/eigen3/Eigen/Dense"
//#include "./eigen/include/eigen3/Eigen/Core"
//#include "./eigen/include/eigen3/Eigen/Geometry"
using namespace Eigen;

namespace Urdf {
	public ref class UrdfMath {
    public:
        // 弧度转角度
        static double Rad2Ang(double rad) {
            return rad * 180 / 3.14159265;
        }
        // 角度转弧度
        static double Ang2Rad(double ang) {
            return ang / 180 * 3.14159265;
        }

        // 根据旋转角度获取转换矩阵
        static void GetMatrixByAngle(double x, double y, double z,
			[Runtime::InteropServices::Out]array<double>^% matrix) {
			matrix = gcnew array<double>(16);

			//x
			AngleAxisd ax_x(x, Vector3d(1, 0, 0));
			Matrix3d rx = ax_x.matrix();

			//y
			AngleAxisd ax_y(y, Vector3d(0, 1, 0));
			Matrix3d ry = ax_y.matrix();

			//z
			AngleAxisd ax_z(z, Vector3d(0, 0, 1));
			Matrix3d rz = ax_z.matrix();

			Matrix3d R = rz * ry * rx;
			//std::cout << R << std::endl;

			//std::cout << R.data()[0] << "  " << R.data()[1] << "  " << R.data()[2] << std::endl;
			//std::cout << R.data()[3] << "  " << R.data()[4] << "  " << R.data()[5] << std::endl;
			//std::cout << R.data()[6] << "  " << R.data()[7] << "  " << R.data()[8] << std::endl;


			matrix[0] = R.data()[0];
			matrix[1] = R.data()[3];
			matrix[2] = R.data()[6];

			matrix[4] = R.data()[1];
			matrix[5] = R.data()[4];
			matrix[6] = R.data()[7];

			matrix[8] = R.data()[2];
			matrix[9] = R.data()[5];
			matrix[10] = R.data()[8];

			matrix[15] = 1;

        }

		// 连续旋转
		static void GetMatrixByAngle2([Runtime::InteropServices::In]array<double>^% matrixIn, 
			[Runtime::InteropServices::In]array<double>^% matrixIn2) {

			double dM1[16], dM2[16];
			for (int i = 0; i < 16; i++) {
				dM1[i] = matrixIn[i];
				dM2[i] = matrixIn2[i];
			}

			CalcCall::GetMatrixByAngle2(dM1, dM2);

			for (int i = 0; i < 16; i++) {
				matrixIn[i] = dM1[i];
			}
		}

	};
}