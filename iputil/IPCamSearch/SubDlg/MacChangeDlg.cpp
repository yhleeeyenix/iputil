#include "stdafx.h"
#include "IPCamSearch.h"
#include "MacChangeDlg.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(CMacChangeDlg, CDialogEx)

namespace {
	const WCHAR* DEFAULT_UI_MAC_ADDRESS = L"00:07:cd:00:00:00";

	inline void MakeMacAddressToString(u_int8_t* addr1, CString& str)
	{
		str.Format(L"%02x:%02x:%02x:%02x:%02x:%02x",
				   addr1[0], addr1[1], addr1[2],
				   addr1[3], addr1[4], addr1[5]
				   );
	}

	inline void MakeStringToMacAddress(CString& str, u_int8_t* addr1)
	{
		// Use Caution..
		for (int i=0; i<6; i++){
			CString strTemp(_T(""));
			AfxExtractSubString(strTemp, str, i, ':');

			CStringA strA = (CStringA)strTemp;
			sscanf_s(strA, "%02x", addr1+i);
		}
	}

	inline 	USHORT parseHexStringToSHORT(const WCHAR* hex_string_const)
	{
		int len = wcslen(hex_string_const);
		int i = 0;
		CString hex_string = hex_string_const;
		hex_string.MakeLower();
		USHORT value = 0;
		USHORT exp = 1;
		for (i = 0; i < len; ++i)
		{
			if (L'0' <= hex_string.GetAt(1 - i) && hex_string.GetAt(1 - i) <= L'9')
			{
				value += ((hex_string.GetAt(1 - i) - L'0') * (exp));
			}
			else if (L'a' <= hex_string.GetAt(1 - i) && hex_string.GetAt(1 - i) <= L'z')
			{
				value += ((hex_string.GetAt(1 - i) - L'a' + 0xa) * (exp));
			}
			else
			{
				ASSERT(0); // invalid string
			}
			exp = exp * 16;
		}
		return value;
	}
	inline void parseMacString(const WCHAR* mac_address, USHORT *ushor)
	{
		// parse one
		CString mac = mac_address;
		CString ms[6];
		int     pos = 0;
		int     i = 0;
		for (i = 0; i < 6; i++)
		{
			ushor[i] = 0;
			ms[i] = mac.Tokenize(L":", pos);
			ASSERT(!ms[i].IsEmpty());
			ushor[i] = parseHexStringToSHORT(ms[i].GetBuffer());
			ASSERT(ushor[i] >= 0 && ushor[i] < 256);
			ms[i].ReleaseBuffer();
		}
	}
}

CMacChangeDlg::CMacChangeDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CMacChangeDlg::IDD, pParent)
	, m_bInit(FALSE)
	, m_iWd(0)
	, m_iHi(0)
	, m_uiSelect(0)
{
	// Item Data
	memset( &m_stItem   , 0, sizeof(netMsg) );
	memset( &m_stOldItem, 0, sizeof(netMsg) );
}

void CMacChangeDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	// Delete Brush
	m_cBrush.DeleteObject();
}

void CMacChangeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDT_MAC_ADDRESS, m_CtlEdt_MacAddress);
}

BEGIN_MESSAGE_MAP(CMacChangeDlg, CDialogEx)
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_BN_CLICKED(IDOK, &CMacChangeDlg::OnBnClickedOk)
END_MESSAGE_MAP()

BOOL CMacChangeDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
		
	m_CtlEdt_MacAddress.SetPromptSymbol(_T('0'));
	m_CtlEdt_MacAddress.SetMask(_T("hh:hh:hh:hh:hh:hh"));
	m_CtlEdt_MacAddress.SetWindowText(DEFAULT_UI_MAC_ADDRESS);

	SetDlgItemInt( IDC_EDT_MAC_COUNT, m_uiSelect );

	// Create Brush
	m_cBrush.CreateSolidBrush(BACK_COLOR);

	// Set Current Mac Address
	CString strMacAddress(_T(""));
	MakeMacAddressToString( m_stItem.chaddr, strMacAddress );
	m_CtlEdt_MacAddress.SetWindowText( strMacAddress );

	m_bInit = TRUE;
	return TRUE;
}

BOOL CMacChangeDlg::SetInitInfo( const int _iCamCount, const netMsg* _pItem )
{
	ASSERT( (_pItem != NULL) && ( 0 < _iCamCount ) );
	
	// Init Data Item
	memcpy( &m_stItem, _pItem, sizeof(netMsg) );

	m_uiSelect = _iCamCount;

	return TRUE;
}
BOOL CMacChangeDlg::GetSetupInfoResult( netMsg* _pItem )
{
	memset( _pItem, 0, sizeof(netMsg) );
	memcpy( _pItem, &m_stItem, sizeof(netMsg));

	return TRUE;
}

BOOL CMacChangeDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
		pMsg->wParam = NULL;
	
	return CDialogEx::PreTranslateMessage(pMsg);
}

HBRUSH CMacChangeDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if (nCtlColor == CTLCOLOR_STATIC ||COLOR_BACKGROUND) {
		pDC->SetBkColor(BACK_COLOR);
		pDC->SetTextColor(TEXT_COLOR);
		hbr = (HBRUSH)m_cBrush.GetSafeHandle();
	}

	if (IDC_STT_MAC_SELECT == pWnd->GetDlgCtrlID())
		pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);
	return hbr;
}

void CMacChangeDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if ( !m_bInit || cx <= 0 ) return;
	m_iWd = cx; m_iHi = cy;
}

void CMacChangeDlg::OnPaint()
{
	CPaintDC dc(this);

	CDC* pDC = GetDC();
	CBitmap cBmp, *pOldBmp = NULL;
	BITMAP bmpInfo;

	CDC MemDC;
	MemDC.CreateCompatibleDC(pDC);
	cBmp.LoadBitmap(IDB_BIT_WARNING);
	cBmp.GetBitmap(&bmpInfo);

	pOldBmp = MemDC.SelectObject(&cBmp);

	pDC->SetBkMode(TRANSPARENT);
	pDC->BitBlt(10, 15, bmpInfo.bmWidth, bmpInfo.bmHeight, &MemDC, 0, 0, SRCCOPY);

	cBmp.DeleteObject();
	ReleaseDC(pDC);
}

void CMacChangeDlg::OnLButtonDown(UINT nFlags, CPoint point)
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

void CMacChangeDlg::OnBnClickedOk()
{
	// Save Current Data
	memcpy( &m_stOldItem, &m_stItem, sizeof(netMsg) );

	// Set Change Mac Address
	CString str(_T(""));
	m_CtlEdt_MacAddress.GetWindowText(str);
	//MakeStringToMacAddress( str, m_stItem.chaddr2 );

	USHORT ushort[6] = {0,};
	parseMacString( str, ushort );
	for (int i=0; i<6; i++){
		m_stItem.chaddr2[i] = (BYTE)ushort[i];
	}

	CDialogEx::OnOK();
}