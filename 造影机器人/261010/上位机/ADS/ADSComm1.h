#pragma once
#include "TcAdsDef.h"
#include "TcAdsAPI.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include<memory>
#include <iostream>

class CADSComm
{
public:
	CADSComm(void);
	~CADSComm(void);


	bool OpenComm();
	bool OpenComm_inside();//内部通信测试用

	bool CloseComm();
	bool ADSWrite(char * paraName, unsigned long length, void * data);
	bool ADSRead(char * paraName, unsigned long length, void * data);
	bool ADSWrite(unsigned long addr, unsigned long length, void * data);
	bool ADSRead(unsigned long addr, unsigned long length, void * data);
	unsigned long ADSGetAddr(char * paraName);
	bool IsCommOpen()
	{
		return m_bOpen;
	}

	const char * GetLastError()
	{
		return m_lastError;
	}

private:

	PAmsAddr		m_PAmsAddr;
	bool			m_bOpen;
	char 			m_lastError[256];
};

