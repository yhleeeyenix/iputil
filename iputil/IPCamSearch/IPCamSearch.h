#pragma once

#ifndef __AFXWIN_H__
#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"

class CIPCamSearchApp : public CWinApp
{
public:
	CIPCamSearchApp();
	virtual BOOL InitInstance();
	virtual int  ExitInstance();

	DECLARE_MESSAGE_MAP()

private:
	HANDLE  m_hDuplication;
	BOOL    m_bDRun; // Duplication Run Status
	BOOL    m_AdminMode; // -setup (Option)

private:
	BOOL    RunAppCommandLine( void );
	BOOL    DuplicationEvent ( void );
	VOID    CloseEventHandle ( void );
};

extern CIPCamSearchApp theApp;