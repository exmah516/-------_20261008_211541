#pragma once
#include <windows.h>
#define Rad  57.29577951308232

struct USERData
{
	unsigned char buttons2;
	long encoders2[2];
};

class Handle
{
public:
	explicit Handle(DWORD serial = 582);
	bool bQuit = false;
	bool bForceEnable = false;
	bool bPause = false;
	int iID1 = -1;
	int iFnt = 2;
	USERData objData;
	double fVels2[2], fJoints2[2];
	unsigned char buttons2;
	long encoders2[2];

	bool init();
	bool init(DWORD serial);
	bool poll();

	void clear_cache();
	void close();

	void setforce(double F, double N);

	void setforce_axis(double F, int axis, double N);

	DWORD serial() const
	{
		return serial_number_;
	}

	bool is_open() const
	{
		return iID1 >= 0;
	}

private:
	DWORD serial_number_;
};
