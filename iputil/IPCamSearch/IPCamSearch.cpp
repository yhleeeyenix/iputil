
// IPCamSearch.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "IPCamSearch.h"
#include "IPCamSearchDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CIPCamSearchApp

BEGIN_MESSAGE_MAP(CIPCamSearchApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CIPCamSearchApp construction

CIPCamSearchApp::CIPCamSearchApp()
:m_hDuplication(NULL)
, m_bDRun(FALSE)
, m_AdminMode(FALSE)
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CIPCamSearchApp object

CIPCamSearchApp theApp;

// 애플리케이션이 실행되면 InitInstance()가 호출됩니다.
// DuplicationEvent()를 통해 중복 실행을 방지합니다.
// 공통 컨트롤과 윈속을 초기화하고 시각적 스타일과 레지스트리 키를 설정합니다.
// 중복 실행이 아닌 경우 CIPCamSearchDlg 대화상자를 생성하고 실행합니다.
// 대화상자가 종료되면 ExitInstance()가 호출되어 이벤트 핸들을 닫고 애플리케이션이 종료됩니다.
// 이 코드는 주로 초기화, 실행, 종료에 관한 부분을 다루고 있으며, 중복 실행 방지와 관리자 모드 설정 등의 기능을 포함하고 있습니다.

// CIPCamSearchApp initialization

BOOL CIPCamSearchApp::InitInstance()
{
	//===========================//
    // (Harace) Duplication Lock //
	if (!DuplicationEvent()){
		m_bDRun = TRUE;
	}
	//===========================//

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	//===========================//
	// (Harace) Check Admin Mode Run
	m_AdminMode = TRUE;//RunAppCommandLine();

	// (Harace) Winsock StartUp  //
	if (!AfxSocketInit()) {
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}
	//===========================//
	
	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	// Set Upper~!!
	if (m_bDRun){
		HWND hWnd = NULL;
		hWnd  = FindWindow( NULL, L"IP Camera Search Util" );

		if (hWnd){
			::SetForegroundWindow(hWnd);
			::BringWindowToTop(hWnd);
		}

		return FALSE;
	}


	CIPCamSearchDlg dlg;
	dlg.SetAdminMode(TRUE); // Set Admin Mode (Harace)
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// Delete the shell manager created above.
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

int CIPCamSearchApp::ExitInstance()
{
    // (Harace) Duplication Lock -----
	CloseEventHandle();

    return CWinApp::ExitInstance();
}

VOID CIPCamSearchApp::CloseEventHandle(void)
{
	if (m_hDuplication){
		CloseHandle(m_hDuplication);
		m_hDuplication = NULL;
	}
}

BOOL CIPCamSearchApp::RunAppCommandLine(void)
{
	CString strArgs    (_T(""));
	CString strStartKey(_T(""));
	strArgs = this->m_lpCmdLine;
	
	AfxExtractSubString(strStartKey, strArgs, 0, ' ');
	if ( 0 != strStartKey.Compare(_T("-setup")) )
		return FALSE;
	return TRUE;
}

BOOL CIPCamSearchApp::DuplicationEvent(void)
{
	LPCTSTR lpszClassName =
		AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW)); // CS_DBLCLKS

	CloseEventHandle();
	m_hDuplication = CreateEvent(NULL, FALSE, FALSE, lpszClassName );
	//m_hDuplication = CreateEvent(NULL, FALSE, TRUE, AfxGetAppName() );

    // If No Error Return 0 // ERROR_SUCCESS
	if (::GetLastError() == ERROR_ALREADY_EXISTS)
		return FALSE;

	return TRUE;
}