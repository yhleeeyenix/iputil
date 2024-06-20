#pragma once
#include "afxwin.h"

class CSelectDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CSelectDlg)

public:
	CSelectDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_DLG_SELECT };

	BOOL GetSetupStatus( void ){ return m_bSelectStatus; }
	BOOL SetInit       ( UINT _iCount, UINT _uiStyle = 0 );

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);
	virtual BOOL   OnInitDialog();
	virtual BOOL   PreTranslateMessage(MSG* pMsg);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void   OnDestroy();
	afx_msg void   OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void   OnPaint();
	afx_msg void   OnSize(UINT nType, int cx, int cy);
	afx_msg void   OnChkZero(UINT nID);

	DECLARE_MESSAGE_MAP()

private:
	BOOL           m_bInit;
	INT	           m_iWd, m_iHi;

	UINT           m_uiCount,
		           m_uiStyle;

	BOOL           m_bSelectStatus;
	CBrush m_cBrush;
public:
	afx_msg void OnBnClickedOk();
};