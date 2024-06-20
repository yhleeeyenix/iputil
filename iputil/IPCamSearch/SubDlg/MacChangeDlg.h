#pragma once
#include "afxwin.h"

class CMacChangeDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CMacChangeDlg)

public:
	CMacChangeDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_DLG_MAC };

	BOOL           SetInitInfo       ( const int _iCamCount, const netMsg* _pItem );
	BOOL           GetSetupInfoResult( netMsg* _pItem );

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);
	virtual BOOL   OnInitDialog();
	virtual BOOL   PreTranslateMessage(MSG* pMsg);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void   OnDestroy();
	afx_msg void   OnSize(UINT nType, int cx, int cy);
	afx_msg void   OnPaint();
	afx_msg void   OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void   OnBnClickedOk();

	DECLARE_MESSAGE_MAP()

private:
	BOOL           m_bInit;
	INT            m_iWd,
		           m_iHi;
	CBrush         m_cBrush;

	UINT           m_uiSelect;

	netMsg         m_stItem   ,
		           m_stOldItem;

	COXMaskedEdit  m_CtlEdt_MacAddress;
};