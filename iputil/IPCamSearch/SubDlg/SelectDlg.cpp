#include "stdafx.h"
#include "IPCamSearch.h"
#include "SelectDlg.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(CSelectDlg, CDialogEx)

CSelectDlg::CSelectDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CSelectDlg::IDD, pParent)
	, m_bInit(FALSE)
	, m_iWd(0)
	, m_iHi(0)
	, m_bSelectStatus(FALSE)
	, m_uiCount(0)
	, m_uiStyle(0)
{
}

void CSelectDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	// Delete Brush
m_cBrush.DeleteObject();
}

void CSelectDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSelectDlg, CDialogEx)
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_COMMAND_RANGE(IDC_RDO_SELECT_ON,IDC_RDO_SELECT_OFF, &CSelectDlg::OnChkZero )
END_MESSAGE_MAP()

BOOL CSelectDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:
	
	// Create Brush
	m_cBrush.CreateSolidBrush(BACK_COLOR);

	// Delete Window Theme
	//::SetWindowTheme(GetDlgItem(IDC_RDO_SELECT_ON )->GetSafeHwnd(),  _T(""), _T(""));
	//::SetWindowTheme(GetDlgItem(IDC_RDO_SELECT_OFF)->GetSafeHwnd(),  _T(""), _T(""));

	// Set Rdo Button
	CheckRadioButton( IDC_RDO_SELECT_ON, IDC_RDO_SELECT_OFF, IDC_RDO_SELECT_OFF );
	
	// Show Text
	switch (m_uiStyle) {
		case 0: { SetDlgItemText( IDC_STT_SELECT_NOTIFY, _T("DHCP Mode :"   ) ); break; }
		case 1: { SetDlgItemText( IDC_STT_SELECT_NOTIFY, _T("Auto IP Mode :") ); break; }		
		default: { ASSERT(0); break; }
	}
	// Set Count
	SetDlgItemInt( IDC_EDT_SELECT_COUNT, m_uiCount );

	m_bInit = TRUE;
	return TRUE;
}
BOOL CSelectDlg::SetInit( UINT _iCount, UINT _uiStyle)
{
	if (m_bInit) return FALSE;
	
	// Set Dlg Style & Select Camera Count
	m_uiStyle = _uiStyle;
	m_uiCount = _iCount;
	return TRUE;
}
void CSelectDlg::OnChkZero(UINT nID)
{
	ASSERT( (IDC_RDO_SELECT_ON < nID) || ( IDC_RDO_SELECT_OFF > nID ) );

	m_bSelectStatus = ( IDC_RDO_SELECT_ON == GetCheckedRadioButton(IDC_RDO_SELECT_ON, IDC_RDO_SELECT_OFF) ) ? TRUE : FALSE;

	return;
}

BOOL CSelectDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO:
    if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
        pMsg->wParam = NULL;

	return CDialogEx::PreTranslateMessage(pMsg);
}

HBRUSH CSelectDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

		if (nCtlColor == CTLCOLOR_STATIC ||COLOR_BACKGROUND) {
		pDC->SetBkColor(BACK_COLOR);
		pDC->SetTextColor(TEXT_COLOR);
		hbr = (HBRUSH)m_cBrush.GetSafeHandle();
	}

	if ( IDC_STT_SELECT_COUNT == pWnd->GetDlgCtrlID() )
		pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);

			// Set Text Color
	//if ( IDC_STT_SELECT_NOTIFY == pWnd->GetDlgCtrlID() )
	//	pDC->SetTextColor(RGB(255,177,30));
	//if ( IDC_RDO_SELECT_ON == pWnd->GetDlgCtrlID() )
	//	pDC->SetTextColor(RGB(255,177,30));
		//pDC->SetTextColor(COLOR_CUSTOMIZE_RED);
	//	//pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);

	//	const COLORREF COLOR_CUSTOMIZE_ORANGE = RGB(255,177,30);
	return hbr;
}

void CSelectDlg::OnLButtonDown(UINT nFlags, CPoint point)
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

void CSelectDlg::OnPaint()
{
	CPaintDC dc(this);
		// TODO:
	CDC* pDC = GetDC();
    CBitmap cBmp, *pOldBmp = NULL;
    BITMAP bmpInfo;

    CDC MemDC;
    MemDC.CreateCompatibleDC(pDC);
    cBmp.LoadBitmap(IDB_BIT_INFO);
    cBmp.GetBitmap(&bmpInfo);

    pOldBmp = MemDC.SelectObject(&cBmp);
	
    pDC->SetBkMode(TRANSPARENT);
	pDC->BitBlt(10,10, bmpInfo.bmWidth, bmpInfo.bmHeight,&MemDC, 0, 0, SRCCOPY);

    cBmp.DeleteObject();
    ReleaseDC(pDC);

}

void CSelectDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if ( !m_bInit || cx <= 0 ) return;
	m_iWd = cx; m_iHi = cy;
}