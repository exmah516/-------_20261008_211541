#pragma once

#include <Eigen\Dense>

using namespace std;
using namespace Eigen;

#define PI 3.141592653
#define Rad 57.295779513082320876798154819203

//转轴，转角（弧度）：指数积
Matrix3d  Rodrigues(Vector3d w_axis, double theta);

Matrix3d  R(string s, double theta);

Matrix3d  Rot(Vector3d w, double theta);

double tr(Matrix3d R);

Matrix3d  hat(Vector3d w);

Vector3d hat_off(Matrix3d w_hat);

//旋转轴线方向，转角（弧度）,轴线的原点位置：指数积
//0向量，位移,位移方向：指数积
Matrix4d  Conversion(Vector3d w_axis, double theta, Vector3d r);

void clamp(double &temp ,double min, double max);

//反解罗德里格斯公式
void Rever_Conversion(Matrix4d T, double& theta, Vector3d& K);

double Rever_Conversion(Matrix4d T);

//计算两个向量之间的转角
double angle(Vector3d j1, Vector3d j2);

void Polynomial3(int times, double& dt, double x0, double y0, double x1, double y1, double v0, double v1);

double Polynomial3(int times, double x0, double y0, double x1, double y1, double v0, double v1);

void Polynomial5(int times, double& dt, double x0, double y0, double x1, double y1, double v0, double v1);

void Polynomial1(int times, double& dt, double x0, double y0, double x1, double y1);

double Polynomial5(int times, double x0, double y0, double x1, double y1, double v0, double v1);