#pragma once
#include "afxcmn.h"

enum tag_ListOperationCtl{
    //LST_OP_SEARCH     = 0,
    //LST_OP_IP_CHANGE     ,
	LST_OP_UPDATE = 0,
	LST_OP_ALL_CLEAR     ,
	LST_OP_DELETE        ,
	LST_OP_WEB_OPEN      ,
    LST_OP_DHCP          ,
	LST_OP_REBOOT        ,
	LST_OP_AUTOIP        ,
	LST_OP_MAC_CHANGE    ,

	LST_OP_MAX_CONTROL
};

class CKOUpdater;

class CIPCamSearchDlg : public CDialogEx
{
public:
	CIPCamSearchDlg(CWnd* pParent = NULL);
	enum { IDD = IDD_IPCAMSEARCH_DIALOG };
	
	DWORD           RunRecv(void);

	BOOL            IsSortAscendingOrder()   { return m_bSortAscending; }
	int             GetSelectedColumnIndex() {	return m_iOldColumn; }
	VOID SetAdminMode(BOOL _bAdminMode){  m_bAdminMode = _bAdminMode; }

protected:
	HICON           m_hIcon;
	virtual void    DoDataExchange(CDataExchange* pDX);
	virtual BOOL    OnInitDialog();
	virtual BOOL    PreTranslateMessage(MSG* pMsg);
	afx_msg void    OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void    OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void    OnNMRClickListIpcam(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void    OnTimer(UINT_PTR nIDEvent);
    afx_msg HBRUSH  OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void    OnLvnColumnclickListIpcam(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void    OnNMDblclkListIpcam(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void    OnDestroy();
	afx_msg void    OnLvnDeleteitemListIpcam(NMHDR *pNMHDR, LRESULT *pResult);
	
	afx_msg void    OnBnClickedBtnPath();
	afx_msg void    OnBnClickedBtnListCtl( UINT _uiID );

	DECLARE_MESSAGE_MAP()

private:
	BOOL            m_bAdminMode; // Run Option

	BOOL		    m_bSearch;
	HANDLE		    m_hThread, m_hWake;
	SOCKET		    m_sockSend, m_sockRecv, m_sockupdate;
	SOCKADDR_IN	    m_addrSend, m_addrRecv, m_addupdate;

	CBrush          m_cBrush;
	CListCtrl       m_cList;

    unsigned int    m_uiItemCount;
    CString         m_strFilePath;
	BOOL            m_bUpdateStartFlg;

    unsigned int*   m_puiID;
    CKOUpdater**    m_arrppUpdateItem;

    CProgressCtrl   m_ctrlPrgSearch;
    
	// About Search & Stop
	UINT            m_uiSearchTimerID;
	UINT            m_uiStopTimerID;
	
	// About IP Change
	UINT            m_uiIpChangeTimerID;
	UINT            m_uiIPChangeSendCount;
	BOOL            m_bContinueIP;
	netMsg*         m_pstIpChangeStartItem;

	// About Auto IP Set
	UINT            m_uiAutoIpTimerID;
	UINT            m_uiAutoIpSendCount;
	BOOL            m_AutoIP;
	
	// About DHCP Set
	BOOL            m_bDhcp;
	UINT            m_uiDhcpSendCount;
	UINT            m_uiDhcpTimerID;

	// About Mac Set
	BOOL m_bMacAddress;
	UINT m_uiMacSendCount;
	UINT m_uiMacTimerID;

	BOOL            m_bSortAscending;
	int             m_iOldColumn;

private:
	VOID            SetCtrlInit      ( void );
	
	//---------------------------------------- About Timer
	BOOL            StartStopTimer     ( void );
	BOOL            FinishStopTimer    ( void );
	BOOL            StartSearchTimer   ( void );
	BOOL            FinishSearchTimer  ( void );
	BOOL            StartIpChangeTimer ( void );
	BOOL            FinishIpChangeTimer( void );
	BOOL            StartAutoIpTimer   ( void );
	BOOL            FinishAutoIpTimer  ( void );
	BOOL            StartDhcpTimer     ( void );
	BOOL            FinishDhcpTimer    ( void );

	BOOL StartMacTimer  ( void );
	BOOL FinishMacTimer ( void );
	// ------------------------------------------------ About Timer
	
	BOOL            ShowSubMenuSetup ( void );
    BOOL            ShowSubMenuClear ( void );
    BOOL            ShowSubMenuCancel( void );

	BOOL            UpdateStart       ( void );
    BOOL            UpdateCancel      ( int _iItemIdx );
	BOOL            UpdateFinishCheck ( void );

	BOOL            ChkLstDuplicationDevice ( void );
	BOOL            ChkLstDuplicationItem   ( int _iSubItem );

    BOOL            IsNewVersion     ( CString &_strVersion );

	inline void     UpdateColumnName(int column_index, BOOL is_ascending);
	inline void     ShowRecvMsg(netMsg* pNetMsg);

	LRESULT         OnRecvSearch  ( WPARAM _wParam, LPARAM _lParam );
    LRESULT         OnUpdateStatus( WPARAM _wParam, LPARAM _lParam );

	BOOL            ShowIPChangeDlg(void);
	BOOL            IPCangeStart( const UINT iCount, const BOOL _bContinueIP, const netMsg* _pItem );

	BOOL            OperationList( int _iOption );
	BOOL            SetUiBtnEnableWindow( const BOOL _bLock = TRUE, const int _iOption = 0 );

	BOOL            SendSerarchCmd(void);
	BOOL            ChkItemSpec( netMsg* _pNetMsg );

	void AllinterfaceSend(netMsg *sNetMsg);

	inline VOID ShowDebugMSG(LPCWSTR _DebugMSG);
public:
	afx_msg void OnBnClickedBtnReboot();
};