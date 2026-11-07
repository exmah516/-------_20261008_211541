#include"screw.h"

//转轴，转角（弧度）：指数积
Matrix3d  Rodrigues(Vector3d w_axis, double theta)
{
	//归一化
	w_axis = w_axis / w_axis.norm();

	//创建反对称矩阵
	Matrix3d w_hat;

	w_hat << 0, -w_axis(2), w_axis(1),
		w_axis(2), 0, -w_axis(0),
		-w_axis(1), w_axis(0), 0;

	Matrix3d I3 = Eigen::Matrix3d::Identity();

	//罗德里格斯公式
	Matrix3d Rodrigues_dR = I3 + w_hat * sin(theta) + w_hat * w_hat * (1 - cos(theta));

	return Rodrigues_dR;
}

Matrix3d  R(string s, double theta)
{
	Vector3d w_axis;

	if (s == "z" || s == "Z")
		w_axis = Vector3d(0, 0, 1);
	else if (s == "x" || s == "X")
		w_axis = Vector3d(1, 0, 0);
	else if (s == "y" || s == "Y")
		w_axis = Vector3d(0, 1, 0);

	//创建反对称矩阵
	Matrix3d w_hat;

	w_hat << 0, -w_axis(2), w_axis(1),
		w_axis(2), 0, -w_axis(0),
		-w_axis(1), w_axis(0), 0;

	Matrix3d I3 = Eigen::Matrix3d::Identity();

	//罗德里格斯公式
	Matrix3d Rodrigues_dR = I3 + w_hat * sin(theta) + w_hat * w_hat * (1 - cos(theta));

	return Rodrigues_dR;
}

Matrix3d  hat(Vector3d w)
{
	Matrix3d w_hat;
	Vector3d w_axis = w / w.norm();

	w_hat << 0, -w_axis(2), w_axis(1),
		w_axis(2), 0, -w_axis(0),
		-w_axis(1), w_axis(0), 0;

	return w_hat;
}

Vector3d hat_off(Matrix3d w_hat)
{
	return Vector3d(w_hat(2, 1), w_hat(0, 2), w_hat(1, 0));
}

double tr(Matrix3d R)
{
	return R(0, 0) + R(1, 1) + R(2, 2);
}

Matrix3d  Rot(Vector3d w, double theta)
{
	Vector3d w_axis;

	//创建反对称矩阵
	Matrix3d w_hat;

	//归一化
	w_axis = w / w.norm();
	w_hat << 0, -w_axis(2), w_axis(1),
		w_axis(2), 0, -w_axis(0),
		-w_axis(1), w_axis(0), 0;

	Matrix3d I3 = Eigen::Matrix3d::Identity();

	//罗德里格斯公式
	Matrix3d Rodrigues_dR = I3 + w_hat * sin(theta) + w_hat * w_hat * (1 - cos(theta));

	return Rodrigues_dR;
}

void clamp(double &temp,double min, double max)
{
	double MAX, MIN;

	if (max > min)
	{
		MAX = max;
		MIN = min;
	}
	else if (max < min)
	{
		MIN = max;
		 MAX= min;
	}
	else 
		MIN = MAX= max;

	if (temp > MAX)temp = max;
	else if (temp < MIN)temp = min;
}

//旋转轴线方向，转角（弧度）,轴线的原点位置：指数积
//0向量，位移,位移方向：指数积
Matrix4d  Conversion(Vector3d w_axis, double theta, Vector3d r)
{
	Matrix4d T = Matrix4d::Identity();

	if (w_axis.norm() == 0)
	{
		Matrix3d Rodrigues_dR = Matrix3d::Identity();

		//位移部分
		Vector3d v = r / r.norm();
		Vector3d Translation = v * theta;

		//整合
		T.block<3, 3>(0, 0) = Rodrigues_dR;
		T.block<3, 1>(0, 3) = Translation;
	}
	else
	{
		//归一化
		w_axis = w_axis / w_axis.norm();

		//创建反对称矩阵
		Matrix3d w_hat;

		w_hat << 0, -w_axis(2), w_axis(1),
			w_axis(2), 0, -w_axis(0),
			-w_axis(1), w_axis(0), 0;

		Matrix3d I3 = Matrix3d::Identity();

		//罗德里格斯公式
		Matrix3d Rodrigues_dR = I3 + w_hat * sin(theta) + w_hat * w_hat * (1 - cos(theta));

		//位移部分,v可以看作整个机器人按照w_axis单位轴旋转时候，基座的线速度（大小及方向）
		Vector3d v = -w_axis.cross(r);
		Vector3d Translation = (I3 - Rodrigues_dR) * (r - w_axis * (w_axis.transpose() * r));

		//整合
		T.block<3, 3>(0, 0) = Rodrigues_dR;
		T.block<3, 1>(0, 3) = Translation;
	}
	return T;
}

//反解罗德里格斯公式
void Rever_Conversion(Matrix4d T, double& theta, Vector3d& K)
{
	double  tr = T(0, 0) + T(1, 1) + T(2, 2);

	theta = acos((tr - 1) * 0.5);

	Matrix3d R = T.block<3, 3>(0, 0);
	Matrix3d k = (R - R.transpose()) / (2 * sin(theta));
	K = Vector3d(k(2, 1), k(0, 2), k(1, 0));
}

double Rever_Conversion(Matrix4d T)
{
	Matrix3d R = T.block<3, 3>(0, 0);

	return atan2(R(0, 2) - R(2, 0), R(2, 1) - R(1, 2));
}

//计算两个向量之间的转角
double angle(Vector3d j1, Vector3d j2)
{

	double tanVal = j1.cross(j2).norm() / (j1.dot(j2));
	return atan(tanVal);
}

void Polynomial3(int times, double& dt, double x0, double y0, double x1, double y1, double v0, double v1)
{
	double a0 = y0;
	double a1 = v0;
	double h = y1 - y0;
	double T = x1 - x0;
	double a2 = (3 * h - (2 * v0 + v1) * T) / pow(T, 2);
	double a3 = (-2 * h + (v0 + v1) * T) / pow(T, 3);
	dt = a0 + a1 * (times - x0) + a2 * pow(times - x0, 2) + a3 * pow(times - x0, 3);
}

double Polynomial3(int times, double x0, double y0, double x1, double y1, double v0, double v1)
{
	double a0 = y0;
	double a1 = v0;
	double h = y1 - y0;
	double T = x1 - x0;
	double a2 = (3 * h - (2 * v0 + v1) * T) / pow(T, 2);
	double a3 = (-2 * h + (v0 + v1) * T) / pow(T, 3);
	return a0 + a1 * (times - x0) + a2 * pow(times - x0, 2) + a3 * pow(times - x0, 3);
}

void Polynomial5(int times, double& dt, double x0, double y0, double x1, double y1, double v0, double v1)
{
	double a0 = y0;
	double a1 = v0;
	double h = y1 - y0;
	double T = x1 - x0;
	double a2 = 0;
	double a3 = (20 * h - (8 * v1 + 12 * v0) * T) / (2 * pow(T, 3));
	double a4 = (-30 * h + (14 * v1 + 16 * v0) * T) / (2 * pow(T, 4));
	double a5 = (12 * h - (6 * v1 + 6 * v0) * T) / (2 * pow(T, 5));

	dt = a0 + a1 * (times - x0) + a2 * pow(times - x0, 2) + a3 * pow(times - x0, 3) + a4 * pow(times - x0, 4) + a5 * pow(times - x0, 5);
}

void Polynomial1(int times, double& dt, double x0, double y0, double x1, double y1)
{
	double k = (y0 - y1) / (x0 - x1);
	double b = y0 - k * x0;

	dt = k * times + b;
}

double Polynomial5(int times, double x0, double y0, double x1, double y1, double v0, double v1)
{
	double a0 = y0;
	double a1 = v0;
	double h = y1 - y0;
	double T = x1 - x0;
	double a2 = 0;
	double a3 = (20 * h - (8 * v1 + 12 * v0) * T) / (2 * pow(T, 3));
	double a4 = (-30 * h + (14 * v1 + 16 * v0) * T) / (2 * pow(T, 4));
	double a5 = (12 * h - (6 * v1 + 6 * v0) * T) / (2 * pow(T, 5));

	return a0 + a1 * (times - x0) + a2 * pow(times - x0, 2) + a3 * pow(times - x0, 3) + a4 * pow(times - x0, 4) + a5 * pow(times - x0, 5);
}