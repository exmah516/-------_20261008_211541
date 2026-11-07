#include "ADSComm1.h"
CADSComm::CADSComm(void)
{
	m_bOpen=false;
	m_PAmsAddr =new AmsAddr;
	memset(m_lastError,0,sizeof(char)*256);
}

CADSComm::~CADSComm(void)
{
	delete m_PAmsAddr;
}

bool CADSComm::ADSWrite(char * paraName, unsigned long length, void * data)//plc变量名，sizeof(东西)，c++要传入的东西的地址
{
	if (!m_bOpen)
	{
		printf(m_lastError,"Error: Ads not Open: \n");
		return false;
	}
	long nErr;
	unsigned long lHdlVar;	//创建句柄
	int namelen=strlen(paraName)+1;
	nErr = AdsSyncReadWriteReq(m_PAmsAddr,ADSIGRP_SYM_HNDBYNAME,0x0, sizeof(lHdlVar), &lHdlVar,namelen,paraName);
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncReadWriteReq:  %d\n",nErr);
		return false;
	}
	nErr = AdsSyncWriteReq(m_PAmsAddr, ADSIGRP_SYM_VALBYHND, lHdlVar, length, data); 
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncWriteReq:  %d\n",nErr);
		return false;
	}
	return true;
}
bool CADSComm::ADSRead(char * paraName, unsigned long length, void * data)
{
	if (!m_bOpen)
	{
		printf(m_lastError,"Error: Ads not Open: \n");
		return false;
	}
	long nErr;
	unsigned long lHdlVar;	//创建句柄
	int namelen=strlen(paraName)+1;
	nErr = AdsSyncReadWriteReq(m_PAmsAddr,ADSIGRP_SYM_HNDBYNAME,0x0, sizeof(lHdlVar), &lHdlVar,namelen,paraName);
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncReadWriteReq:  %d\n",nErr);
		return false;
	}
	nErr = AdsSyncReadReq(m_PAmsAddr, ADSIGRP_SYM_VALBYHND, lHdlVar,length, data); 
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncWriteReq:  %d\n",nErr);
		return false;
	}
	return true;
}
bool CADSComm::ADSWrite(unsigned long addr , unsigned long length, void * data)
{
	if (!m_bOpen)
	{
		printf(m_lastError,"Error: Ads not Open: \n");
		return false;
	}
	long nErr;
	nErr = AdsSyncWriteReq(m_PAmsAddr, ADSIGRP_SYM_VALBYHND, addr, length, data); 
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncWriteReq:  %d\n",nErr);
		return false;
	}
	return true;
}
bool CADSComm::ADSRead(unsigned long addr , unsigned long length, void * data)
{
	if (!m_bOpen)
	{
		printf(m_lastError,"Error: Ads not Open: \n");
		return false;
	}
	long nErr;
	nErr = AdsSyncReadReq(m_PAmsAddr, ADSIGRP_SYM_VALBYHND, addr,length, data); 
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncWriteReq:  %d\n",nErr);
		return false;
	}
	return true;
}

unsigned long CADSComm::ADSGetAddr(char * paraName)
{
	if (!m_bOpen)
	{
		printf(m_lastError,"Error: Ads not Open: \n");
		return 0;
	}
	long nErr;
	unsigned long lHdlVar;	//创建句柄
	int namelen=strlen(paraName)+1;
	nErr = AdsSyncReadWriteReq(m_PAmsAddr,ADSIGRP_SYM_HNDBYNAME,0x0, sizeof(lHdlVar), &lHdlVar,namelen,paraName);
	if (nErr)
	{ 
		printf(m_lastError,"Error: AdsSyncReadWriteReq:  %d\n",nErr);
		return 0;
	}
	return lHdlVar; 
}
bool CADSComm::OpenComm()
{
	m_bOpen=false;
	long nPort, nErr;
	USHORT   nAdsState;	//包含PLC的状态信息

	nPort = AdsPortOpen();

	AmsNetId _AmsId = { 5, 100, 219, 36, 1, 1 };
	m_PAmsAddr->netId = _AmsId;			//外部通信

	m_PAmsAddr->port = 851;


	USHORT        nDeviceState;
	nErr = AdsSyncReadStateReq(m_PAmsAddr, &nAdsState, &nDeviceState);//这一步老慢了


	if (nErr)
	{
		(m_lastError,"Error: AdsSyncReadStateReq:  %d\n",nErr);
		return false;
	}
	if (nAdsState==6)
	{
		nAdsState = ADSSTATE_RUN;
		nErr = AdsSyncWriteControlReq(m_PAmsAddr, nAdsState, nDeviceState, 0, NULL);
		if (nErr)
		{
			printf(m_lastError,"Error: AdsSyncWriteControlReq: " ,nErr);
			return false;
		}
	}
	m_bOpen=true;


	return true;
}

bool CADSComm::OpenComm_inside()
{
	m_bOpen = false;
	long nPort, nErr;
	USHORT   nAdsState;	//包含PLC的状态信息

	nPort = AdsPortOpen();

	nErr = AdsGetLocalAddress(m_PAmsAddr);//内部通信

	m_PAmsAddr->port = 851;

	USHORT        nDeviceState;
	nErr = AdsSyncReadStateReq(m_PAmsAddr, &nAdsState, &nDeviceState);
	if (nErr)
	{
		(m_lastError, "Error: AdsSyncReadStateReq:  %d\n", nErr);
		return false;
	}
	if (nAdsState == 6)
	{
		nAdsState = ADSSTATE_RUN;
		nErr = AdsSyncWriteControlReq(m_PAmsAddr, nAdsState, nDeviceState, 0, NULL);
		if (nErr)
		{
			printf(m_lastError, "Error: AdsSyncWriteControlReq: ", nErr);
			return false;
		}
	}
	m_bOpen = true;
	return true;
}

bool CADSComm::CloseComm()
{
	long nErr;
	nErr = AdsPortClose();
	if (nErr)
	{
		printf(m_lastError, "Error: AdsPortClose:   %d\n", nErr);
		return false;
	}
	return true;
}