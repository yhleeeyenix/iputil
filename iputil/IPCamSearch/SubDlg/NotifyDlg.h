#pragma once

class CNotifyDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CNotifyDlg)

public:
	CNotifyDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_DLG_NOTIFY };

	BOOL setInitStyle( UINT _uiCount, UINT _uiStyle );

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);
	virtual BOOL   PreTranslateMessage(MSG* pMsg);
	virtual BOOL   OnInitDialog();
	afx_msg void   OnDestroy();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void   OnSize(UINT nType, int cx, int cy);
	afx_msg void   OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void   OnPaint();
	
	DECLARE_MESSAGE_MAP()

private:
	BOOL   m_bInit;
	INT    m_iWd,
		   m_iHi;

	UINT   m_uiSelect     ,
		   m_uiStyleOption;

	CBrush m_cBrush;

};
