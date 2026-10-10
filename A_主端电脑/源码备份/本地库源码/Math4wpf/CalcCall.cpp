#include "CalcCall.h"
#include "Eigen/Dense"

using namespace Eigen;


void CalcCall::GetMatrixByAngle2(double* pM1, double* pM2) {
	Eigen::Matrix4d m1 = (Eigen::Matrix4d() << 
		pM1[0], pM1[1], pM1[2], pM1[3],
		pM1[4], pM1[5], pM1[6], pM1[7],
		pM1[8], pM1[9], pM1[10],pM1[11],
		pM1[12],pM1[13],pM1[14],pM1[15]).finished();

	Eigen::Matrix4d m2 = (Eigen::Matrix4d() <<
		pM2[0], pM2[1], pM2[2], pM2[3],
		pM2[4], pM2[5], pM2[6], pM2[7],
		pM2[8], pM2[9], pM2[10],pM2[11],
		pM2[12],pM2[13],pM2[14],pM2[15]).finished();

	Matrix4d m = m1 * m2;


	pM1[0] = m.data()[0];
	pM1[1] = m.data()[4];
	pM1[2] = m.data()[8];
	pM1[3] = m.data()[12];

	pM1[4] = m.data()[1];
	pM1[5] = m.data()[5];
	pM1[6] = m.data()[9];
	pM1[7] = m.data()[13];

	pM1[8] = m.data()[2];
	pM1[9] = m.data()[6];
	pM1[10] = m.data()[10];
	pM1[11] = m.data()[14];

	pM1[12] = m.data()[3];
	pM1[13] = m.data()[7];
	pM1[14] = m.data()[11];
	pM1[15] = m.data()[15];
}