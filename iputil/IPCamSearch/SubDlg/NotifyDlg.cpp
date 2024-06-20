
// 이 코드는 CNotifyDlg라는 이름의 MFC 대화 상자를 정의한 것입니다. 이 대화 상자는 사용자에게 특정 알림을 표시하기 위해 설계되었습니다. 이 클래스는 CDialogEx를 상속받아 구현되었으며, 여러 메시지 처리기와 사용자 정의 초기화 코드를 포함하고 있습니다.

#include "stdafx.h"
#include "IPCamSearch.h"
#include "NotifyDlg.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(CNotifyDlg, CDialogEx)

CNotifyDlg::CNotifyDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CNotifyDlg::IDD, pParent)
	, m_uiSelect(0)
	, m_bInit(FALSE)
	, m_iWd(0)
	, m_iHi(0)
	, m_uiStyleOption(0)
{
}

void CNotifyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNotifyDlg, CDialogEx)
	ON_WM_DESTROY()
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CNotifyDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
        pMsg->wParam = NULL;

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CNotifyDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	// Delete Brush
	m_cBrush.DeleteObject();
}

HBRUSH CNotifyDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if (nCtlColor == CTLCOLOR_STATIC ||COLOR_BACKGROUND) {
		pDC->SetBkColor(BACK_COLOR);
		pDC->SetTextColor(TEXT_COLOR);
		hbr = (HBRUSH)m_cBrush.GetSafeHandle();
	}

	
	if ( IDC_STT_NOTIFY_SELECT == pWnd->GetDlgCtrlID() )
		pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);

	return hbr;
}

void CNotifyDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if ( !m_bInit || cx <= 0 ) return;
	m_iWd = cx; m_iHi = cy;
}

BOOL CNotifyDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Create Brush
	m_cBrush.CreateSolidBrush(BACK_COLOR);

	// Show Count
	SetDlgItemInt( IDC_EDT_NOTIFY_COUNT, m_uiSelect );

	// Set Style Text
	switch (m_uiStyleOption){
		case 0: { SetDlgItemText(IDC_STT_NOTIFY_CANCEL, _T("Do you want run update?"));    break; }
		case 1: { SetDlgItemText(IDC_STT_NOTIFY_CANCEL, _T("Do you want cancel update?")); break; }
		default: { ASSERT(0); break; }
	}

	m_bInit = TRUE;
	return TRUE;
}
BOOL CNotifyDlg::setInitStyle(UINT _uiCount, UINT _uiStyle )
{
	m_uiSelect      = _uiCount;
	m_uiStyleOption = _uiStyle;

	return TRUE;
}

void CNotifyDlg::OnLButtonDown(UINT nFlags, CPoint point)
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

void CNotifyDlg::OnPaint()
{
	CPaintDC dc(this);
	
	// TODO:
	CDC* pDC = GetDC();
    CBitmap cBmp, *pOldBmp = NULL;
    BITMAP bmpInfo;

    CDC MemDC;
    MemDC.CreateCompatibleDC(pDC);
    cBmp.LoadBitmap(IDB_BIT_WARNING);
    cBmp.GetBitmap(&bmpInfo);

    pOldBmp = MemDC.SelectObject(&cBmp);
	
    pDC->SetBkMode(TRANSPARENT);
	pDC->BitBlt(10,15, bmpInfo.bmWidth, bmpInfo.bmHeight,&MemDC, 0, 0, SRCCOPY);

    cBmp.DeleteObject();
    ReleaseDC(pDC);
}