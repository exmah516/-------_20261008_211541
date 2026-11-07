#include "DSA.h"

// 与实验补偿相同的旋量指数：移动关节用米，转动关节用弧度。
static Matrix4d 现场旋量指数(const Vector6d& axis, double q)
{
	const Vector3d w = axis.head<3>(), v = axis.tail<3>();
	if (w.squaredNorm() < 1e-16)
	{
		Matrix4d pose = Matrix4d::Identity();
		pose.block<3, 1>(0, 3) = v * q;
		return pose;
	}
	return Conversion(w, q, w.cross(v) / w.squaredNorm());
}

void DSA::设置现场补偿参数()
{
	// 直接使用补偿验证的192组辨识结果；集中在这一个文件中，便于核对。
	#include "现场补偿参数.cpp.inc"
	xi_lower_comp = xi_lower;
	xi_upper_comp = xi;
	// 实验与现场关节6的读数零点相差40mm，只在模型内换参考零点。
	// q6_实验 = q6_现场 - 0.040；之后输出已经是现场读数，不再额外加40。
	Matrix4d zero_shift = Matrix4d::Identity();
	zero_shift.block<3, 1>(0, 3) = -0.040 * xi.col(5).tail<3>();
	xi_upper_comp.col(6).tail<3>() += zero_shift.block<3, 1>(0, 3).cross(xi.col(6).head<3>());
	C_field_comp = zero_shift * C;
	C_control_comp = C_field_comp;
}

void DSA::建立补偿初始参考()
{
	d1 = d1f = 关节编码器值[7] / Rad;
	d2 = d2f = 关节编码器值[8] / Rad;
	h = atan2(sin(d2) * cos(d1), cos(d2)) * Rad;
	r = atan2(sin(d1) * sin(d2) * cos(h / Rad), cos(d2)) * Rad;
	u3f = 关节编码器值[2] / Rad;
	u7f = 关节编码器值[6] / Rad;
	p6 = 0;
	const Matrix4d F_lower = 现场旋量指数(xi_lower_comp.col(0), d1) *
		现场旋量指数(xi_lower_comp.col(1), d2);
	Matrix4d F_upper = Matrix4d::Identity();
	for (int j = 0; j < 7; ++j)
		F_upper *= 现场旋量指数(xi_upper_comp.col(j), 关节编码器值[j] / ((j == 0 || j == 1 || j == 5) ? 1000.0 : Rad));
	const Matrix4d initial_relative = F_lower.inverse() * F_upper * C_field_comp;
	// 保留回零后的原轴向间距，以这个距离作为高度0；只修正横向和姿态。
	// 这是任务高度参考，辨识得到的 C_field_comp 保持完整，不改PLC零点。
	Matrix4d height_reference = Matrix4d::Identity();
	height_reference(2, 3) = -initial_relative(2, 3);
	C_control_comp = C_field_comp * height_reference;
	compensation_initialized = true;
	collision_wait_neutral = true;
	启停阶段速度限制的分母 = 30;
}

void DSA::补偿初始点过渡()
{
	if (collision_state != 0)
	{
		// 初始化中碰撞也先保持；解除后从实际停点重走剩下的初始过渡。
		initial_move_active = false;
		std::copy(关节编码器值, 关节编码器值 + 10, target_joint);
		return;
	}
	if (!initial_move_active)
	{
		std::copy(关节编码器值, 关节编码器值 + 10, initial_start_joint);
		std::copy(target_joint, target_joint + 10, initial_goal_joint);
		initial_move_seconds = 2.0;
		for (int j = 0; j < 9; ++j)
		{
			// 沿用现场回零速度8mm/s、3度/s，五次曲线最大斜率为1.875。
			const double speed = (j == 0 || j == 1 || j == 5) ? 8.0 : 3.0;
			initial_move_seconds = (std::max)(initial_move_seconds, 1.875 * abs(initial_goal_joint[j] - initial_start_joint[j]) / speed);
		}
		initial_move_begin = std::chrono::steady_clock::now();
		initial_move_active = true;
		std::cout << "补偿初始过渡开始，预计轨迹时间 " << initial_move_seconds << " 秒；到位后再接手柄。" << std::endl;
	}
	double progress = std::chrono::duration<double>(std::chrono::steady_clock::now() - initial_move_begin).count() / initial_move_seconds;
	progress = (std::min)(1.0, progress);
	const double ratio = progress * progress * progress * (10.0 - 15.0 * progress + 6.0 * progress * progress);
	bool arrived = progress >= 1.0;
	for (int j = 0; j < 9; ++j)
	{
		target_joint[j] = initial_start_joint[j] + ratio * (initial_goal_joint[j] - initial_start_joint[j]);
		const double tolerance = (j == 0 || j == 1 || j == 5) ? 0.1 : 0.05;
		arrived = arrived && abs(关节编码器值[j] - initial_goal_joint[j]) <= tolerance;
	}
	if (arrived)
	{
		compensation_ready = true;
		collision_task_latched = true; // 空闲时也继续保持补偿后的对齐目标。
		std::cout << "已到达补偿初始点；运动按钮回中立后可操作。" << std::endl;
	}
}

DSA::DSA()
{
	设置现场补偿参数();
	//a.OpenComm_inside();
	a.OpenComm();

	start = std::chrono::system_clock::now();
	File.open(format_time_to_string(start) + ".csv", std::ios::out);
}

DSA::~DSA()
{
	a.CloseComm();
	File.close();
}

void DSA::identify_theory(Matrix4d& Ta, Matrix4d& T)
{
	VectorXd x(7);
	double theta = 1;
	double dis = 1;
	Vector3d w;

	double temp[7];
	temp[0] = u1;
	temp[1] = u2;
	temp[2] = u3;
	temp[3] = u4;
	temp[4] = u5;
	temp[5] = u6;
	temp[6] = u7;

	//迭代辨识
	while (dis * 1000 > 0.0000000001 || theta * Rad > 0.0000000001)
	{
		MatrixXd* A = new MatrixXd;
		VectorXd* Y = new VectorXd;
		*Y = y(Ta, T);
		*A = Cali_Jacobian_err();
		x = (*A).transpose() * ((*A) * (*A).transpose()).inverse() * (*Y);
		delete A, Y;

		u1 -= x(0);
		u2 -= x(1);
		u3 -= x(2);
		u4 -= x(3);
		u5 -= x(4);
		u6 -= x(5);
		u7 -= x(6);

		Ta = Forward_exponential_up_err();
		Rever_Conversion(Ta.inverse() * T, theta, w);
		dis = pow((Ta - T)(0, 3) * (Ta - T)(0, 3) + (Ta - T)(1, 3) * (Ta - T)(1, 3) + (Ta - T)(2, 3) * (Ta - T)(2, 3), 0.5);

		cout << "姿态误差\t" << theta * Rad << "°" << endl;
		cout << "位置误差\t" << dis * 1000 << "mm" << endl << endl;
	}

	cout << "一共补偿了：\n";
	cout << (u1 - temp[0]) * 1000 << "mm\n"
		<< (u2 - temp[1]) * 1000 << "mm\n"
		<< (u3 - temp[2]) * Rad << "°\n"
		<< (u4 - temp[3]) * Rad << "°\n"
		<< (u5 - temp[4]) * Rad << "°\n"
		<< (u6 - temp[5]) * 1000 << "mm\n"
		<< (u7 - temp[6]) * Rad << "°\n";

	if (up_6_vir > (25.0374 + 72.88105) / 1000)cout << "碰壁了";
	if (up_6_vir < (-49.2005 - 72.88105) / 1000)cout << "碰壁了";

	if (up_1_vir > (14.93 + 21.08) / 1000)cout << "碰壁了";
	if (up_1_vir < (-599.41 - 21.08) / 1000)cout << "碰壁了";

	if (up_2_vir > (401.691 + 8.8415) / 1000)cout << "碰壁了";
	if (up_2_vir < (-212.626 - 8.8415) / 1000)cout << "碰壁了";

	return;
}

vector<string> DSA::split(const string& s, const string& seperator)
{
	vector<string> result;
	typedef string::size_type string_size;
	string_size i = 0;

	while (i != s.size())
	{
		//找到字符串中首个不等于分隔符的字母；
		int flag = 0;
		while (i != s.size() && flag == 0)
		{
			flag = 1;
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[i] == seperator[x])
				{
					++i;
					flag = 0;
					break;
				}
		}

		//找到又一个分隔符，将两个分隔符之间的字符串取出；
		flag = 0;
		string_size j = i;
		while (j != s.size() && flag == 0)
		{
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[j] == seperator[x])
				{
					flag = 1;
					break;
				}
			if (flag == 0)
				++j;
		}
		if (i != j)
		{
			result.push_back(s.substr(i, j - i));
			i = j;
		}
	}
	return result;
}

//误差测量
void DSA::NDI(Matrix4d& Ta, Matrix4d& T)
{
	//在ndi界面导入文件需要先导入参考系1，再导入参考系2,最后会得到参考系2相对于参考系1的位置
	//在record界面，把两个标定架都选上，passive strays不要选
	//勾上Include column header
	ifstream infile;
	string neirong;

	double qw, x, y, z, qx, qy, qz;
	Quaterniond tmp;
	Matrix3d Rot1_end, Rot2_end, Rot1_start, Rot2_start;
	Vector3d  Trans1_start, Trans2_start, Trans1_end, Trans2_end;
	homo h1;

	deque <homo>dq(aver_num);

	Matrix4d T1_start, T1_end, T2_start, T2_end;
	T1_start = T1_end = T2_start = T2_end = Matrix4d::Identity();

	//第一个标记物449
	int i = 0;
	infile.open("C:\\Users\\Administrator\\Desktop\\example.csv", ios::in);
	while (infile >> neirong)
	{
		vector<string> v = split(neirong, ","); //可按任意个字符来分隔;
		qw = x = y = z = qx = qy = qz = 0;

		//13开始每隔9个数记一次
		if (i > 12 && (i - 13) % 9 == 0)
		{
			h1.a[0] = atof(v[4].c_str());
			h1.a[1] = atof(v[5].c_str());
			h1.a[2] = atof(v[6].c_str());
			h1.a[3] = atof(v[7].c_str());
			h1.a[4] = atof(v[8].c_str());
			h1.a[5] = atof(v[9].c_str());
			h1.a[6] = atof(v[10].c_str());

			//队列没有满就补充内容
			if (dq.size() < aver_num)	dq.push_front(h1);

			//队列满了就更新内容并且计算现在队列里面的总的四元数和总的位置
			else
			{
				dq.push_front(h1);
				dq.pop_back();
				for (int i = 0; i < aver_num; i++)
				{
					qx += dq[i].a[1];
					qy += dq[i].a[2];
					qz += dq[i].a[3];
					qw += dq[i].a[0];
					x += dq[i].a[4];
					y += dq[i].a[5];
					z += dq[i].a[6];
				}
			}

			//计算当前队列的平均四元数和位置和其次矩阵
			tmp.x() = qx / aver_num;
			tmp.y() = qy / aver_num;
			tmp.z() = qz / aver_num;
			tmp.w() = qw / aver_num;
			Rot1_end = tmp.normalized().toRotationMatrix();
			Trans1_end = Vector3d(x / aver_num, y / aver_num, z / aver_num);

			T1_end.block<3, 3>(0, 0) = Rot1_end;
			T1_end.block<3, 1>(0, 3) = Trans1_end;

			//获取最开始的平均四元数的位置
			if (i == int(aver_num * 9 + 13))
			{
				Trans1_start = Trans1_end;
				Rot1_start = Rot1_end;
				T1_start.block<3, 3>(0, 0) = Rot1_start;
				T1_start.block<3, 1>(0, 3) = Trans1_start;
			}
		}
		i++;
	}
	infile.close();

	//第二个标记物339
	infile.open("C:\\Users\\Administrator\\Desktop\\example.csv", ios::in);
	dq.clear();
	i = 0;
	while (infile >> neirong)
	{
		vector<string> v = split(neirong, ","); //可按任意个字符来分隔;
		qw = x = y = z = qx = qy = qz = 0;

		//17开始每隔9个数记一次
		if (i > 16 && (i - 17) % 9 == 0)
		{
			h1.a[0] = atof(v[4].c_str());
			h1.a[1] = atof(v[5].c_str());
			h1.a[2] = atof(v[6].c_str());
			h1.a[3] = atof(v[7].c_str());
			h1.a[4] = atof(v[8].c_str());
			h1.a[5] = atof(v[9].c_str());
			h1.a[6] = atof(v[10].c_str());

			//队列没有满就补充内容
			if (dq.size() < aver_num)	dq.push_front(h1);

			//队列满了就更新内容并且计算现在队列里面的总的四元数和总的位置
			else
			{
				dq.push_front(h1);
				dq.pop_back();
				for (int i = 0; i < aver_num; i++)
				{
					qx += dq[i].a[1];
					qy += dq[i].a[2];
					qz += dq[i].a[3];
					qw += dq[i].a[0];
					x += dq[i].a[4];
					y += dq[i].a[5];
					z += dq[i].a[6];
				}
			}

			//计算当前队列的平均四元数和位置和其次矩阵
			tmp.x() = qx / aver_num;
			tmp.y() = qy / aver_num;
			tmp.z() = qz / aver_num;
			tmp.w() = qw / aver_num;
			Rot2_end = tmp.normalized().toRotationMatrix();
			Trans2_end = Vector3d(x / aver_num, y / aver_num, z / aver_num);

			T2_end.block<3, 3>(0, 0) = Rot2_end;
			T2_end.block<3, 1>(0, 3) = Trans2_end;

			//获取最开始的平均四元数的位置
			if (i == int(aver_num * 9 + 17))
			{
				Trans2_start = Trans2_end;
				Rot2_start = Rot2_end;

				T2_start.block<3, 3>(0, 0) = Rot2_start;
				T2_start.block<3, 1>(0, 3) = Trans2_start;
			}
		}
		i++;
	}
	infile.close();

	//各取所需
	Matrix4d trans_rcm1, trans_rcm2, rx90, rz180, rx180;

	//上臂449   
	trans_rcm1 << 1, 0, 0, 10.13,
		0, 1, 0, -40.15,
		0, 0, 1, -163.37 - 59.0,
		0, 0, 0, 1;

	//下臂339
	trans_rcm2 << 1, 0, 0, 23.13,
		0, 1, 0, 46.77,
		0, 0, 1, 17.3,
		0, 0, 0, 1;

	rx90 = rz180 = rx180 = Matrix4d::Identity();

	AngleAxisd ax05_2(PI / 2, Vector3d(1, 0, 0));
	Matrix3d r05_2 = ax05_2.matrix();
	rx90.block<3, 3>(0, 0) = r05_2;

	AngleAxisd ax05(PI, Vector3d(0, 0, 1));
	Matrix3d r05 = ax05.matrix();
	rz180.block<3, 3>(0, 0) = r05;

	AngleAxisd ax05_3(PI, Vector3d(1, 0, 0));
	Matrix3d r05_3 = ax05_3.matrix();
	rx180.block<3, 3>(0, 0) = r05_3;

	Ta = T1_end * rx90 * rz180 * trans_rcm1;
	T = T2_end * rx180 * rx90 * rz180 * trans_rcm2;

	//误差补偿的最后一列是m，这里要换算mm到m
	Ta(0, 3) *= 0.001;
	Ta(1, 3) *= 0.001;
	Ta(2, 3) *= 0.001;
	T(0, 3) *= 0.001;
	T(1, 3) *= 0.001;
	T(2, 3) *= 0.001;

	Ta = FK_down() * T.inverse() * Ta;		//上臂相对于基座的位姿
	T = FK_down();							//下臂相对于基座的位姿

	return;
}

MatrixXd DSA::Cali_Jacobian()
{
	Vector3d  w7, w6, w5, w4, w3, w2, w1,//旋转关节的方向
		r6, r5, r4, r3, r2, r1,
		zero;//零轴，轴角法的轴

	Matrix4d e1, e2, e3, e4, e5, e6, e7;//变换矩阵

	w7 = Vector3d(0, 0, 1);
	w5 = Vector3d(-cos(29.449820210574603297399709992454 / Rad), sin(29.449820210574603297399709992454 / Rad), 0);
	w4 = Vector3d(sin(29.449820210574603297399709992454 / Rad), cos(29.449820210574603297399709992454 / Rad), 0);
	w3 = Vector3d(0, 0, 1);

	zero = Vector3d(0, 0, 0);
	r6 = Vector3d(0, 0, 1);
	r2 = Vector3d(1, 0, 0);
	r1 = Vector3d(0, 1, 0);
	r5 = Vector3d(-0.1968, 0.11112, 0.54912);
	r4 = Vector3d(-0.094533, 0.29224, 0.54912);
	r3 = Vector3d(-0.094533, 0.29224, 0.54912);

	e7 = Conversion(w7, u7, zero);
	e6 = Conversion(zero, u6, r6);
	e5 = Conversion(w5, u5, r5);
	e4 = Conversion(w4, u4, r4);
	e3 = Conversion(w3, u3, r3);
	e2 = Conversion(zero, u2, r2);
	e1 = Conversion(zero, u1, r1);

	VectorXd s1(6), s2(6), s3(6), s4(6), s5(6), s6(6), s7(6);
	s1 << r1(0), r1(1), r1(2), zero(0), zero(1), zero(2);
	s2 << r2(0), r2(1), r2(2), zero(0), zero(1), zero(2);
	s3 << r3(0), r3(1), r3(2), w3(0), w3(1), w3(2);
	s4 << r4(0), r4(1), r4(2), w4(0), w4(1), w4(2);
	s5 << r5(0), r5(1), r5(2), w5(0), w5(1), w5(2);
	s6 << r6(0), r6(1), r6(2), zero(0), zero(1), zero(2);
	s7 << zero(0), zero(1), zero(2), w7(0), w7(1), w7(2);

	VectorXd A1(6), A2(6), A3(6), A4(6), A5(6), A6(6), A7(6);

	Matrix4d ex = e1 * e2 * e3 * e4;

	A1 = Ad(e1) * s1;
	A2 = Ad(e1 * e2) * s2;
	A3 = Ad(e1 * e2 * e3) * s3;
	A4 = Ad(ex) * s4;
	A5 = Ad(ex * e5) * s5;
	A6 = Ad(ex * e5 * e6) * s6;
	A7 = Ad(ex * e5 * e6 * e7) * s7;

	//A矩阵
	MatrixXd A(6, 7);

	A.block<6, 1>(0, 0) = A1;
	A.block<6, 1>(0, 1) = A2;
	A.block<6, 1>(0, 2) = A3;
	A.block<6, 1>(0, 3) = A4;
	A.block<6, 1>(0, 4) = A5;
	A.block<6, 1>(0, 5) = A6;
	A.block<6, 1>(0, 6) = A7;

	return A;
}

MatrixXd DSA::Cali_Jacobian_err()
{
	Vector3d  w7, w6, w5, w4, w3, w2, w1,//旋转关节的方向
		r6, r5, r4, r3, r2, r1,
		zero;//零轴，轴角法的轴

	Matrix4d e1, e2, e3, e4, e5, e6, e7;//变换矩阵

	w7 = Vector3d(0, 0, 1);
	w5 = Vector3d(-cos(29.449820210574603297399709992454 / Rad), sin(29.449820210574603297399709992454 / Rad), 0);
	w4 = Vector3d(sin(29.449820210574603297399709992454 / Rad), cos(29.449820210574603297399709992454 / Rad), 0);
	w3 = Vector3d(0, 0, 1);

	zero = Vector3d(0, 0, 0);
	r6 = Vector3d(0, 0, 1);
	r2 = Vector3d(1, 0, 0);
	r1 = Vector3d(0, 1, 0);
	r5 = Vector3d(-0.1968, 0.11112, 0.54912);
	r4 = Vector3d(-0.094533, 0.29224, 0.54912);
	r3 = Vector3d(-0.094533, 0.29224, 0.54912);

	e7 = Conversion(w7, up_7_vir, zero);
	e6 = Conversion(zero, up_6_vir, r6);
	e5 = Conversion(w5, up_5_vir, r5);
	e4 = Conversion(w4, up_4_vir, r4);
	e3 = Conversion(w3, up_3_vir, r3);
	e2 = Conversion(zero, up_2_vir, r2);
	e1 = Conversion(zero, up_1_vir, r1);

	VectorXd s1(6), s2(6), s3(6), s4(6), s5(6), s6(6), s7(6);
	s1 << r1(0), r1(1), r1(2), zero(0), zero(1), zero(2);
	s2 << r2(0), r2(1), r2(2), zero(0), zero(1), zero(2);
	s3 << r3(0), r3(1), r3(2), w3(0), w3(1), w3(2);
	s4 << r4(0), r4(1), r4(2), w4(0), w4(1), w4(2);
	s5 << r5(0), r5(1), r5(2), w5(0), w5(1), w5(2);
	s6 << r6(0), r6(1), r6(2), zero(0), zero(1), zero(2);
	s7 << zero(0), zero(1), zero(2), w7(0), w7(1), w7(2);

	VectorXd A1(6), A2(6), A3(6), A4(6), A5(6), A6(6), A7(6);

	Matrix4d ex = e1 * e2 * e3 * e4;

	A1 = Ad(e1) * s1;
	A2 = Ad(e1 * e2) * s2;
	A3 = Ad(e1 * e2 * e3) * s3;
	A4 = Ad(ex) * s4;
	A5 = Ad(ex * e5) * s5;
	A6 = Ad(ex * e5 * e6) * s6;
	A7 = Ad(ex * e5 * e6 * e7) * s7;

	//A矩阵
	MatrixXd A(6, 7);

	A.block<6, 1>(0, 0) = A1;
	A.block<6, 1>(0, 1) = A2;
	A.block<6, 1>(0, 2) = A3;
	A.block<6, 1>(0, 3) = A4;
	A.block<6, 1>(0, 4) = A5;
	A.block<6, 1>(0, 5) = A6;
	A.block<6, 1>(0, 6) = A7;

	return A;
}

VectorXd DSA::y(Matrix4d Ta, Matrix4d T)
{
	Matrix4d temp = T * Ta.inverse() - Matrix4d::Identity();

	VectorXd v(6);
	v << temp(0, 3), temp(1, 3), temp(2, 3), temp(2, 1), temp(0, 2), temp(1, 0);

	return v;
}

MatrixXd DSA::Ad(Matrix4d T)
{
	Matrix3d temp;
	temp << 0, -T(2, 3), T(1, 3), T(2, 3), 0, -T(0, 3), -T(1, 3), T(0, 3), 0;

	MatrixXd Ai(6, 6);
	Ai.block<3, 3>(0, 0) = T.block<3, 3>(0, 0);
	Ai.block<3, 3>(3, 3) = T.block<3, 3>(0, 0);
	Ai.block<3, 3>(3, 0) = temp * T.block<3, 3>(0, 0);
	Ai.block<3, 3>(0, 3) = MatrixXd::Zero(3, 3);
	return Ai;
}

MatrixXd DSA::Jacobian()
{
	Vector3d  w7, w6, w5, w4, w3, w2, w1,//旋转关节的方向
		r6, r5, r4, r3, r2, r1,
		zero;//零轴，轴角法的轴

	MatrixXd J(6, 7);//J矩阵

	double theta0 = 29.449820210574603297399709992454 / Rad;

	Matrix4d e1, e2, e3, e4, e5, e6, e7;//变换矩阵

	VectorXd s1(6), s2(6), s3(6), s4(6), s5(6), s6(6), s7(6),//各个轴的旋量坐标
		J1(6), J2(6), J3(6), J4(6), J5(6), J6(6), J7(6);//J矩阵各列

	w5 = Vector3d(-cos(theta0), sin(theta0), 0);
	w4 = Vector3d(sin(theta0), cos(theta0), 0);
	w3 = w7 = Vector3d(0, 0, 1);

	zero = Vector3d(0, 0, 0);
	r6 = Vector3d(0, 0, 1);
	r2 = Vector3d(1, 0, 0);
	r1 = Vector3d(0, 1, 0);
	r5 = Vector3d(-0.1968, 0.11112, 0.54912);
	r4 = Vector3d(-0.094533, 0.29224, 0.54912);
	r3 = Vector3d(-0.094533, 0.29224, 0.54912);

	e7 = Conversion(w7, u7, zero);
	e6 = Conversion(zero, u6, r6);
	e5 = Conversion(w5, u5, r5);
	e4 = Conversion(w4, u4, r4);
	e3 = Conversion(w3, u3, r3);
	e2 = Conversion(zero, u2, r2);
	e1 = Conversion(zero, u1, r1);

	s1 << r1(0), r1(1), r1(2), zero(0), zero(1), zero(2);
	s2 << r2(0), r2(1), r2(2), zero(0), zero(1), zero(2);
	s3 << r3(0), r3(1), r3(2), w3(0), w3(1), w3(2);
	s4 << r4(0), r4(1), r4(2), w4(0), w4(1), w4(2);
	s5 << r5(0), r5(1), r5(2), w5(0), w5(1), w5(2);
	s6 << r6(0), r6(1), r6(2), zero(0), zero(1), zero(2);
	s7 << zero(0), zero(1), zero(2), w7(0), w7(1), w7(2);

	Matrix4d ex = e1 * e2 * e3 * e4;

	J1 = s1;
	J2 = Ad(e1) * s2;
	J3 = Ad(e1 * e2) * s3;
	J4 = Ad(e1 * e2 * e3) * s4;
	J5 = Ad(ex) * s5;
	J6 = Ad(ex * e5) * s6;
	J7 = Ad(ex * e5 * e6) * s7;

	J.block<6, 1>(0, 0) = J1;
	J.block<6, 1>(0, 1) = J2;
	J.block<6, 1>(0, 2) = J3;
	J.block<6, 1>(0, 3) = J4;
	J.block<6, 1>(0, 4) = J5;
	J.block<6, 1>(0, 5) = J6;
	J.block<6, 1>(0, 6) = J7;

	return J;
}

Matrix4d DSA::FK_up()
{
	Vector3d  w7, w6, w5, w4, w3, w2, w1,//旋转关节的方向
		r6, r5, r4, r3, r2, r1,
		zero;//零轴，轴角法的轴

	Matrix4d e1, e2, e3, e4, e5, e6, e7;//变换矩阵

	double theta0 = 29.449820210574603297399709992454 / Rad;

	w5 = Vector3d(-cos(theta0), sin(theta0), 0);
	w4 = Vector3d(sin(theta0), cos(theta0), 0);
	w3 = w7 = Vector3d(0, 0, 1);

	zero = Vector3d(0, 0, 0);
	r6 = Vector3d(0, 0, 1);
	r2 = Vector3d(1, 0, 0);
	r1 = Vector3d(0, 1, 0);

	r5 = Vector3d(-0.1968, 0.11112, 0.54912);
	r4 = Vector3d(-0.094533, 0.29224, 0.54912);
	r3 = Vector3d(-0.094533, 0.29224, 0.54912);

	e7 = Conversion(w7, u7, zero);
	e6 = Conversion(zero, u6, r6);
	e5 = Conversion(w5, u5, r5);
	e4 = Conversion(w4, u4, r4);
	e3 = Conversion(w3, u3, r3);
	e2 = Conversion(zero, u2, r2);
	e1 = Conversion(zero, u1, r1);

	return e1 * e2 * e3 * e4 * e5 * e6 * e7;
}

Matrix4d DSA::Forward_exponential_up_err()
{
	Vector3d  w7, w6, w5, w4, w3, w2, w1,//旋转关节的方向
		r6, r5, r4, r3, r2, r1,
		zero;//零轴，轴角法的轴

	Matrix4d e1, e2, e3, e4, e5, e6, e7;//变换矩阵

	double theta0 = 29.449820210574603297399709992454 / Rad;

	w5 = Vector3d(-cos(theta0), sin(theta0), 0);
	w4 = Vector3d(sin(theta0), cos(theta0), 0);
	w3 = w7 = Vector3d(0, 0, 1);

	zero = Vector3d(0, 0, 0);
	r6 = Vector3d(0, 0, 1);
	r2 = Vector3d(1, 0, 0);
	r1 = Vector3d(0, 1, 0);

	r5 = Vector3d(-0.1968, 0.11112, 0.54912);
	r4 = Vector3d(-0.094533, 0.29224, 0.54912);
	r3 = Vector3d(-0.094533, 0.29224, 0.54912);

	e7 = Conversion(w7, up_7_vir, zero);
	e6 = Conversion(zero, up_6_vir, r6);
	e5 = Conversion(w5, up_5_vir, r5);
	e4 = Conversion(w4, up_4_vir, r4);
	e3 = Conversion(w3, up_3_vir, r3);
	e2 = Conversion(zero, up_2_vir, r2);
	e1 = Conversion(zero, up_1_vir, r1);

	return e1 * e2 * e3 * e4 * e5 * e6 * e7;
}

Matrix4d DSA::FK_down()
{
	Vector3d  w2, w1, zero;

	Matrix4d e1, e2;//变换矩阵

	w1 = Vector3d(0, 0, 1);
	w2 = Vector3d(1, 0, 0);
	zero = Vector3d(0, 0, 0);

	e2 = Conversion(w2, d2, zero);
	e1 = Conversion(w1, d1, zero);

	return e1 * e2;
}


int DSA::Close_exe(string s)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
	std::wstring wWindowTitle(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wWindowTitle[0], size_needed);

	LPCWSTR windowTitle = wWindowTitle.c_str();

	HWND hwnd = FindWindowW(NULL, windowTitle); // 获取窗口句柄

	if (hwnd == NULL)
	{
		std::cout << "Error: Unable to find window." << std::endl;
		return -1;
	}

	DWORD processId;
	GetWindowThreadProcessId(hwnd, &processId); // 获取进程ID

	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId); // 打开进程
	if (hProcess == NULL)
	{
		std::cout << "Error: Unable to open process." << std::endl;
		return -1;
	}

	if (TerminateProcess(hProcess, 0))
	{ // 关闭进程
		std::cout << "Application closed successfully." << std::endl;
	}
	else
	{
		std::cout << "Error: Unable to close application." << std::endl;
	}

	CloseHandle(hProcess); // 关闭进程句柄
}

void DSA::Open_exe(string s)
{
	string applicationPath = s;

	int size_needed = MultiByteToWideChar(CP_UTF8, 0, applicationPath.c_str(), -1, NULL, 0);
	std::wstring wApplicationPath(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, applicationPath.c_str(), -1, &wApplicationPath[0], size_needed);

	HINSTANCE result = ShellExecuteW(NULL, L"open", wApplicationPath.c_str(), NULL, NULL, SW_SHOW);

	if ((int)result <= 32)
		std::cout << "Error: Unable to open application. Error code: " << GetLastError() << std::endl;
	else
		std::cout << "Application opened successfully." << std::endl;
}

MatrixXd DSA::pinv(Eigen::MatrixXd A)
{
	Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
	double  pinvtoler = 1.e-8; //tolerance
	int row = A.rows();
	int col = A.cols();
	int k = min(row, col);
	Eigen::MatrixXd X = Eigen::MatrixXd::Zero(col, row);
	Eigen::MatrixXd singularValues_inv = svd.singularValues();//奇异值
	Eigen::MatrixXd singularValues_inv_mat = Eigen::MatrixXd::Zero(col, row);
	for (long i = 0; i < k; ++i)
	{
		if (singularValues_inv(i) > pinvtoler)
			singularValues_inv(i) = 1.0 / singularValues_inv(i);
		else singularValues_inv(i) = 0;
	}
	for (long i = 0; i < k; ++i)
	{
		singularValues_inv_mat(i, i) = singularValues_inv(i);
	}
	X = (svd.matrixV()) * (singularValues_inv_mat) * (svd.matrixU().transpose());

	return X;
}

void DSA::微分运动学(Matrix4d Target)
{
	fstream f("I:\\DSA\\DK.csv", ios::out);
	int i = 0;

	VectorXd x(7), H_dot(7), joint_min(7), joint_max(7);
	joint_min << -1, -1, -180 / Rad, -180 / Rad, -180 / Rad, -1, -180 / Rad;
	joint_max << 1, 1, 180 / Rad, 180 / Rad, 180 / Rad, 1, 180 / Rad;

	while (y(FK_up(), Target).norm() > 0.0001)
	{
		f << u1 * 1000 << "," << u2 * 1000 << "," << u3 * Rad << ","
			<< u4 * Rad << "," << u5 * Rad << "," << u6 * 1000 << ","
			<< u7 * Rad << "," << y(FK_up(), Target).norm() << "\n";

		auto F = [&](int i, double joint)
		{
			double 分子 = (joint_min(i) + joint_max(i) - 2 * joint) * (joint_max(i) - joint_min(i));
			double 分母 = (joint_max(i) - joint) * (joint - joint_min(i)) * (joint_max(i) - joint) * (joint - joint_min(i));
			return 分子 / 分母;
		};

		H_dot << F(0, u1), F(1, u2), F(2, u3), F(3, u4), F(4, u5), F(5, u6), F(6, u7);

		x = pinv(Jacobian()) * y(FK_up(), Target) - 0.001 * (I7 - pinv(Jacobian()) * Jacobian()) * H_dot;
		//x = pinv(Jacobian()) * y(FK_up(), Target) ;//无零空间

		u1 += x(0);
		u2 += x(1);
		u3 += x(2);
		u4 += x(3);
		u5 += x(4);
		u6 += x(5);
		u7 += x(6);
		i++;
	}

	cout << "次数为：" << i << "\tH为：" << abs(u1) +
		abs(u2) +
		abs(u3) +
		abs(u4) +
		abs(u5) +
		abs(u6) +
		abs(u7) << endl;
	f.close();
	Open_exe("I:\\DSA\\DK.csv");
}

void DSA::Solution()
{
	double theta;
	double theta0 = 29.449820210574603297399709992454 / Rad;

	Vector3d  w7, w5, w4, w3,//旋转关节的方向
		zero, w;//零轴，轴角法的轴

	Matrix4d e3, e4, e5, e7,//变换矩阵
		e31, T;//关节3用于补充的变换矩阵和总体变换矩阵

	zero = Vector3d(0, 0, 0);
	w7 = Vector3d(0, 0, 1);
	w5 = Vector3d(-cos(theta0), sin(theta0), 0);
	w4 = Vector3d(sin(theta0), cos(theta0), 0);
	w3 = Vector3d(0, 0, 1);

	u5 = -acos(cos(d2) / cos(u4));
	u3 = atan2(sin(u5) * sin(u4), cos(u5) + cos(u4) + cos(u5) * cos(u4) - cos(d2));

	e5 = Conversion(w5, u5, zero);
	e4 = Conversion(w4, u4, zero);
	e3 = Conversion(w3, -u3, zero);

	T = e3 * e4 * e5;
	if (T == Matrix4d::Identity())
		u7 = 0;
	else
	{
		//方法1
		Rever_Conversion(T, theta, w);
		u7 = atan2(w(1), w(0));

		//方法2
		//up_7 = Rever_Conversion(T);
	}

	u3 = -u3 - u7 + d1;
	if (u3 > 180 / Rad) u3 -= 360 / Rad;
	if (u3 < -180 / Rad) u3 += 360 / Rad;
	//e31 = Conversion(w3, -up_7 + down_1, zero);

	//e7 = Conversion(w7, up_7, zero);

	//T = e31 * T * e7;

	//求解移动关节
	double h1, h2, h3, h3y, h2y, h1y, h3x, h2x, h1x;//最后三个是h1、2、3的系数
	h1 = 0.208;
	h2 = 0.226;
	h3 = 0.54912;
	h2y = cos(u4) * (cos(theta0) * sin(u3) - sin(theta0) * cos(u3)) + sin(theta0);
	h1y = -sin(u3) * sin(theta0) + cos(theta0) - cos(theta0) * cos(u3);
	h3y = -sin(u5) * (sin(u3) * sin(theta0) + cos(theta0) * cos(u3)) + cos(u5) * sin(u4) * (sin(theta0) * cos(u3) - cos(theta0) * sin(u3));
	h2x = cos(u4) * (cos(theta0) * cos(u3) + sin(theta0) * sin(u3)) - cos(theta0);
	h1x = sin(u3) * cos(theta0) + sin(theta0) - sin(theta0) * cos(u3);
	h3x = sin(u5) * (sin(u3) * cos(theta0) - sin(theta0) * cos(u3)) - cos(u5) * sin(u4) * (cos(theta0) * cos(u3) + sin(theta0) * sin(u3));

	double dz = (1 - cos(u4) * cos(u5)) * h3 - sin(u4) * h2;
	double dy = h1y * h1 + h2y * h2 + h3y * h3;
	double dx = h1x * h1 + h2x * h2 + h3x * h3;

	u6 = -dz / (cos(u4) * cos(u5));
	u1 = -dy - u6 * (sin(u4) * cos(u5) * (-sin(theta0) * cos(u3) + cos(theta0) * sin(u3)) + sin(u5) * (cos(theta0) * cos(u3) + sin(u3) * sin(theta0)));
	u2 = -dx - u6 * (sin(u4) * cos(u5) * (cos(theta0) * cos(u3) + sin(theta0) * sin(u3)) + sin(u5) * (sin(theta0) * cos(u3) - sin(u3) * cos(theta0)));

	//if (up_6 > (25.0374 + 49.3623) / 1000)up_6 = (25.0374 + 49.3623) / 1000;//由于走线问题吃掉了一厘米的安全范围
	//if (up_6 < (-75.2005 - 59.88105) / 1000)up_6 = (-75.2005 - 59.88105) / 1000;

	//if (up_1 > (14.93 + 21.08) / 1000)up_1 = (14.93 + 21.08) / 1000;
	//if (up_1 < (-599.41 - 21.08) / 1000)up_1 = (-599.41 - 21.08) / 1000;

	//if (up_2 > (401.691 + 8.8415) / 1000)up_2 = (401.691 + 8.8415) / 1000;
	//if (up_2 < (-212.626 - 8.8415) / 1000)up_2 = (-212.626 - 8.8415) / 1000;

	//cout << T << endl;
	//cout << "up_1:" << up_1 * 1000 << endl;
	//cout << "up_2:" << up_2 * 1000 << endl;
	//cout << "up_3:" << up_3 * Rad << endl;
	//cout << "up_4:" << up_4 * Rad << endl;
	//cout << "up_5:" << up_5 * Rad << endl;
	//cout << "up_6:" << up_6 * 1000 << endl;
	//cout << "up_7:" << up_7 * Rad << endl;
}

void DSA::Solution_0522()
{
	Matrix3d I = Matrix3d::Identity();
	double theta0 = 29.449820210574603297399709992454 / Rad;
	Matrix3d R = target.block<3, 3>(0, 0);

	//********************** 目标 **********************
	Vector3d rr4 = Vector3d(sin(theta0), cos(theta0), 0);//关节4

	Matrix3d R1 = I;
	Matrix3d R2 = Rot(rr4, u4);
	Matrix3d R3 = I;
	Matrix3d R4 = I;

	Vector3d rr1 = Vector3d(0, 0, 1);//关节3
	Vector3d rr2 = Vector3d(-cos(theta0), sin(theta0), 0);//关节5
	Vector3d rr3 = Vector3d(0, 0, 1);//关节7

	//********************** 稍微处理一下 **********************
	R = R2.transpose() * R1.transpose() * R * R4.transpose() * R3.transpose();

	Vector3d r1 = R2.transpose() * rr1;
	Vector3d r2 = rr2;
	Vector3d r3 = R3 * rr3;

	//********************** 欧拉角分解 **********************
	double a = -r1.transpose() * hat(r2) * hat(r2) * r3;
	double b = r1.transpose() * hat(r2) * r3;
	double c = r1.transpose() * (R - I - hat(r2) * hat(r2)) * r3;

	u5 = atan2(b, a) - atan2(pow(a * a + b * b - c * c, 0.5), c);
	//u5 = atan2(b, a) + atan2(pow(a * a + b * b - c * c, 0.5), c);

	Vector3d v1 = Rot(r2, u5) * r3;
	Vector3d w1 = R * r3;
	Vector3d v3 = Rot(r2, -u5) * r1;
	Vector3d w3 = R.transpose() * r1;

	u3 = atan2(w1.transpose() * r1.cross(v1), v1.transpose() * w1 - double(v1.transpose() * r1 * w1.transpose() * r1));

	//连续轨迹的时候u3要连续变化
	{
		double super_low = u3 - 360.0 * 2 / Rad;
		double low = u3 - 360.0 / Rad;
		double high = u3 + 360.0 / Rad;
		double super_high = u3 + 360.0 * 2 / Rad;
		double dis_super_low = abs(u3f - super_low);
		double dis_low = abs(u3f - low);
		double dis_high = abs(u3f - high);
		double dis_super_high = abs(u3f - super_high);
		double dis = abs(u3f - u3);
		double probability[5] = { dis_super_low ,dis_low ,dis_high,dis,dis_super_high };
		sort(probability, probability + sizeof(probability) / sizeof(probability[0]));
		if (probability[0] == dis_super_low)u3 = u3 - 360.0 * 2 / Rad;
		else if (probability[0] == dis_low)u3 = u3 - 360.0 / Rad;
		else if (probability[0] == dis)u3 = u3;
		else if (probability[0] == dis_high)u3 = u3 + 360.0 / Rad;
		else if (probability[0] == dis_super_high)u3 = u3 + 360.0 * 2 / Rad;
		u3f = u3;
	}

	u7 = -atan2(w3.transpose() * r3.cross(v3),
		v3.transpose() * w3 - double(v3.transpose() * r3 * w3.transpose() * r3));

	//if (abs(u7 - u7f) * Rad > 0.3)
	//	u7 = u7f + 0.3 * (u7 - u7f) / abs(u7 - u7f) / Rad;

	//连续轨迹的时候u7也要连续变化，这个功能到它们把线穿到里面再说
	{
		double super_low = u7 - 360 * 2 / Rad;
		double low = u7 - 360 / Rad;
		double high = u7 + 360 / Rad;
		double super_high = u7 + 360 * 2 / Rad;
		double dis_super_low = abs(u7f - super_low);
		double dis_low = abs(u7f - low);
		double dis_high = abs(u7f - high);
		double dis_super_high = abs(u7f - super_high);
		double dis = abs(u7f - u7);
		double probability[5] = { dis_super_low ,dis_low ,dis_high,dis,dis_super_high };
		sort(probability, probability + sizeof(probability) / sizeof(probability[0]));
		if (probability[0] == dis_super_low)u7 = u7 - 360 * 2 / Rad;
		else if (probability[0] == dis_low)u7 = u7 - 360 / Rad;
		else if (probability[0] == dis)u7 = u7;
		else if (probability[0] == dis_high)u7 = u7 + 360 / Rad;
		else if (probability[0] == dis_super_high)u7 = u7 + 360 * 2 / Rad;
		u7f = u7;
	}

	//********************** R chain计算完毕 **********************

	Vector3d rrr5 = Vector3d(-0.1968, 0.11112, 0.54912);
	Vector3d rrr4 = Vector3d(-0.094533, 0.29224, 0.54912);
	Vector3d rrr3 = Vector3d(-0.094533, 0.29224, 0.54912);

	Vector3d vv5 = -rr2.cross(rrr5);
	Vector3d vv4 = -rr4.cross(rrr4);
	Vector3d vv3 = -rr1.cross(rrr3);

	Vector3d Poffset = (I - Rot(rr1, u3)) * rr1.cross(vv3) +
		Rot(rr1, u3) * (I - Rot(rr4, u4)) * rr4.cross(vv4) +
		Rot(rr1, u3) * Rot(rr4, u4) * (I - Rot(rr2, u5)) * rr2.cross(vv5);

	Vector3d P = target.block<3, 1>(0, 3);
	Vector3d v6 = Rot(rr1, u3) * Rot(rr4, u4) * Rot(rr2, u5) * Vector3d(0, 0, 1);

	Matrix3d V = Matrix3d::Identity();
	V << 0, 1, -v6(1) / v6(2),
		1, 0, -v6(0) / v6(2),
		0, 0, 1 / v6(2);

	u1 = (V * (P - Poffset))(0);
	u2 = (V * (P - Poffset))(1);
	u6 = (V * (P - Poffset))(2);
}

void DSA::Solution_redundant4_change()
{
	double theta0 = 29.449820210574603297399709992454 / Rad;

	x = atan2(pow(1 - target(2, 2) * target(2, 2), 0.5), target(2, 2));
	//x = atan2(-pow(1 - target(2, 2) * target(2, 2), 0.5), target(2, 2));

	if (sin(x) > 0)
	{
		z1 = atan2(target(0, 2), -target(1, 2));
		z2 = atan2(target(2, 0), target(2, 1));
	}
	else
	{
		z1 = atan2(target(0, 2), target(1, 2));
		z2 = atan2(-target(2, 0), -target(2, 1));
	}

	u5 = -acos(cos(x) / cos(u4));
	//u5 = acos(cos(x) / cos(u4));

	if (z1 * Rad > 90 + theta0 * Rad)		z1 -= 360 / Rad;//为了能够连续

	u3 = 0.5 * atan2(-sin(u5) * sin(u4), cos(u4) * cos(u5) + cos(u4) + cos(u5) - cos(x))
		- atan2(sin(u5) * (1 + cos(u4)), sin(u4) * (1 + cos(u5)))
		+ z1;

	u7 = atan2(sin(u5) * (1 + cos(u4)), sin(u4) * (1 + cos(u5))) +
		0.5 * atan2(-sin(u5) * sin(u4), cos(u4) * cos(u5) + cos(u4) + cos(u5) - cos(x))
		+ z2 + 90 / Rad - theta0;

	double l1 = 0.208;
	double l2 = 0.226;
	double l3 = 0.54912;

	double X1 = l1 * (1 - cos(u3)) + l2 * sin(u3);
	double Y1 = l2 * (1 - cos(u3)) - l1 * sin(u3);
	double Y2 = l2 * (1 - cos(u4)) + l3 * sin(u4);
	double Z2 = l3 * (1 - cos(u4)) - l2 * sin(u4);
	double X3 = -l3 * sin(u5);
	double Z3 = l3 * (1 - cos(u5));

	double dx = cos(u3) * X3 + Z3 * sin(u3) * sin(u4) - sin(u3) * Y2 + X1;
	double dy = sin(u3) * X3 - sin(u4) * cos(u3) * Z3 + cos(u3) * Y2 + Y1;
	double dz = l3 * (1 - cos(u4) * cos(u5)) - l2 * sin(u4);

	Vector3d w5 = Vector3d(0, 1, 0);
	Vector3d w4 = Vector3d(1, 0, 0);
	Vector3d w3 = Vector3d(0, 0, 1);
	Vector3d zero = Vector3d(0, 0, 0);
	Vector3d r5 = Vector3d(0, 0, l3);
	Vector3d r4 = Vector3d(0, l2, l3);
	Vector3d r3 = Vector3d(l1, l2, 0);

	Matrix4d e5 = Conversion(w5, u5, r5);
	Matrix4d e4 = Conversion(w4, u4, r4);
	Matrix4d e3 = Conversion(w3, u3, r3);

	double aaa = target(2, 3) - dz;
	double aab = (e3 * e4 * e5)(2, 2);

	u6 = (target(2, 3) - dz) / ((e3 * e4 * e5)(2, 2));

	u1 = ((-dy + target(1, 3) - u6 * ((e3 * e4 * e5)(1, 2))) / cos(theta0) + (-dx + target(0, 3) - u6 * ((e3 * e4 * e5)(0, 2))) / sin(theta0))
		/ (tan(theta0) + 1 / tan(theta0));

	u2 = (-dx + target(0, 3) - u6 * ((e3 * e4 * e5)(0, 2)) - cos(theta0) * u1) / sin(theta0);
}


void DSA::test_231205_做运动学参数标定()
{
	//算target
	target = Conversion(Vector3d(0, 0, 1), d1, Vector3d(0, 0, 0)) *
		Conversion(Vector3d(1, 0, 0), d2, Vector3d(0, 0, 0));

	//算u4
	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;

	double y = d2 * Rad;

	double z0 = -0.03304;
	double A1 = -3.53463E-7;
	double A2 = -5.89886E-9;
	double A3 = 1.54483E-12;
	double A4 = 4.83649E-14;
	double A5 = 1.61074E-17;
	double B1 = 0.77359;
	double B2 = -0.00992;
	double B3 = 6.04849E-4;
	double B4 = -1.43371E-5;
	double B5 = 1.30915E-7;

	u4 = (z0 + A1 * x + A2 * pow(x, 2) + A3 * pow(x, 3) + A4 * pow(x, 4) + A5 * pow(x, 5) +
		B1 * y + B2 * pow(y, 2) + B3 * pow(y, 3) + B4 * pow(y, 4) + B5 * pow(y, 5)) / Rad;

	if (d2 * Rad < 2)
	{
		double rate = Polynomial3(d2, 2 / Rad, 1, 0.00064 / Rad, 0, 0, 0);
		u4 = (d2 * 33.290015625 / 45.0) * (1 - rate) + (u4)*rate;
	}

	if (abs(u4) > abs(d2))
		u4 = u4 / abs(u4) * abs(d2);
}

void DSA::计算d1与d2与target与u4(bool 是否限制速度)
{
	//************** d2速度位置限制 *************
	if (是否限制速度)
	{
		// 奇异区减速：下臂关节二越接近0越接近奇异位形，单周期步长上限随之收紧。
		// 这段只在“是否限制速度 == true”（即手柄微调）时生效，
		// 初始回零与补偿初始过渡走各自的速度规划，不受影响。
		const double d2_deg = abs(d2f) * Rad;        // 用上一周期值判断，避免临界抖动
		const double 奇异区阈值_deg = 5.0;            // d2 < 5° 进入减速区
		const double 常规步长上限_deg = 0.10;         // 原本的手柄速度上限
		const double 奇异区最小步长_deg = 0.001;       // 下限不能取0，否则卡在奇异点退不出来

		double 步长上限_deg = 常规步长上限_deg;
		if (d2_deg < 奇异区阈值_deg)
			步长上限_deg = 奇异区最小步长_deg +
				(常规步长上限_deg - 奇异区最小步长_deg) * d2_deg / 奇异区阈值_deg;

		if (abs(d2 - d2f) * Rad > 步长上限_deg)
			d2 = d2f + 步长上限_deg * (d2 - d2f) / abs(d2 - d2f) / Rad;
	}
	clamp(d2, 0.0 / Rad, 45.0 / Rad);

	//************** d1奇异处理、速度位置限制 *************
	if (abs(d1 - d1f) * Rad > 182)
	{
		double super_low = d1 - 360 * 2 / Rad;
		double low = d1 - 360 / Rad;
		double high = d1 + 360 / Rad;
		double super_high = d1 + 360 * 2 / Rad;

		double dis_super_low = abs(d1f - super_low);
		double dis_low = abs(d1f - low);
		double dis_high = abs(d1f - high);
		double dis_super_high = abs(d1f - super_high);
		double dis = abs(d1f - d1);

		double probability[5] = { dis_super_low ,dis_low ,dis_high,dis,dis_super_high };
		sort(probability, probability + sizeof(probability) / sizeof(probability[0]));

		if (probability[0] == dis_super_low)d1 = d1 - 360 * 2 / Rad;
		else if (probability[0] == dis_low)d1 = d1 - 360 / Rad;
		else if (probability[0] == dis)d1 = d1;
		else if (probability[0] == dis_high)d1 = d1 + 360 / Rad;
		else if (probability[0] == dis_super_high)d1 = d1 + 360 * 2 / Rad;
	}
	if (d1 > 359 / Rad)
	{
		d1 = 359 / Rad;
		h = atan2(sin(d2) * cos(d1), cos(d2)) * Rad;
		r = atan2(sin(d1) * sin(d2) * cos(h / Rad), cos(d2)) * Rad;
	}
	else if (d1 < -359 / Rad)
	{
		d1 = -359 / Rad;
		h = atan2(sin(d2) * cos(d1), cos(d2)) * Rad;
		r = atan2(sin(d1) * sin(d2) * cos(h / Rad), cos(d2)) * Rad;
	}
	if (是否限制速度)
	{
		if (abs(d1 - d1f) * Rad > 0.1)
			d1 = d1f + 0.1 * (d1 - d1f) / abs(d1 - d1f) / Rad;
	
	}
	else
	{
		if (abs(d1 - d1f) * Rad > 1.0)
			d1 = d1f + 1.0 * (d1 - d1f) / abs(d1 - d1f) / Rad;
	}

	//************** target计算 *************
	// 下臂仍走原来的d1、d2，用辨识模型预测其位姿，再安排上臂。
	const Matrix4d F_lower_hat = 现场旋量指数(xi_lower_comp.col(0), d1) *
		现场旋量指数(xi_lower_comp.col(1), d2);
	Matrix4d Y_star = Conversion(Vector3d(0, 0, 1), -d1, Vector3d(0, 0, 0));
	const double z0 = 0; // 以原现场初始间距为高度0，保留原来的反向Z旋转关系。
	Y_star(2, 3) = z0;
	target = F_lower_hat * Y_star * C_control_comp.inverse();

	//************** u4计算 *************
	u4 = d2 * 0.9;

	//************** 数据更新 *************
	d2f = d2;
	d1f = d1;
}

void DSA::Solution_Redundant_输入旋量坐标(Eigen::MatrixXd xi)
{
	Eigen::Matrix3d R = target.block<3, 3>(0, 0);

	Eigen::Matrix3d I3 = Eigen::Matrix3d::Identity();

	auto unit = [](const Eigen::Vector3d& a)->Eigen::Vector3d {
		long double n = a.norm();
		if (n < 1e-20) return a;
		return a / n;
	};

	// ===================== 归一化 =====================
	Eigen::Vector3d w3 = xi.col(2).head<3>();
	Eigen::Vector3d w4 = xi.col(3).head<3>();
	Eigen::Vector3d w5 = xi.col(4).head<3>();
	Eigen::Vector3d w7 = xi.col(6).head<3>();

	Eigen::Vector3d v3 = xi.col(2).tail<3>();
	Eigen::Vector3d v4 = xi.col(3).tail<3>();
	Eigen::Vector3d v5 = xi.col(4).tail<3>();
	Eigen::Vector3d v7 = xi.col(6).tail<3>();

	Eigen::Vector3d v1 = xi.col(0).tail<3>();
	Eigen::Vector3d v2 = xi.col(1).tail<3>();
	Eigen::Vector3d v6 = xi.col(5).tail<3>();

	// ===================== R =====================

	long double α = -w3.transpose() * Rot(w4, u4) * hat(w5) * hat(w5) * w7;

	long double β = w3.transpose() * Rot(w4, u4) * hat(w5) * w7;

	long double γ = w3.transpose() * Rot(w4, u4) * (Rot(w4, -u4) * R - I3 - hat(w5) * hat(w5)) * w7;

	long double disc = α * α + β * β - γ * γ;
	if (disc < 0.0) disc = 0.0; // 数值保护
	u5 = atan2(β, α) - atan2(std::sqrt(disc), γ);

	Eigen::Vector3d V1 = Rot(w5, u5) * w7;
	Eigen::Vector3d U1 = Rot(w4, -u4) * R * w7;
	Eigen::Vector3d V4 = Rot(w5, -u5) * Rot(w4, -u4) * w3;
	Eigen::Vector3d U4 = R.transpose() * w3;

	u3 = atan2(U1.transpose() * (Rot(w4, -u4) * w3).cross(V1), V1.transpose() * U1 - double(V1.transpose() * Rot(w4, -u4) * w3 * U1.transpose() * Rot(w4, -u4) * w3));

	////连续轨迹的时候u3要连续变化
	{
		double super_low = u3 - 360.0 * 2 / Rad;
		double low = u3 - 360.0 / Rad;
		double high = u3 + 360.0 / Rad;
		double super_high = u3 + 360.0 * 2 / Rad;
		double dis_super_low = abs(u3f - super_low);
		double dis_low = abs(u3f - low);
		double dis_high = abs(u3f - high);
		double dis_super_high = abs(u3f - super_high);
		double dis = abs(u3f - u3);
		double probability[5] = { dis_super_low ,dis_low ,dis_high,dis,dis_super_high };
		sort(probability, probability + sizeof(probability) / sizeof(probability[0]));
		if (probability[0] == dis_super_low)u3 = u3 - 360.0 * 2 / Rad;
		else if (probability[0] == dis_low)u3 = u3 - 360.0 / Rad;
		else if (probability[0] == dis)u3 = u3;
		else if (probability[0] == dis_high)u3 = u3 + 360.0 / Rad;
		else if (probability[0] == dis_super_high)u3 = u3 + 360.0 * 2 / Rad;
		u3f = u3;
	}
	u7 = -atan2(U4.transpose() * w7.cross(V4), V4.transpose() * U4 - double(V4.transpose() * w7 * U4.transpose() * w7));

	//连续轨迹的时候u7也要连续变化，这个功能到它们把线穿到里面再说
	{
		double super_low = u7 - 360 * 2 / Rad;
		double low = u7 - 360 / Rad;
		double high = u7 + 360 / Rad;
		double super_high = u7 + 360 * 2 / Rad;
		double dis_super_low = abs(u7f - super_low);
		double dis_low = abs(u7f - low);
		double dis_high = abs(u7f - high);
		double dis_super_high = abs(u7f - super_high);
		double dis = abs(u7f - u7);
		double probability[5] = { dis_super_low ,dis_low ,dis_high,dis,dis_super_high };
		sort(probability, probability + sizeof(probability) / sizeof(probability[0]));
		if (probability[0] == dis_super_low)u7 = u7 - 360 * 2 / Rad;
		else if (probability[0] == dis_low)u7 = u7 - 360 / Rad;
		else if (probability[0] == dis)u7 = u7;
		else if (probability[0] == dis_high)u7 = u7 + 360 / Rad;
		else if (probability[0] == dis_super_high)u7 = u7 + 360 * 2 / Rad;
		u7f = u7;
	}
	// ===================== P =====================
	Eigen::Vector3d Poffset =
		(I3 - Rot(w3, u3)) * w3.cross(v3) +
		Rot(w3, u3) * (I3 - Rot(w4, u4)) * w4.cross(v4) +
		Rot(w3, u3) * Rot(w4, u4) * (I3 - Rot(w5, u5)) * w5.cross(v5) +
		Rot(w3, u3) * Rot(w4, u4) * Rot(w5, u5) * (I3 - Rot(w7, u7)) * w7.cross(v7);

	Eigen::Matrix3d M;
	M.col(0) = v1;
	M.col(1) = v2;
	M.col(2) = Rot(w3, u3) * Rot(w4, u4) * Rot(w5, u5) * v6;

	Eigen::Vector3d sol = M.colPivHouseholderQr().solve(target.block<3, 1>(0, 3) - Poffset);

	u1 = sol(0);
	u2 = sol(1);
	u6 = sol(2);
}

double DSA::joint_err_value1_5()
{
	double cfct[] = { -28.62938,0.27367,5.80074,-2.63488E-4,-0.00229,-0.78835,-1.37202E-5,4.23095E-6,
		-1.4993E-5,0.04358,1.05209E-8,1.53904E-7,-4.42976E-8,-4.10033E-7,-0.00103,1.46021E-10,-4.74778E-10,
		-1.99009E-9,2.28087E-9,2.74035E-8,8.68837E-6 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;

	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / 1000;
}

double DSA::joint_err_value2_5()
{
	double cfct[] = { 45.23223,-0.0039,4.38952,-0.00212,0.00557,-0.67567,-1.83148E-6,4.15906E-5,-7.63927E-4,
		0.03853,4.1578E-8,3.11196E-7,-1.29531E-6,2.33111E-5,-9.22151E-4,1.28334E-11,-6.36762E-10,-5.13598E-9,
		2.16183E-8,-2.15044E-7,7.80922E-6 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;
	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / 1000;
}

double DSA::joint_err_value3_5()
{
	double cfct[] = { -0.33007,-0.00639,0.03415,2.98144E-5,-1.04201E-4,0.00104,2.93684E-7,-1.19547E-7,
		-8.75489E-6,-9.56985E-5,-5.03206E-10,4.21478E-9,2.16208E-8,3.62403E-7,2.37601E-6,-3.54318E-12,
		-9.14539E-12,-1.85348E-11,-1.97208E-10,-3.86567E-9,-1.93367E-8 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;
	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / Rad;
}

double DSA::joint_err_value4_5()
{
	double cfct[] = { 2.30681,-0.00312,-0.03817,-1.9724E-4,-4.23104E-4,0.00381,3.84468E-7,-3.65057E-7,
		4.05548E-5,-1.98159E-4,3.05297E-9,-1.44991E-9,-9.94416E-9,-1.00356E-6,4.19406E-6,-8.75534E-12,
		2.46203E-11,-2.17055E-10,1.28134E-10,1.02702E-8,-3.346E-8 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;
	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / Rad;
}

double DSA::joint_err_value5_5()
{
	double cfct[] = { 2.71222,-0.02263,0.06469,8.79108E-6,-3.05873E-4,-0.00619,8.51778E-7,-5.85433E-7,
		3.07852E-5,1.86481E-4,-7.8389E-10,-3.87496E-9,-6.37797E-10,-7.40606E-7,-2.19036E-6,-4.52957E-12,
		3.73758E-11,-1.18941E-10,-1.20654E-10,7.3739E-9,4.88026E-9 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;
	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / Rad;
}

double DSA::joint_err_value6_5()
{
	double cfct[] = { -0.32712,-0.0139,-0.62541,-8.46981E-4,-0.00203,0.05441,1.86052E-6,2.94447E-5,3.03721E-5,
		-0.00279,1.31914E-8,1.41004E-8,-4.48486E-7,-9.77704E-7,6.16021E-5,-4.2842E-11,-3.69885E-10,1.1147E-9,
		8.51934E-9,4.87654E-9,-5.06361E-7 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;
	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / 1000;
}

double DSA::joint_err_value7_5()
{
	double cfct[] = { 0.09208,0.01169,-0.04665,1.23547E-5,2.30918E-4,0.00228,-7.21804E-7,-4.73179E-7,5.85118E-7,
		-1.34566E-4,-1.1574E-10,-1.9335E-9,3.28002E-8,1.85744E-8,3.50678E-6,1.04054E-11,4.83828E-12,-1.83243E-10,
		-5.25928E-10,6.05387E-10,-3.12975E-8 };

	double x;

	if (d1 * Rad > 180)x = d1 * Rad - 360;
	else if (d1 * Rad < -180)x = d1 * Rad + 360;
	else x = d1 * Rad;
	double y = d2 * Rad;

	double z =
		cfct[0] +
		cfct[1] * x + cfct[2] * y +
		cfct[3] * pow(x, 2) + cfct[4] * x * y + cfct[5] * pow(y, 2) +
		cfct[6] * pow(x, 3) + cfct[7] * pow(x, 2) * y + cfct[8] * pow(y, 2) * x + cfct[9] * pow(y, 3) +
		cfct[10] * pow(x, 4) + cfct[11] * pow(x, 3) * y + cfct[12] * pow(y, 2) * pow(x, 2) + cfct[13] * pow(y, 3) * x + cfct[14] * pow(y, 4) +
		cfct[15] * pow(x, 5) + cfct[16] * pow(x, 4) * y + cfct[17] * pow(y, 2) * pow(x, 3) + cfct[18] * pow(y, 3) * pow(x, 2) + cfct[19] * pow(y, 4) * x + cfct[20] * pow(y, 5);

	return z / Rad;
}



std::string DSA::format_time_to_string(std::chrono::system_clock::time_point& time_point)
{
	// Convert to time_t using system_clock
	std::time_t time = std::chrono::system_clock::to_time_t(time_point);

	// Convert to local time
	std::tm bt;
#if defined(__unix__) || defined(__APPLE__)
	localtime_r(&time, &bt);
#elif defined(_MSC_VER)
	localtime_s(&bt, &time);
#else
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	bt = *std::localtime(&time);
#endif

	// Format the time to a string
	std::ostringstream oss;
	oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

std::string DSA::format_time_to_string_简易(std::chrono::system_clock::time_point& time_point)
{
	// Convert to time_t using system_clock
	std::time_t time = std::chrono::system_clock::to_time_t(time_point);

	// Get milliseconds part
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()) % 1000;

	// Convert to local time
	std::tm bt;
#if defined(__unix__) || defined(__APPLE__)
	localtime_r(&time, &bt);
#elif defined(_MSC_VER)
	localtime_s(&bt, &time);
#else
	static std::mutex mtx;
	std::lock_guard<std::mutex> lock(mtx);
	bt = *std::localtime(&time);
#endif

	// Format the time to a string with milliseconds
	std::ostringstream oss;
	oss << std::put_time(&bt, "%d %H:%M:%S");
	oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

	return oss.str();
}

void DSA::运动至目标体位(double end1, double end2)
{
	a.ADSRead((char*)"G.temp2", sizeof(关节编码器值), &关节编码器值);

	begin1 = 关节编码器值[7] / Rad;
	begin2 = (关节编码器值[8]) / Rad;
	end1 = end1 / Rad;
	end2 = end2 / Rad;

	times = 1;

	path_number = int(abs(关节编码器值[7] - end1) * 25.0 + abs(关节编码器值[8] - end2) * 40.0);

	if (path_number < 900)
		path_number = 900;

	while (times <= path_number)
	{
		//读取数据
		a.ADSRead((char*)"G.temp2", sizeof(关节编码器值), &关节编码器值);

		// 碰撞后丢弃这次未走完的体位指令，交回手柄流程对齐锁存任务。
		if (!a.ADSRead((char*)"G.collision_state", sizeof(collision_state), &collision_state) || collision_state != 0)
			return;

		关节编码器值[8] = 关节编码器值[8];

		Polynomial3(times, d1, 1, begin1, path_number, end1, 0.0002, 0);
		Polynomial3(times, d2, 1, begin2, path_number, end2, 0.0002, 0);

		//处理d1 处理d2 算target 算u4
		计算d1与d2与target与u4(false);//计算d1、d2、target、u4

		Solution_Redundant_输入旋量坐标(xi_upper_comp);

		target_joint[0] = u1 * 1000;
		target_joint[1] = u2 * 1000;
		target_joint[2] = u3 * Rad;
		target_joint[3] = u4 * Rad;
		target_joint[4] = u5 * Rad;
		target_joint[5] = u6 * 1000 + p6;
		target_joint[6] = u7 * Rad;
		target_joint[7] = d1 * Rad;
		target_joint[8] = d2 * Rad;

		//发送数据
		a.ADSWrite((char*)"G.temp", sizeof(target_joint), &target_joint);

		times += 1;
	}

	//为了衔接手柄
	h = atan2(sin(d2) * cos(d1), cos(d2)) * Rad;
	r = atan2(sin(d1) * sin(d2) * cos(h / Rad), cos(d2)) * Rad;
}


int DSA::手柄微调运动(HANDLE g_hMutex)
{
	if (control_state == 0) return 1; // 尚未按现场启动按钮，不发补偿运动。
	if (!compensation_initialized)
	{
		bool handle_ready = false;
		if (!a.ADSRead((char*)"G.handle_ready", sizeof(handle_ready), &handle_ready) || !handle_ready)
			return 1; // 等待原PLC回零流程结束。
	}
	//读取数据
	bool position_read_ok = a.ADSRead((char*)"G.temp2", sizeof(关节编码器值), &关节编码器值);
	if (!a.ADSRead((char*)"G.collision_state", sizeof(collision_state), &collision_state))
		return 1; // 本周期不发新目标；保留已有任务。
	if (!position_read_ok) return 1; // 补偿初始化及关节6增量均依赖实际位置。
	if (!compensation_initialized)
		建立补偿初始参考();

	if (collision_state == 1 && collision_state_f != 1)
	{
		double collision_target[9] = { 0 };
		if (!a.ADSRead((char*)"G.collision_target", sizeof(collision_target), &collision_target))
			return 1;
		// 用 PLC 触发时的任务恢复 h、r，丢弃通信延迟期间多算的增量。
		d1 = d1f = collision_target[7] / Rad;
		d2 = d2f = collision_target[8] / Rad;
		h = atan2(sin(d2) * cos(d1), cos(d2)) * Rad;
		r = atan2(sin(d1) * sin(d2) * cos(h / Rad), cos(d2)) * Rad;
		collision_task_latched = true;
		启停阶段速度限制的分母 = 30;
	}
	collision_state_f = collision_state;

	number += 1;

	if (number % 10 == 0)
	{
		auto now = std::chrono::system_clock::now();

		auto duration = now.time_since_epoch();

		// 将时间长度转换为微秒
		auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

		long long mt = microseconds - m;

		std::cout << mt << "\t"
			<< sqrt(pow(关节编码器值[8] - 关节编码器值f[8], 2) + pow(关节编码器值[7] - 关节编码器值f[7], 2)) << "\t"
			<< sqrt(pow(关节编码器值[8] - 关节编码器值f[8], 2) + pow(关节编码器值[7] - 关节编码器值f[7], 2)) * 100000 / mt << std::endl;

		m = microseconds;

		for (int i = 0; i < 10; i++)
			关节编码器值f[i] = 关节编码器值[i];
	}

	//及时停止的逻辑
	WaitForSingleObject(g_hMutex, INFINITE);
	bool motion_neutral = vh == 0 && vr == 0 && v6 == 0 && 手柄盘上的按钮 == 0;
	if (collision_state != 0 || !compensation_ready)
		collision_wait_neutral = true;
	bool block_input = collision_state != 0 || collision_wait_neutral || !compensation_ready;
	if (compensation_ready && collision_state == 0 && motion_neutral)
		collision_wait_neutral = false; // 本周期仍不接输入，下一次运动才是新指令。

	if (!block_input)
	{
		if (手柄把上的按钮 == 1 && (vh != 0 || vr != 0))
		{
			//****************** 启停阶段的目标速度约束 ******************
			if (0 != 启停阶段速度限制的分母)
			{
				vh = vh / 启停阶段速度限制的分母;
				vr = vr / 启停阶段速度限制的分母;
				启停阶段速度限制的分母 -= 1.0;
			}
			else if (启停阶段速度限制的分母<0)
				启停阶段速度限制的分母 = 0.0;

			collision_task_latched = false;
			h += vh;
			r += vr;

			//****************** 目标位置约束 ******************
			clamp(h, -46.0, 46.0);
			clamp(r, -46.0, 46.0);

			d1 = atan2(cos(h / Rad) * sin(r / Rad), sin(h / Rad));
			d2 = acos(cos(r / Rad) * cos(h / Rad));
		}
		else if (0 == 手柄把上的按钮 && v6 == 0 && !collision_task_latched)
		{
			d1 = 关节编码器值[7] / Rad;
			d2 = 关节编码器值[8] / Rad;

			h = atan2(sin(d2) * cos(d1), cos(d2)) * Rad;
			r = atan2(sin(d1) * sin(d2) * cos(h / Rad), cos(d2)) * Rad;

			启停阶段速度限制的分母 = 30;
		}

	}
	const double joint6_increment = block_input ? 0.0 : v6;

	ReleaseMutex(g_hMutex);

	计算d1与d2与target与u4(true);//计算d1、d2、target、u4

	Solution_Redundant_输入旋量坐标(xi_upper_comp);

	target_joint[0] = u1 * 1000;
	target_joint[1] = u2 * 1000;
	target_joint[2] = u3 * Rad;
	target_joint[3] = u4 * Rad;
	target_joint[4] = u5 * Rad;
	// 保持时只重接关节6的实际位置；h、r仍由锁存任务经原逆解得到。
	if (compensation_ready && collision_state == 1)
		p6 = 关节编码器值[5] - u6 * 1000;
	else if (compensation_ready && joint6_increment != 0)
		p6 = 关节编码器值[5] + joint6_increment - u6 * 1000; // 基于下位机实际关节6增量
	target_joint[5] = u6 * 1000+ p6;
	target_joint[6] = u7 * Rad;
	target_joint[7] = d1 * Rad;
	target_joint[8] = d2 * Rad;

	if (!compensation_ready)
		补偿初始点过渡();

	//发送数据
	bool target_written = a.ADSWrite((char*)"G.temp", sizeof(target_joint), &target_joint);
	if (collision_state == 1)
	{
		// 必须先发送对齐后的目标，再允许 PLC 进入原来的缓恢复。
		bool resume_ready = motion_neutral && position_read_ok && target_written;
		a.ADSWrite((char*)"G.collision_resume_ready", sizeof(resume_ready), &resume_ready);
	}

	//记录
	//current_time = std::chrono::system_clock::now();

	//fstream ffff("测试1.csv", ios::out);
	////ffff << format_time_to_string_简易(start) << ",";
	////记录编码器的数据
	//for (int i = 0; i < 9; i++)
	//	ffff << 关节编码器值[i] << ",";
	//ffff << h << "," << r << "," << d1 << "," << d2 << ",";
	////记录手柄给出的数据
	//ffff << vh << "," << vr << "," << v6 << "," << 手柄把上的按钮 << ",";
	////记录发给倍福的数据
	//for (int i = 0; i < 9; i++)
	//	ffff << target_joint[i] << ",";
	//ffff << endl;

	return 1;
}

int DSA::手柄信息获取(HANDLE g_hMutex, int 通信周期, double rate_v)
{
	int err = cc.Recv(&frame, 通信周期);

	if (err == COPLEYCAN_ERR_TIMEOUT)
	{
		vh = 0;
		vr = 0;
		return 1;//continue
	}
	else if (err)
	{
		printf("Error 0x%04x waiting on message\n", err);
		return 0;//break
	}
	else if (frame.type == CAN_FRAME_ERROR)
	{
		CanErrorInfo* errInfo = (CanErrorInfo*)&frame;
		printf("Error information frame received.  Error type 0x%08x, tx err count: %d, rx err count: %d\n",
			errInfo->mask, errInfo->data[0], errInfo->data[1]);
	}
	else if (frame.type == CAN_FRAME_REMOTE)
		printf("Remote request, ID: 0x%08x, data count: %d", frame.id, frame.length);
	else if (frame.id == 0x00000012)
	{
		WaitForSingleObject(g_hMutex, INFINITE);//等待g_hMutex锁，等待时间为无限
		//cout << frame.data[1]-128 << endl;//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		switch (frame.data[1] - 128)
		{
		case 0:
			vr = 0;	vh = 0; 手柄把上的按钮 = 1;	break;//中心位置按住


		case -128:
			vr = 0;	vh = 0; 手柄把上的按钮 = 0;	break;//松手
		case 1:
			vr = -frame.data[3] / rate_v; 手柄把上的按钮 = 1;	/*cout << "1" << endl;*/ break;

			
		case 2:
			vr = frame.data[3] / rate_v; 手柄把上的按钮 = 1;	/*cout << "2" << endl;*/ break;

		case 4:
			vh = frame.data[2] / rate_v; 手柄把上的按钮 = 1;	/*cout << "4" << endl;*/ break;
		case 8:
			vh = -frame.data[2] / rate_v; 手柄把上的按钮 = 1;	/*cout << "8" << endl;*/ break;
		case 5:
			vr = -frame.data[3] / rate_v;	vh = frame.data[2] / rate_v; 手柄把上的按钮 = 1;	break;
		case 9:
			vr = -frame.data[3] / rate_v;	vh = -frame.data[2] / rate_v; 手柄把上的按钮 = 1;	break;
		case 6:
			vr = frame.data[3] / rate_v;	vh = frame.data[2] / rate_v; 手柄把上的按钮 = 1;	break;
		case 10:
			vr = frame.data[3] / rate_v;	vh = -frame.data[2] / rate_v; 手柄把上的按钮 = 1;	break;
		}

		if (int(frame.data[4]) == 2)
		{
			v6 = -2.0;
		}
		else if (int(frame.data[4]) == 1)
		{
			v6 = 2.0;
		}
		else v6 = 0;

		手柄盘上的按钮 = int(frame.data[5]);//记录目标体位

		ReleaseMutex(g_hMutex);
	}

	return 2;//正常
}
