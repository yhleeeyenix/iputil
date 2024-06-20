// MFC(Microsoft Foundation Class)를 사용하여 작성된 IP 카메라의 IP 주소를 변경하는 대화상자(CIpChangeDlg)를 구현한 것입니다. 이 대화상자는 IP 주소를 입력하고 설정하는 기능을 제공합니다.
// CIpChangeDlg 객체가 생성될 때, 멤버 변수를 초기화합니다.
// OnInitDialog() 함수에서 대화상자가 초기화되고, 컨트롤이 설정됩니다.
// 사용자가 OK 버튼을 클릭하면 OnBnClickedOk() 함수가 호출되어 현재 설정된 IP 주소 정보를 저장하고 대화상자를 종료합니다.
// 콤보 박스 선택이 변경되면 OnCbnSelchangeCmbChoise() 함수가 호출되어 IP 설정 모드를 업데이트합니다.
// 대화상자가 파괴될 때 OnDestroy() 함수가 호출되어 리소스를 정리합니다.
// 마우스 왼쪽 버튼을 눌러 창을 이동하거나 크기를 조정할 수 있습니다.
// 이 코드는 사용자가 IP 주소를 입력하고 설정할 수 있는 대화상자를 구현하여, 여러 개의 IP 카메라에 대해 IP 주소를 관리할 수 있도록 합니다.

#include "stdafx.h"

#include "IPCamSearch.h"
#include "IpChangeDlg.h"
#include "afxdialogex.h"

namespace
{
#define EYENIX_DEFAULT_IPADDRESS _T( "192.168.0.195" )

	const INT IP_INPUT_MAX_LEN = 20;

	inline void MakeIPAddressToWchar(u_int32_t _addr, WCHAR* _psz)
	{
		memset(_psz, 0, sizeof(_psz));
		wsprintf(_psz, _T("%d.%d.%d.%d"), (_addr & 0xff),
				 ((_addr >> 8) & 0xff),
				 ((_addr >> 16) & 0xff),
				 ((_addr >> 24) & 0xff));
	}

	inline void MakeStringToIPAddress(CString _strIP, u_int32_t &_iIP)
	{
		_iIP = 0;

		for (int i=0; i<4; i++){
			CString strTemp(_T(""));
			AfxExtractSubString(strTemp, _strIP, i, '.');
			switch (i){
				case 0:{ _iIP  |= (_ttoi(strTemp) <<  0);  break; }
				case 1:{ _iIP  |= (_ttoi(strTemp) <<  8);  break; }
				case 2:{ _iIP  |= (_ttoi(strTemp) << 16);  break; }
				case 3:{ _iIP  |= (_ttoi(strTemp) << 24); break; }
				default:{ ASSERT(0); break; }
			}
		}
	}

#if 0
	inline void MakeMacAddressToString( u_int8_t* addr, WCHAR* _psz )
	{
		wsprintf( _psz, _T("%02x:%02x:%02x:%02x:%02x:%02x"), addr[0], addr[1], addr[2] ,
				 addr[3], addr[4], addr[5] );
	}
#endif
}

IMPLEMENT_DYNAMIC(CIpChangeDlg, CDialogEx)

CIpChangeDlg::CIpChangeDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CIpChangeDlg::IDD, pParent)
	, m_bInit(FALSE)
	, m_iWd(0)
	, m_iHi(0)
	, m_iCameraNumber(0)
	, m_bAutoIP(FALSE)
{
	memset( &m_stItem   , 0, sizeof(netMsg) );
	memset( &m_stOldItem, 0, sizeof(netMsg) );
}

void CIpChangeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CMB_IPC_CHOISE, m_ctlChoise);
}

void CIpChangeDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	// Delete Brush
	m_cBrush.DeleteObject();
}

BEGIN_MESSAGE_MAP(CIpChangeDlg, CDialogEx)	
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_BN_CLICKED(IDOK, &CIpChangeDlg::OnBnClickedOk)
	ON_CBN_SELCHANGE(IDC_CMB_IPC_CHOISE, &CIpChangeDlg::OnCbnSelchangeCmbChoise)
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CIpChangeDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
        pMsg->wParam = NULL;

	return CDialogEx::PreTranslateMessage(pMsg);
}

BOOL CIpChangeDlg::SetInitInfo( const int _iCamCount, const netMsg* _pItem )
{
	ASSERT( (0 < _iCamCount) && (NULL != _pItem) );

	// Init Data Item
	memcpy( &m_stItem, _pItem, sizeof(netMsg) );

	// Set Camera Count
	m_iCameraNumber = _iCamCount;

	return TRUE;
}

BOOL CIpChangeDlg::GetSetupInfoResult( BOOL& _bAutoIP, netMsg* _pItem )
{
	memcpy( _pItem, &m_stItem, sizeof(netMsg));
	
	_bAutoIP = m_bAutoIP;
	
	return TRUE;
}

BOOL CIpChangeDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Init Control Item
	SetInitCtl();

	// Create Brush
	m_cBrush.CreateSolidBrush(BACK_COLOR);

	// Delete Window Theme
	//::SetWindowTheme( GetDlgItem(IDC_GRP_IPC_IPSETUP)->GetSafeHwnd(), _T(""), _T("") );
	//::SetWindowTheme( GetDlgItem(IDC_CMB_IPC_CHOISE)->GetSafeHwnd() , _T(""), _T("") );

	m_bInit = TRUE;
	return TRUE;
}

BOOL CIpChangeDlg::SetInitCtl(void)
{
	// Set Ip Camera Count
	SetDlgItemInt( IDC_EDT_IPC_COUNT, m_iCameraNumber );

	// Set IP Address
	WCHAR szItemText[IP_INPUT_MAX_LEN] ={ 0, };
	MakeIPAddressToWchar(m_stItem.ciaddr, szItemText);
	SetDlgItemText(IDC_IPA_IPC_TARGET, szItemText);
	MakeIPAddressToWchar(m_stItem.giaddr, szItemText);
	SetDlgItemText(IDC_IPA_IPC_GATEWAY, szItemText);
	MakeIPAddressToWchar(m_stItem.miaddr, szItemText);
	SetDlgItemText(IDC_IPA_IPC_NETMASK, szItemText);
	MakeIPAddressToWchar(m_stItem.d1iaddr, szItemText);
	SetDlgItemText(IDC_IPA_IPC_DDNS1, szItemText);
	MakeIPAddressToWchar(m_stItem.d2iaddr, szItemText);
	SetDlgItemText(IDC_IPA_IPC_DDNS2, szItemText);

	// Set Multi IP
	if (1 <  m_iCameraNumber) {
		// Init IP Type
		m_ctlChoise.AddString( _T("Continuous") );
		m_ctlChoise.AddString( _T("Static"    ) );
		m_ctlChoise.SetCurSel(0);
		m_bAutoIP = TRUE;
		
		// Window Size Modify
		CRect rtWindow;
		GetWindowRect(rtWindow);
		rtWindow.bottom = rtWindow.top + 175;
		this->MoveWindow(rtWindow);

		// Move Apply Cancel Btn
		CRect rtBtn, rtClinet;
		GetClientRect(rtClinet);
		int iWidth(0), iHeight(0);
		CWnd* pWnd(NULL);
		pWnd = GetDlgItem(IDOK);
		if (pWnd){
			pWnd->GetWindowRect(rtBtn);
			iHeight = rtBtn.bottom - rtBtn.top;
			iWidth  = rtBtn.right - rtBtn.left;

			rtBtn.top    = rtClinet.bottom - iHeight - 10;
			rtBtn.bottom = rtBtn.top + iHeight;
			rtBtn.left   = rtClinet.CenterPoint().x - iWidth - 2;
			rtBtn.right  = rtBtn.left + iWidth;
			// Move Apply
			pWnd->MoveWindow(rtBtn);
		}
		pWnd = GetDlgItem(IDCANCEL);
		if (pWnd){
			pWnd->GetWindowRect(rtBtn);
			iHeight = rtBtn.bottom - rtBtn.top;
			iWidth  = rtBtn.right - rtBtn.left;

			rtBtn.top    = rtClinet.bottom - iHeight - 10;
			rtBtn.bottom = rtBtn.top + iHeight;
			rtBtn.left   = rtClinet.CenterPoint().x + 2;
			rtBtn.right  = rtBtn.left + iWidth;
			// Move Cancel
			pWnd->MoveWindow(rtBtn);
		}

		// Hide Control
		/*pWnd = GetDlgItem(IDC_STT_IPC_GATEWAY);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);*/
		pWnd = GetDlgItem(IDC_STT_IPC_NETMASK);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);
		pWnd = GetDlgItem(IDC_STT_IPC_DDNS1);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);
		pWnd = GetDlgItem(IDC_STT_IPC_DDNS2);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);
		/*pWnd = GetDlgItem(IDC_IPA_IPC_GATEWAY);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);*/
		pWnd = GetDlgItem(IDC_IPA_IPC_NETMASK);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);
		pWnd = GetDlgItem(IDC_IPA_IPC_DDNS1);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);
		pWnd = GetDlgItem(IDC_IPA_IPC_DDNS2);
		if (pWnd) pWnd->ShowWindow(SW_HIDE);
	}
	else { // Set Just One IP
		// Init IP Type
		m_ctlChoise.AddString(_T("Set Just One Ip"));
		//m_ctlChoise.AddString(_T("Set Same of IP"));	
		m_ctlChoise.SetCurSel(0);

		// Lock IP Type
		m_ctlChoise.EnableWindow(FALSE);
	}

	return TRUE;
}

void CIpChangeDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting


	CDC* pDC = GetDC();
    CBitmap cBmp, *pOldBmp = NULL;
    BITMAP bmpInfo;

    CDC MemDC;
    MemDC.CreateCompatibleDC(pDC);
    cBmp.LoadBitmap(IDB_BIT_IP);
    cBmp.GetBitmap(&bmpInfo);

    pOldBmp = MemDC.SelectObject(&cBmp);
	
    pDC->SetBkMode(TRANSPARENT);
	pDC->BitBlt(20,52, bmpInfo.bmWidth, bmpInfo.bmHeight,&MemDC, 0, 0, SRCCOPY);

    cBmp.DeleteObject();
    ReleaseDC(pDC);
}

HBRUSH CIpChangeDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if (nCtlColor == CTLCOLOR_STATIC ||COLOR_BACKGROUND) {
		pDC->SetBkColor(BACK_COLOR);
		pDC->SetTextColor(TEXT_COLOR);
		hbr = (HBRUSH)m_cBrush.GetSafeHandle();
	}

	if (IDC_STT_IPC_SELECT == pWnd->GetDlgCtrlID())
		pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);

	return hbr;
}

void CIpChangeDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if ( !m_bInit || cx <= 0 ) return ;
	m_iWd = cx; m_iHi = cy;
}

void CIpChangeDlg::OnBnClickedOk()
{
	// Save Current Data
	memcpy( &m_stOldItem, &m_stItem, sizeof(netMsg) );

	// Set Setup IP
	CString strIP(_T(""));
	GetDlgItemText(IDC_IPA_IPC_TARGET, strIP);
	MakeStringToIPAddress(strIP, m_stItem.yiaddr); // Change IP
	strIP = _T("");
	GetDlgItemText(IDC_IPA_IPC_GATEWAY, strIP);
	MakeStringToIPAddress(strIP, m_stItem.giaddr); // Gateway
	strIP = _T("");
	GetDlgItemText(IDC_IPA_IPC_NETMASK, strIP);
	MakeStringToIPAddress(strIP, m_stItem.miaddr);  // NetMask
	strIP = _T("");
	GetDlgItemText(IDC_IPA_IPC_DDNS1, strIP);
	MakeStringToIPAddress(strIP, m_stItem.d1iaddr); // DDNS1
	strIP = _T("");
	GetDlgItemText(IDC_IPA_IPC_DDNS2, strIP);
	MakeStringToIPAddress(strIP, m_stItem.d2iaddr); // DDNS2

	CDialogEx::OnOK();
}

void CIpChangeDlg::OnCbnSelchangeCmbChoise()
{
	if (0 == m_ctlChoise.GetCurSel()){
		m_bAutoIP = TRUE;

		// Set Base IP Address
		WCHAR szItemText[IP_INPUT_MAX_LEN] ={ 0, };
		MakeIPAddressToWchar( m_stItem.ciaddr, szItemText );
		SetDlgItemText( IDC_IPA_IPC_TARGET, szItemText );
	} else {
		m_bAutoIP = FALSE;
		SetDlgItemText( IDC_IPA_IPC_TARGET, EYENIX_DEFAULT_IPADDRESS );
	}
}

void CIpChangeDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Any Way Catch Focus and Move
	CRect moveableRect;
	GetClientRect(&moveableRect);
	moveableRect.left = max(1, moveableRect.right - 10);
	moveableRect.top  = max(1, moveableRect.bottom - 10);

	if (PtInRect(&moveableRect, point))
	{
		SetCursor(LoadCursor(0, IDC_SIZEALL));

		if (nFlags&MK_LBUTTON)
			SendMessage(WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0);
	}
	else
		PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));

	CDialogEx::OnLButtonDown(nFlags, point);
}
