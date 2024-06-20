#pragma once
#include "afxwin.h"

class CIpChangeDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CIpChangeDlg)

public:
	CIpChangeDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_DLG_IPCHANGE };

	BOOL           SetInitInfo( const int _iCamCount, const netMsg* _pItem );
	BOOL           GetSetupInfoResult( BOOL& _bAutoIP, netMsg* _pItem );

protected:
	virtual void   DoDataExchange(CDataExchange* pDX);
	virtual BOOL   PreTranslateMessage(MSG* pMsg);
	virtual BOOL   OnInitDialog();
	afx_msg void   OnPaint();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void   OnDestroy();
	afx_msg void   OnSize(UINT nType, int cx, int cy);
	afx_msg void   OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void   OnCbnSelchangeCmbChoise();
	afx_msg void   OnBnClickedOk();

	DECLARE_MESSAGE_MAP()

private:
	BOOL           m_bInit;
	INT	           m_iWd, m_iHi;
	CBrush         m_cBrush;

	BOOL           m_bAutoIP;
	INT            m_iCameraNumber;

	netMsg         m_stItem   ,
		           m_stOldItem;

	CComboBox      m_ctlChoise;

private:
	BOOL           SetInitCtl( void );
public:
	afx_msg void OnBnClickedCancel();
};