#include "stdafx.h"
#include "IPCamSearch.h"
#include "IPCamSearchDlg.h"
#include "afxdialogex.h"

#include "KOUpdater.h"

#include "Harace.h"

#include "IpChangeDlg.h"
#include "NotifyDlg.h"
#include "SelectDlg.h"
#include "MacChangeDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	const UINT  ID_TIMER_SEARCH   = 100;
	const UINT  ID_TIMER_STOP     = 200;
	const UINT  ID_TIMER_IPCHANGE = 300;
	const UINT  ID_TIMER_AUTOIP   = 400;
	const UINT  ID_TIMER_DHCP     = 500;
	const UINT  ID_TIMER_MAC      = 600;

	enum
	{
		DEVICE_LIST_NUMBER      = 0,
		DEVICE_LIST_FW_VERSION     ,
		DEVICE_LIST_MAC_ADDRESS    ,
		DEVICE_LIST_IP_ADDRESS     ,
		DEVICE_LIST_GATEWAY        ,
		DEVICE_LIST_NETMASK        ,
		DEVICE_LIST_DNS1           ,
		DEVICE_LIST_DNS2           ,
		DEVICE_LIST_HTTP_PORT      ,
		DEVICE_LIST_RTSP_PORT      ,
		DEVICE_LIST_RUN_TIME       ,
		DEVICE_LIST_UPDATE         ,
		DEVICE_LIST_AUTOIP         ,
		DEVICE_LIST_DHCP           ,
		DEVICE_LIST_MAX
	};

	inline void MakeMacAddressToString(u_int8_t* addr1, CString& str)
	{
		str.Format(L"%02x:%02x:%02x:%02x:%02x:%02x",
				   addr1[0], addr1[1], addr1[2],
				   addr1[3], addr1[4], addr1[5]
				   );
	}
	inline void MakeIPAddressToString(u_int32_t addr1, CString& str)
	{
		str.Format(L"%d.%d.%d.%d",
				   (addr1 & 0xff),
				   ((addr1 >> 8) & 0xff),
				   ((addr1 >> 16) & 0xff),
				   ((addr1 >> 24) & 0xff)
				   );
	}
	inline void MakeIPAddressToZeroFillString(u_int32_t addr1, CString& str)
	{
		str.Format(L"%03d.%03d.%03d.%03d",
				   (addr1 & 0xff),
				   ((addr1 >> 8) & 0xff),
				   ((addr1 >> 16) & 0xff),
				   ((addr1 >> 24) & 0xff)
				   );
	}
	inline void MakeRunningTimeToZeroFillString(unsigned int running_time, CString& str)
	{
		str.Empty();
		if (running_time > 0)
		{
			unsigned int uiSec      = (unsigned int)running_time % 60;
			unsigned int uiMin      = (unsigned int)(running_time / 60) % 60;
			unsigned int uiHour     = (unsigned int)(running_time / 3600)% 24;
			unsigned int uiDay      = (unsigned int)(running_time / 86400);
			str.Format(L"%d Day ( %02d:%02d:%02d )", uiDay, uiHour, uiMin, uiSec);
		}
		else
		{
			str.Format(L"0000:00:00");
		}
	}
	inline void MakeRunningTimeToString(unsigned int running_time, CString& str)
	{
		str.Empty();
		if (running_time > 0)
		{
			unsigned int uiSec      = (unsigned int)running_time % 60;
			unsigned int uiMin      = (unsigned int)(running_time / 60) % 60;
			unsigned int uiHour     = (unsigned int)(running_time / 3600)% 24;
			unsigned int uiDay      = (unsigned int)(running_time / 86400);
			str.Format(L"%d Day ( %02d:%02d:%02d )", uiDay, uiHour, uiMin, uiSec);
		}
	}
	inline int compareMacAddress(u_int8_t* addr1, u_int8_t* addr2)
	{
		CString str1, str2;
		MakeMacAddressToString(addr1, str1);
		MakeMacAddressToString(addr2, str2);
		return str1.Compare(str2);
	}
	inline int compareIPAddress(u_int32_t addr1, u_int32_t addr2)
	{
		CString str1, str2;
		MakeIPAddressToZeroFillString(addr1, str1);
		MakeIPAddressToZeroFillString(addr2, str2);
		return str1.Compare(str2);
	}
	inline int compareRunningTime(u_int32_t running_time1, u_int32_t running_time2)
	{
		CString str1, str2;
		MakeRunningTimeToZeroFillString(ntohl(running_time1), str1);
		MakeRunningTimeToZeroFillString(ntohl(running_time2), str2);
		return str1.Compare(str2);
	}
};

DWORD WINAPI recv_run_thread(LPVOID pObject)
{
	CIPCamSearchDlg * pMain = (CIPCamSearchDlg *)pObject;
	return pMain->RunRecv();
}

// CIPCamSearchDlg dialog

CIPCamSearchDlg::CIPCamSearchDlg(CWnd* pParent /*=NULL*/)
: CDialogEx(CIPCamSearchDlg::IDD, pParent)
, m_hThread(NULL)
, m_hWake(NULL)
, m_sockSend(0)
, m_sockRecv(0)
, m_bSearch(FALSE)
, m_uiSearchTimerID(0)
, m_uiItemCount(0)
, m_strFilePath(_T(""))
, m_arrppUpdateItem(NULL)
, m_bUpdateStartFlg(FALSE)
, m_uiStopTimerID(0)
, m_uiIpChangeTimerID(0)
, m_uiIPChangeSendCount(0)
, m_bContinueIP(FALSE)
, m_bSortAscending(TRUE)
, m_iOldColumn(-1)
, m_pstIpChangeStartItem(NULL)
, m_bAdminMode(FALSE) // Admin Mode Option
, m_AutoIP(FALSE)        // ----- AutoIP
, m_uiAutoIpTimerID(0)
, m_uiAutoIpSendCount(0)
, m_bDhcp(FALSE)         // ----- DHCP
, m_uiDhcpSendCount(0)
, m_uiDhcpTimerID(0)
, m_bMacAddress(FALSE)   // -----  Mac Address
, m_uiMacSendCount(0)
, m_uiMacTimerID(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	// Init UDP Socket
	ZeroMemory(&m_addrSend, sizeof(SOCKADDR_IN));
	ZeroMemory(&m_addrRecv, sizeof(SOCKADDR_IN));
}

void CIPCamSearchDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LST_IPCAM, m_cList);
	DDX_Control(pDX, IDC_PRO_SEARCH, m_ctrlPrgSearch);
}

BEGIN_MESSAGE_MAP(CIPCamSearchDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_MESSAGE( WM_UPDATE_DATA, &CIPCamSearchDlg::OnUpdateStatus )
	ON_MESSAGE( WM_SEARCH_DATA, &CIPCamSearchDlg::OnRecvSearch   )
	ON_NOTIFY( LVN_COLUMNCLICK, IDC_LST_IPCAM, &CIPCamSearchDlg::OnLvnColumnclickListIpcam )
	ON_NOTIFY( LVN_DELETEITEM , IDC_LST_IPCAM, &CIPCamSearchDlg::OnLvnDeleteitemListIpcam  )
	ON_NOTIFY( NM_DBLCLK, IDC_LST_IPCAM, &CIPCamSearchDlg::OnNMDblclkListIpcam )
	ON_NOTIFY( NM_RCLICK, IDC_LST_IPCAM, &CIPCamSearchDlg::OnNMRClickListIpcam )
	/*
	ON_BN_CLICKED( IDC_BTN_SEARCH   , &CIPCamSearchDlg::OnBnClickedButSearch   )
	ON_BN_CLICKED( IDC_BTN_IP_CHANGE, &CIPCamSearchDlg::OnBnClickedBtnIpChange )
	ON_BN_CLICKED( IDC_BTN_UPDATE   , &CIPCamSearchDlg::OnBnClickedBtnUpdate   )
	ON_BN_CLICKED( IDC_BTN_CLEAR    , &CIPCamSearchDlg::OnBnClickedBtnClear    )
	ON_BN_CLICKED( IDC_BTN_DELETE   , &CIPCamSearchDlg::OnBnClickedBtnDelete   )
	ON_BN_CLICKED( IDC_BTN_WEB      , &CIPCamSearchDlg::OnBnClickedBtnWeb      )
	ON_BN_CLICKED( IDC_BTN_REBOOT   , &CIPCamSearchDlg::OnBnClickedBtnReboot   )
	ON_BN_CLICKED( IDC_BTN_DHCP     , &CIPCamSearchDlg::OnBnClickedBtnDhcp     )
	ON_BN_CLICKED( IDC_BTN_AUTOIP   , &CIPCamSearchDlg::OnBnClickedBtnAutoip   )
	ON_BN_CLICKED( IDC_BTN_MAC      , &CIPCamSearchDlg::OnBnClickedBtnMac      )
	*/

	ON_BN_CLICKED( IDC_BTN_PATH     , &CIPCamSearchDlg::OnBnClickedBtnPath     )

	ON_COMMAND_RANGE( IDC_BTN_SEARCH, IDC_BTN_MAC, &CIPCamSearchDlg::OnBnClickedBtnListCtl )
	
END_MESSAGE_MAP()

LRESULT CIPCamSearchDlg::OnUpdateStatus(WPARAM _wParam, LPARAM _lParam)
{
	char buf;
	int nLen(0), nMsgLen = sizeof(buf);
	ZeroMemory(&buf, 1);
	CString strDebug(_T(""));

	// Update Receive Data
	if ((NULL == _wParam) || (NULL == _lParam)) return 0L;

	CKOUpdater*       pUpdate       = (CKOUpdater*)_wParam;
	UPDATE_INFO_ITEM* pstUpdateItem = (UPDATE_INFO_ITEM*)_lParam;

	// Make Temp Data
	UPDATE_INFO_ITEM stUpdateItem;
	memset(&stUpdateItem, 0, sizeof(stUpdateItem));
	memcpy(&stUpdateItem, pstUpdateItem, sizeof(UPDATE_INFO_ITEM));

	// Delete Dynamic Item (LPARAM)
	if (pstUpdateItem) {
		delete pstUpdateItem;
		pstUpdateItem = NULL;
	}

	// ====================================================
	if (pUpdate == m_arrppUpdateItem[stUpdateItem.iItemIdx]) {
		// 아직 있는경우
		int i=100;
	}
	else {
		// 이미 삭제된 경우..
		int i=100;
	}
	// ====================================================


	// Show Update Status !!!
	switch (stUpdateItem.iUpdateStatus)
	{
		case UPDATE_START: {
#if 0
			TRACE(_T("<RECV MESSGE START> : ItemIdx=%d, LstIdx=%d, Vol=%d\n"), stUpdateItem.iItemIdx,
				  stUpdateItem.iLstIdx ,
				  stUpdateItem.iVolume );
#endif
			CString str = _T("");
			str.Format(_T("%d %%"), stUpdateItem.iVolume);
			m_cList.SetItemText(stUpdateItem.iLstIdx, DEVICE_LIST_UPDATE, str); 
			
			if(stUpdateItem.iVolume == 100)
				OperationList(LST_OP_UPDATE); 
			break; }

		case UPDATE_WRITE: {
#if 0
			TRACE(_T("<RECV MESSGE START> : ItemIdx=%d, LstIdx=%d, Vol=%d\n"), stUpdateItem.iItemIdx,
				stUpdateItem.iLstIdx ,
				stUpdateItem.iVolume );
#endif
			CString str = _T("Write");
			m_cList.SetItemText(stUpdateItem.iLstIdx, DEVICE_LIST_UPDATE, str);

			break; }

		case UPDATE_FINISH: {
#if 0
			TRACE(_T("<RECV MESSAGE FINISH> : ItemIdx=%d, LstIdx=%d, Vol=%d\n"), stUpdateItem.iItemIdx,
				  stUpdateItem.iLstIdx ,
				  stUpdateItem.iVolume );
#endif
			CString str = _T("Finish");
			m_cList.SetItemText(stUpdateItem.iLstIdx, DEVICE_LIST_UPDATE, str);

			OperationList(LST_OP_REBOOT);

			// Delete Update Instance
			if (m_arrppUpdateItem[stUpdateItem.iItemIdx])
			{
				delete m_arrppUpdateItem[stUpdateItem.iItemIdx];
				m_arrppUpdateItem[stUpdateItem.iItemIdx] = NULL;
			}
			UpdateFinishCheck(); 

			break; }

		case UPDATE_FAIL: {
#if 0
			TRACE(_T("<RECV MESSAGE FAIL> : ItemIdx=%d, LstIdx=%d, Vol=%d\n"), stUpdateItem.iItemIdx,
				  stUpdateItem.iLstIdx ,
				  stUpdateItem.iVolume );
#endif
			CString str = _T("Fail");
			m_cList.SetItemText(stUpdateItem.iLstIdx, DEVICE_LIST_UPDATE, str);

			// Delete Update Instance
			if (m_arrppUpdateItem[stUpdateItem.iItemIdx])
			{
				delete m_arrppUpdateItem[stUpdateItem.iItemIdx];
				m_arrppUpdateItem[stUpdateItem.iItemIdx] = NULL;
			}
			UpdateFinishCheck(); break; }

		default: { break; }
	}

	return 0L;
}


// 네트워크 상의 모든 인터페이스(IP 주소)를 통해 데이터를 전송하는 역할
void CIPCamSearchDlg::AllinterfaceSend(netMsg *sNetMsg)
{
	char szHostName[128] = "";
	if (gethostname(szHostName, sizeof(szHostName))) {
		AfxMessageBox(L"gethostname Failed!"); return;
	}

	struct hostent *pHost = 0;
	pHost = gethostbyname(szHostName);
	if (!pHost) {
		AfxMessageBox(L"gethostbyname Failed!"); return;
	}

	struct in_addr addr;
	for (int i = 0; pHost->h_addr_list[i]; i++) {
		memcpy(&addr.s_addr, pHost->h_addr_list[i], pHost->h_length);
		setsockopt(m_sockSend, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&addr, sizeof(addr));
		sendto(m_sockSend, (char*)sNetMsg, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
	}
}

// CIPCamSearchDlg message handlers
BOOL CIPCamSearchDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// ToDo :
	ShowDebugMSG(_T("OnInitDlg Start!!"));
	
	SetCtrlInit();

	// (Harae) Show Tittle Version
	CString strVersion = GetFileVersion(NULL);
	CString strCaption = _T("");
	CString str;
	GetWindowText(strCaption);
	strCaption = strCaption + _T(" ( Mutilcast Version ") + strVersion + _T(" )");
	SetDlgItemText(IDC_STT_VERSION,strCaption);

	// Set List Ctrl
	m_cList.SetExtendedStyle( LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT );
	str.LoadString(IDS_DEVICE_LIST_COL0);
	m_cList.InsertColumn( DEVICE_LIST_NUMBER, str, LVCFMT_CENTER, 30, 0);
	str.LoadString(IDS_DEVICE_LIST_COL1);
	m_cList.InsertColumn(DEVICE_LIST_FW_VERSION, str, LVCFMT_CENTER, 90, 0);
	str.LoadString(IDS_DEVICE_LIST_COL2);
	m_cList.InsertColumn(DEVICE_LIST_MAC_ADDRESS, str, LVCFMT_CENTER, 105, 0);
	str.LoadString(IDS_DEVICE_LIST_COL3);
	m_cList.InsertColumn(DEVICE_LIST_IP_ADDRESS, str, LVCFMT_CENTER, 96, 0);
	str.LoadString(IDS_DEVICE_LIST_COL4);
	m_cList.InsertColumn(DEVICE_LIST_GATEWAY, str, LVCFMT_CENTER, 96, 0);
	str.LoadString(IDS_DEVICE_LIST_COL5);
	m_cList.InsertColumn(DEVICE_LIST_NETMASK, str, LVCFMT_CENTER, 96, 0);
	str.LoadString(IDS_DEVICE_LIST_COL6);
	m_cList.InsertColumn(DEVICE_LIST_DNS1, str, LVCFMT_CENTER, 96, 0);
	str.LoadString(IDS_DEVICE_LIST_COL7);
	m_cList.InsertColumn(DEVICE_LIST_DNS2, str, LVCFMT_CENTER, 96, 0);
	str.LoadString(IDS_DEVICE_LIST_COL8);
	m_cList.InsertColumn(DEVICE_LIST_HTTP_PORT, str, LVCFMT_CENTER, 55, 0);
	str.LoadString(IDS_DEVICE_LIST_COL9);
	m_cList.InsertColumn(DEVICE_LIST_RTSP_PORT, str, LVCFMT_CENTER, 55, 0);
	str.LoadString(IDS_DEVICE_LIST_COL10);
	m_cList.InsertColumn(DEVICE_LIST_RUN_TIME, str, LVCFMT_CENTER, 105, 0);
	str.LoadString(IDS_DEVICE_LIST_COL11);
	m_cList.InsertColumn(DEVICE_LIST_UPDATE, str, LVCFMT_CENTER, 55, 0);
	if (m_bAdminMode){
		str.LoadString(IDS_DEVICE_LIST_COL12);
		m_cList.InsertColumn(DEVICE_LIST_AUTOIP, str, LVCFMT_CENTER, 40, 0);
	}
	str.LoadString(IDS_DEVICE_LIST_COL13);
	m_cList.InsertColumn(DEVICE_LIST_DHCP, str, LVCFMT_CENTER, 40, 0);

	// Create Wake Event
	m_hWake = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Init Mulitcast Socket Create and Setup
	if ((m_sockSend = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == INVALID_SOCKET) {
		AfxMessageBox(L"Send Socket Create Failed!"); return FALSE;
	}

	int timeLive = 63;
	if ((setsockopt(m_sockSend, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&timeLive, sizeof(timeLive))) == INVALID_SOCKET) {
		AfxMessageBox(L"Send Socket Mulitdcast Option Setting Failed - MULTICAST_TTL!"); return FALSE;
	}
	WCHAR szDebug[100] = { 0, };
	wsprintf(szDebug, _T("Create SendSocket : %d \n"), m_sockSend);
	ShowDebugMSG(szDebug);

	memset(&m_addrSend, 0, sizeof(m_addrSend));
	m_addrSend.sin_family = AF_INET;
	m_addrSend.sin_port = htons(SEND_PORT);
	m_addrSend.sin_addr.s_addr = inet_addr("239.255.255.250");

	m_ctrlPrgSearch.SetRange(0, 50);
	m_ctrlPrgSearch.SetPos(0);

	// Create Brush
	m_cBrush.CreateSolidBrush(BACK_COLOR);

	// Set Disable Buttons
	SetUiBtnEnableWindow( FALSE, 1 );

	// Show Mode Status
	SetDlgItemText(IDC_STT_MODE, (m_bAdminMode) ? _T("ADMIN MODE") : _T("NORMAL MODE") );

	return TRUE;
}
VOID CIPCamSearchDlg::SetCtrlInit(void)
{
	this->MoveWindow(CRect(0,0,1100,600));

	GetDlgItem(IDC_BTN_SEARCH)->MoveWindow(CRect(10, 20, 60, 45));
	m_ctrlPrgSearch.MoveWindow(CRect(63, 21,280,44));

	int iStartBtnLeft  = 285;
	int iStartBtnRight = iStartBtnLeft + 66;
	GetDlgItem(IDC_BTN_IP_CHANGE)->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45)); iStartBtnLeft = iStartBtnRight+1; iStartBtnRight = iStartBtnLeft+66;
	GetDlgItem(IDC_BTN_UPDATE   )->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45)); iStartBtnLeft = iStartBtnRight+1; iStartBtnRight = iStartBtnLeft+66;
	GetDlgItem(IDC_BTN_CLEAR    )->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45)); iStartBtnLeft = iStartBtnRight+1; iStartBtnRight = iStartBtnLeft+66;
	GetDlgItem(IDC_BTN_DELETE   )->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45)); iStartBtnLeft = iStartBtnRight+1; iStartBtnRight = iStartBtnLeft+66;
	GetDlgItem(IDC_BTN_WEB      )->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45)); iStartBtnLeft = iStartBtnRight+1; iStartBtnRight = iStartBtnLeft+66;
	GetDlgItem(IDC_BTN_DHCP     )->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45)); iStartBtnLeft = iStartBtnRight+1; iStartBtnRight = iStartBtnLeft+66;
	GetDlgItem(IDC_BTN_REBOOT   )->MoveWindow(CRect(iStartBtnLeft,20, iStartBtnRight, 45));
	GetDlgItem(IDC_EDT_PATH     )->MoveWindow(CRect(760,20, 1040, 45));
	GetDlgItem(IDC_BTN_PATH     )->MoveWindow(CRect(1045,20, 1075, 45));
	
	m_cList.MoveWindow(CRect( 10, 50, 1075, 530) );
	GetDlgItem(IDC_STT_MODE)->MoveWindow( 20, 535, 80, 20);
	
	int iStartAdminBtnLeft(100);
	int iStartAdminBtnRight = iStartAdminBtnLeft + 66;
	GetDlgItem(IDC_BTN_AUTOIP)->MoveWindow(CRect(iStartAdminBtnLeft, 533, iStartAdminBtnRight, 558)); iStartAdminBtnLeft = iStartAdminBtnRight+1; iStartAdminBtnRight = iStartAdminBtnLeft+66;
	GetDlgItem(IDC_BTN_MAC)->MoveWindow(CRect(iStartAdminBtnLeft, 533, iStartAdminBtnRight, 558));

	GetDlgItem(IDC_STT_VERSION)->MoveWindow( 800, 535, 300, 20);
	
	// Not Support
	GetDlgItem(IDC_BTN_REBOOT   )->EnableWindow(FALSE); // Not Support & Use	

	// Only Show Admin Mode
	if (!m_bAdminMode){
		GetDlgItem(IDC_BTN_AUTOIP)->ShowWindow(SW_HIDE);
		GetDlgItem(IDC_BTN_MAC   )->ShowWindow(SW_HIDE);
	}
}

BOOL CIPCamSearchDlg::ShowIPChangeDlg(void)
{
	// Get First Pos IP
	POSITION pos = m_cList.GetFirstSelectedItemPosition();
	if (NULL == pos) return FALSE;

	// Get Selected Item Count
	m_uiItemCount = m_cList.GetSelectedCount();
	netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));

	// Clear First Change Item
	if (m_pstIpChangeStartItem){
		delete m_pstIpChangeStartItem;
		m_pstIpChangeStartItem = NULL;
	}
	
	CIpChangeDlg Dlg;
	Dlg.SetInitInfo( m_uiItemCount, pstItem );
	
	if ( IDOK == Dlg.DoModal() ){
		TRACE(_T("IP Change IP Start \n"));

		// Set Disable Buttons
		SetUiBtnEnableWindow( FALSE, 1 );

		// Make First Chagne
		m_pstIpChangeStartItem = new netMsg;
		memset(m_pstIpChangeStartItem, 0, sizeof(netMsg));
		Dlg.GetSetupInfoResult( m_bContinueIP, m_pstIpChangeStartItem );

		// IP Change Timer Start Just 5 Send
		StartIpChangeTimer();
	}
	else{
		TRACE(_T("IP Change Cancel \n"));

		// Set Disable Buttons
		SetUiBtnEnableWindow( TRUE, 1 );
		return FALSE;
	}
	return TRUE;
}

BOOL CIPCamSearchDlg::IPCangeStart( const UINT iCount, const BOOL _bContinueIP, const netMsg* _pItem )
{
	if (NULL == _pItem) return FALSE;
	if (1 == iCount){
		netMsg stSetupItem;
		memset( &stSetupItem, 0, sizeof(netMsg) );
		memcpy( &stSetupItem, _pItem, sizeof(netMsg));

		// Change New IP
		stSetupItem.opcode  = MSG_IP_SET;
		stSetupItem.nettype = NET_STATIC;

#if (1)
		AllinterfaceSend(&stSetupItem);
#else
		int iSendLen = sendto(m_sockSend, (char*)&stSetupItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
		return TRUE;
	}

	// Get Start IP
	CString strStartIP(_T(""));
	MakeIPAddressToString( _pItem->yiaddr, strStartIP );
	
	// Update Start
	POSITION pos = m_cList.GetFirstSelectedItemPosition();
	int iItemIdx(0), iAutoIP(0);
	do{
		if (NULL == pos) break;

		int iListIndex = m_cList.GetNextSelectedItem(pos);
		netMsg*  pItem = (netMsg*)m_cList.GetItemData(iListIndex);

		int iChangeIP(0);
		for (int i=0; i<4; i++){
			CString strTemp(_T(""));
			AfxExtractSubString(strTemp, strStartIP, i, '.');
			switch (i){
				case 0:{ iChangeIP |= (_ttoi(strTemp) <<  0);  break; }
				case 1:{ iChangeIP |= (_ttoi(strTemp) <<  8);  break; }
				case 2:{ iChangeIP |= (_ttoi(strTemp) << 16);  break; }
				case 3:{
					if (_bContinueIP)
						iChangeIP |= ((_ttoi(strTemp)+iAutoIP++) << 24);
					else
						iChangeIP |= ((_ttoi(strTemp)) << 24);

					break; }

				default:{ ASSERT(0); break; }
			}
		}
		// Change Gateway ( All Same Static )
		pItem->giaddr = _pItem->giaddr;

		// Change New IP
		pItem->opcode  = MSG_IP_SET;
		pItem->yiaddr  = iChangeIP;
		pItem->nettype = NET_STATIC;

#if (1)
		AllinterfaceSend(pItem);
#else
		int iSendLen = sendto( m_sockSend, (char*)pItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif

		iItemIdx++;
	} while (pos);

	return TRUE;
}
void CIPCamSearchDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	CDialogEx::OnSysCommand(nID, lParam);
}
HCURSOR CIPCamSearchDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
void CIPCamSearchDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

BOOL CIPCamSearchDlg::StartStopTimer(void)
{
	FinishStopTimer();

	// Search Prograss & Serch MSG Start
	m_uiStopTimerID = SetTimer( ID_TIMER_STOP, 4000, NULL );
	return TRUE;
}
BOOL CIPCamSearchDlg::FinishStopTimer(void)
{
	// Search Stop Kill
	if (m_uiStopTimerID) {
		if (this->GetSafeHwnd())
			KillTimer(m_uiStopTimerID);
		m_uiStopTimerID = 0;
	}
	return TRUE;
}
BOOL CIPCamSearchDlg::StartSearchTimer(void)
{
	FinishSearchTimer();

	// Search Prograss & Serch MSG Start
	m_uiSearchTimerID = SetTimer( ID_TIMER_SEARCH, 200, NULL );

	return TRUE;
}
BOOL CIPCamSearchDlg::FinishSearchTimer(void)
{
	// Search Prograss Kill
	if (m_uiSearchTimerID) {
		if (this->GetSafeHwnd())
			KillTimer(m_uiSearchTimerID);
		m_uiSearchTimerID = 0;
		m_ctrlPrgSearch.SetPos(0);
		}
	return TRUE;
}
BOOL CIPCamSearchDlg::StartIpChangeTimer ( void )
{
	FinishIpChangeTimer();

	// IP Change MSG Send Start
	m_uiIpChangeTimerID = SetTimer( ID_TIMER_IPCHANGE, 200, NULL );
	return TRUE;
}
BOOL CIPCamSearchDlg::FinishIpChangeTimer( void )
{
	if (m_uiIpChangeTimerID) {
		if (this->GetSafeHwnd())
			KillTimer(m_uiIpChangeTimerID);
		m_uiIpChangeTimerID = 0;
		m_uiIPChangeSendCount = 0;
	}
	return TRUE;
}
BOOL CIPCamSearchDlg::StartAutoIpTimer ( void )
{
	FinishAutoIpTimer();

	// Auto IP Change MSG Send Start
	m_uiAutoIpTimerID = SetTimer( ID_TIMER_AUTOIP, 200, NULL );
	return TRUE;
}
BOOL CIPCamSearchDlg::FinishAutoIpTimer( void )
{
	if (m_uiAutoIpTimerID) {
		if (this->GetSafeHwnd())
			KillTimer(m_uiAutoIpTimerID);
		m_uiAutoIpTimerID = 0;
		m_uiAutoIpSendCount = 0;
	}
	return TRUE;
}
BOOL CIPCamSearchDlg::StartDhcpTimer(void)
{
	FinishDhcpTimer();

	// DHCP Change MSG Send Start
	m_uiDhcpTimerID = SetTimer( ID_TIMER_DHCP, 200, NULL );
	return TRUE;
}
BOOL CIPCamSearchDlg::FinishDhcpTimer(void)
{
	if (m_uiDhcpTimerID) {
		if (this->GetSafeHwnd())
			KillTimer(m_uiDhcpTimerID);
		m_uiDhcpTimerID   = 0;
		m_uiDhcpSendCount = 0;
	}
	return TRUE;
}
BOOL CIPCamSearchDlg::StartMacTimer  ( void )
{
	FinishMacTimer();
	
	// MAC Change MSG Send Start
	m_uiMacTimerID = SetTimer( ID_TIMER_MAC, 200, NULL );

	return TRUE;
}
BOOL CIPCamSearchDlg::FinishMacTimer ( void )
{
	if (m_uiMacTimerID) {
		if (this->GetSafeHwnd())
			KillTimer(m_uiMacTimerID);
		m_uiMacTimerID   = 0;
		m_uiMacSendCount = 0;
	}
	return TRUE;
}

BOOL CIPCamSearchDlg::SendSerarchCmd(void)
{
	if (FALSE == m_bSearch)
		return FALSE;

	DWORD dwTick = GetTickCount();
	netMsg sNetMsg ={ 0 };
	sNetMsg.opcode = (u_int8_t)MSG_IP_SEARCH; // operation ip search - IP 카메라 검색 명령 설정
	sNetMsg.magic  = htonl(MAGIC);            // magic code - 매직코드
	sNetMsg.xid    = htonl(dwTick);           // transaction id, random number - 검색 인덱스 구분을 위한 램덤 번호

#if (1)
	AllinterfaceSend(&sNetMsg);
#else
	int iSendLen = sendto(m_sockSend, (char*)&sNetMsg, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
	return TRUE;
}

// 이 함수는 메시지가 카메라 응답인지 확인하고, 올바른 매직 코드를 가지고 있는지 확인
BOOL CIPCamSearchDlg::ChkItemSpec( netMsg* _pNetMsg )
{
	// Only if the camera response processing
	if (_pNetMsg->opcode != MSG_CAM_ACK) return FALSE;
	
	// Only if the response processing  that matches the Magic Code.
	if (_pNetMsg->magic != htonl(MAGIC)) return FALSE;

	// Only if the porcessing that Lowend model.
	CString strTemp = L"";
	USES_CONVERSION; // MultiByteToWideChar
	strTemp.Format(L"%s", A2W((char*)_pNetMsg->fw_ver));
	CString strCutTemp = strTemp.Left(3);
	strTemp.ReleaseBuffer();
		
	/*if (0 != strCutTemp.Compare(_T("SYS")))
		return FALSE;*/
#if 1
	// High End // ( UfineM1_ )
	if (0 == strCutTemp.Compare(_T("Ufi")))
		return FALSE;
#endif



	// Check List Duplication.
	int iCurItemCount = m_cList.GetItemCount();
	
	CString strCount;
		strCount.Format(_T("%d"),iCurItemCount);
	if (1 > iCurItemCount)
		return TRUE;
		
	for (int i=0; i<iCurItemCount; i++){
		netMsg* pstItem = (netMsg*)m_cList.GetItemData(i);

		// Check Same MacAddress.
		CString strRecv  (_T("")),
			    strTarget(_T(""));

		MakeMacAddressToString( _pNetMsg->chaddr, strRecv );
		MakeMacAddressToString( pstItem->chaddr, strTarget );

		if (0 == strRecv.Compare(strTarget)){
			// Change xid
			pstItem->xid = _pNetMsg->xid;
			return FALSE;
		}
	}

	// Send ack message - 정상적으로 응답을 받았다고 아이피 카메라에 통보
	_pNetMsg->opcode = (u_int8_t)MSG_IP_ACK;
#if (1)
	AllinterfaceSend(_pNetMsg);
#else
	sendto(m_sockSend, (char*)_pNetMsg, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
	
	return TRUE;
}

inline void CIPCamSearchDlg::UpdateColumnName(int column_index, BOOL is_ascending)
{
	ASSERT(column_index != DEVICE_LIST_UPDATE);
	CHeaderCtrl* header_ctrl1 = m_cList.GetHeaderCtrl();
	WCHAR        text_buffer[255] ={ 0, };
	LVCOLUMN     lvColumn;
	CString      str;
	//header_ctrl1->GetItemCount();
	for (int i = DEVICE_LIST_FW_VERSION; i < DEVICE_LIST_UPDATE; ++i)
	{
		lvColumn.mask = LVCF_TEXT;
		str.LoadString(IDS_DEVICE_LIST_COL0 + i);
		_stprintf_s(text_buffer, 255, L"%s%s", (i == column_index) ? ((is_ascending) ? L"+ " : L"- ") : L"", str);
		lvColumn.pszText = text_buffer;
		m_cList.SetColumn(i, &lvColumn);
	}
}

inline void CIPCamSearchDlg::ShowRecvMsg(netMsg* pNetMsg)
{
	int nSubItem = 0, nItem = m_cList.GetItemCount();
	nItem = m_cList.InsertItem(nItem, L"IpCam");

	// store netMsg information into list item data, it will be clear when list item deleted(delete callback message)
	netMsg* copyedNetMsg = new netMsg;
	*copyedNetMsg = *pNetMsg;
	m_cList.SetItemData(nItem, (DWORD_PTR)copyedNetMsg);
	//m_cList.SetItemData(,0)

	CString str = L"";
	// Item Number
	str.Format(L"%d", nItem+1);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// FW Version.
	USES_CONVERSION; // MultiByteToWideChar
	str.Format(L"%s", A2W((char*)pNetMsg->fw_ver));
	int version = 0;
	version = _ttoi(str);
	str.Format(L"%x", version);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// MAC Address
	MakeMacAddressToString(pNetMsg->chaddr, str);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// Ip Address
	MakeIPAddressToString(pNetMsg->ciaddr, str);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// Gateway
	MakeIPAddressToString(pNetMsg->giaddr, str);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// Subnet Mask
	MakeIPAddressToString(pNetMsg->miaddr, str);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// DNS 1
	MakeIPAddressToString(pNetMsg->d1iaddr, str);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// DNS 2
	MakeIPAddressToString(pNetMsg->d2iaddr, str);
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// Http Port
	str.Format(L"%d", ntohl(pNetMsg->http));// Change Byte Order
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// Rtsp Port
	str.Format(L"%d", ntohl(pNetMsg->stream));
	m_cList.SetItemText(nItem, nSubItem, str); nSubItem++;
	str.ReleaseBuffer();

	// Run Time	
	unsigned int uiTotalSec = (unsigned int)ntohl(pNetMsg->runningtime);
	if (0 < uiTotalSec){
		unsigned int uiSec      = (unsigned int)uiTotalSec % 60;
		unsigned int uiMin      = (unsigned int)(uiTotalSec / 60) % 60;
		unsigned int uiHour     = (unsigned int)(uiTotalSec / 3600)% 24;
		unsigned int uiDay      = (unsigned int)(uiTotalSec / 86400);
		str.Format(L"%d Day ( %02d:%02d:%02d )", uiDay, uiHour, uiMin, uiSec);

		m_cList.SetItemText(nItem, nSubItem, str);
		str.ReleaseBuffer();
	}
	nSubItem++;

	// Update
	str = _T("Ready");
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer(); nSubItem++;

	// When Admin Mode, Show Auto IP Status.
	if (m_bAdminMode){
		str = (ntohl(pNetMsg->flag) & (0x1 << FLAG_STATUS_AUTOIP)) ? _T("ON") : _T("OFF");

		m_cList.SetItemText(nItem, nSubItem, str);
		str.ReleaseBuffer(); nSubItem++;
	}

	// DHCP Status
	str = ( ntohl(pNetMsg->flag) & (0x1 << FLAG_STATUS_DHCP)) ?  _T("ON") : _T("OFF");	
	m_cList.SetItemText(nItem, nSubItem, str);
	str.ReleaseBuffer();
}

DWORD CIPCamSearchDlg::RunRecv()
{
	char buf[256] ={ 0, };
	int nLen(0), nMsgLen = sizeof(netMsg);
	while (m_bSearch) {
		ZeroMemory(&buf, 256);
		nLen = recvfrom(m_sockRecv, (char*)buf, 256, 0, NULL, 0);

		WCHAR szDebug[100]={ 0, };
		wsprintf(szDebug, _T("Recv Socket %d Len %d \n"), m_sockRecv, nLen);
		ShowDebugMSG(szDebug);

		if (nLen < 0) break;
		if (nLen == 0) {
			Sleep(100); continue;
		}

		// Make Dynamic Item
		netMsg* pItem = new netMsg;
		memset( pItem, 0, sizeof(netMsg) );
		memcpy( pItem, (netMsg*)buf, sizeof(netMsg) );

		// Struct Modify( Add Running Time )
		if (nLen == nMsgLen || nLen == nMsgLen-sizeof(u_int32_t)){
			if (this->GetSafeHwnd()){
				PostMessage( WM_SEARCH_DATA, (WPARAM)pItem );
			//ShowDebugMSG(_T("Recv +++++++ \n"));
			}
		}

		Sleep(1);
	}
	WaitForSingleObject(m_hWake, INFINITE);
	return 0L;
}

LRESULT CIPCamSearchDlg::OnRecvSearch( WPARAM _wParam, LPARAM _lParam )
{
	if ( NULL == _wParam ) return 0L;

	netMsg* pItem = (netMsg*)_wParam;

	// Check Item Inspection ( Duplication or Etc .. )
	if( ChkItemSpec(pItem) )
		ShowRecvMsg(pItem);
		
	if(pItem){
		delete pItem;
		pItem = NULL;
	}

	return 0L;
}

BOOL CIPCamSearchDlg::ShowSubMenuSetup(void)
{
	CString strMenu = _T("");

	CMenu menu;
	menu.CreatePopupMenu();
	strMenu.LoadStringW(IDS_STR_MENU_IPCHANGE);
	menu.AppendMenuW(MF_STRING, IDC_MENU_IPCHANGE, strMenu);
	strMenu.LoadStringW(IDS_STR_MENU_UPDATE);
	menu.AppendMenuW(MF_STRING, IDC_MENU_UPDATE, strMenu);
	strMenu.LoadStringW(IDS_STR_MENU_DELETE);
	menu.AppendMenuW(MF_STRING, IDC_MENU_DELETE, strMenu);
	strMenu.LoadStringW(IDS_STR_MENU_DHCP);
	menu.AppendMenuW(MF_STRING, IDC_MENU_DHCP, strMenu);
	strMenu.LoadStringW(IDS_STR_MENU_REBOOT);
	menu.AppendMenuW(MF_STRING, IDC_MENU_REBOOT, strMenu);

	if (m_bAdminMode){
		strMenu.LoadStringW(IDS_STR_MENU_AUTOIP);
		menu.AppendMenuW(MF_STRING, IDC_MENU_AUTOIP, strMenu);
	}

	// Get Mouse Position
	CPoint p;
	GetCursorPos(&p);
	unsigned int uiMenuID = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, this);

	// Menu Control
	switch (uiMenuID) {
		case IDC_MENU_IPCHANGE: { //OnBnClickedBtnIpChange();
			OnBnClickedBtnListCtl(IDC_BTN_IP_CHANGE);
			break; }

		case IDC_MENU_UPDATE  : {// OnBnClickedBtnUpdate();
			OnBnClickedBtnListCtl(IDC_BTN_UPDATE);
			break; }

		case IDC_MENU_DELETE  : { OperationList(LST_OP_DELETE); break; }

		case IDC_MENU_DHCP    : { //OnBnClickedBtnDhcp();
			OnBnClickedBtnListCtl(IDC_BTN_DHCP);
			break; }

		case IDC_MENU_REBOOT  : { //OnBnClickedBtnReboot();
			OnBnClickedBtnListCtl(IDC_BTN_REBOOT);
			break; }
				
		case IDC_MENU_AUTOIP  : { //OnBnClickedBtnAutoip();
			OnBnClickedBtnListCtl(IDC_BTN_AUTOIP);
			break; }
		default: { break; }
	}
	menu.DestroyMenu();
	return TRUE;
}
BOOL CIPCamSearchDlg::ShowSubMenuClear(void)
{
	CString strMenu = _T("");

	if (0 >= m_cList.GetItemCount())
		return FALSE;

	CMenu menu;
	menu.CreatePopupMenu();

	strMenu.LoadStringW(IDS_STR_MENU_CLEAR);
	menu.AppendMenuW(MF_STRING, IDC_MENU_CLEAR, strMenu);

	// Get Mouse Position
	CPoint p;
	GetCursorPos(&p);
	unsigned int uiMenuID = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, this);

	switch (uiMenuID)
	{
		case IDC_MENU_CLEAR: {
			m_cList.DeleteAllItems();
			break; }
		default: { break; }
	}

	menu.DestroyMenu();
	return TRUE;
}
BOOL CIPCamSearchDlg::ShowSubMenuCancel(void)
{
	CString strMenu = _T("");

	CMenu menu;
	menu.CreatePopupMenu();

	strMenu.LoadStringW(IDS_STR_MENU_CANCEL);
	menu.AppendMenuW(MF_STRING, IDC_MENU_CANCEL, strMenu);

	// Get Mouse Position
	CPoint p;
	GetCursorPos(&p);
	unsigned int uiMenuID = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, this);

	switch (uiMenuID)
	{
		case IDC_MENU_CANCEL: {
			// All Update Cancel Notify
			CNotifyDlg Dlg;
			Dlg.setInitStyle( m_uiItemCount, 1 );
			if (IDCANCEL == Dlg.DoModal())
				return FALSE;

			// Find Now Updating Module
			for (int i=0; i <(int)m_uiItemCount; i++)
			{
				if (NULL != m_arrppUpdateItem[i])
				{
					int iItemIdx = 0,
						iLstIdx  = 0;
					m_arrppUpdateItem[i]->GetIndex(iItemIdx, iLstIdx);

					POSITION pos = m_cList.GetFirstSelectedItemPosition();

					do{
						if (NULL == pos) break;
						if (iLstIdx == m_cList.GetNextSelectedItem(pos))
							UpdateCancel(iItemIdx);
					} while (pos);
				}
			}
			break; }
		default: { break; }
	}

	menu.DestroyMenu();

	return TRUE;
}
void CIPCamSearchDlg::OnNMRClickListIpcam(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	// Check Search Status
	if (m_bSearch) return;

	// Get First Position from ListView
	POSITION stFirstPos = m_cList.GetFirstSelectedItemPosition();

	if (stFirstPos) {
		if (!m_bUpdateStartFlg)
			ShowSubMenuSetup();
#if 0
		if (m_bUpdateStartFlg)
			ShowSubMenuCancel();
		else
			ShowSubMenuSetup();
#endif
	}
	else {
		if (false == m_bUpdateStartFlg)
			ShowSubMenuClear();
	}

	*pResult = 0;
}

void CIPCamSearchDlg::OnTimer(UINT_PTR nIDEvent)
{
	switch (nIDEvent)
	{
		case ID_TIMER_SEARCH: {
			// Send Serarch Cmd
			SendSerarchCmd();

			if (50 > m_ctrlPrgSearch.GetPos())
				m_ctrlPrgSearch.SetPos(m_ctrlPrgSearch.GetPos() + 1);

			if (50 == m_ctrlPrgSearch.GetPos()){				
				//OnBnClickedButSearch();
				OnBnClickedBtnListCtl(IDC_BTN_SEARCH);
			}
			break; }

		case ID_TIMER_STOP: {

			FinishStopTimer();
			GetDlgItem(IDC_BTN_SEARCH)->EnableWindow(TRUE);



			// 검색 중이면 중단
			SetDlgItemText(IDC_BTN_SEARCH, L"START");
			m_bSearch = FALSE;
			closesocket(m_sockRecv);
			m_sockRecv = 0;
			SetEvent(m_hWake);
			WaitForSingleObject(m_hThread, INFINITE);
			CloseHandle(m_hThread); m_hThread = NULL;

			// UI Button Enable
			SetUiBtnEnableWindow(TRUE, 1);

			// List Heaer Lock
			CHeaderCtrl* pCLstCtrlHeader = m_cList.GetHeaderCtrl();
			pCLstCtrlHeader->EnableWindow(TRUE);

			break; }
		case ID_TIMER_IPCHANGE: {
			if (5 == m_uiIPChangeSendCount){
				FinishIpChangeTimer();
				//OnBnClickedButSearch();
				OnBnClickedBtnListCtl(IDC_BTN_SEARCH);

				if (m_pstIpChangeStartItem){
					delete m_pstIpChangeStartItem;
					m_pstIpChangeStartItem = NULL;
				}
				return;
			}
				
			m_uiIPChangeSendCount++;

			// Send IP Change Command
			IPCangeStart( m_uiItemCount, m_bContinueIP, m_pstIpChangeStartItem );
			break; }

		case ID_TIMER_AUTOIP:{
			if (5 == m_uiAutoIpSendCount){
				FinishAutoIpTimer();
				//OnBnClickedButSearch();
				OnBnClickedBtnListCtl(IDC_BTN_SEARCH);
				return;
			}
			
			// Send Auto IP Set
			m_uiAutoIpSendCount++;
			OperationList(LST_OP_AUTOIP);
			break; }

		case ID_TIMER_DHCP: {
			if (5 == m_uiDhcpSendCount){
				FinishDhcpTimer();
				//OnBnClickedButSearch();
				OnBnClickedBtnListCtl(IDC_BTN_SEARCH);
				return;
			}

			// Send Auto IP Set
			m_uiDhcpSendCount++;
			OperationList(LST_OP_DHCP);
			break; }
		
		case ID_TIMER_MAC: {
			if (5 == m_uiMacSendCount){

				if (m_pstIpChangeStartItem)
					delete m_pstIpChangeStartItem;
				m_pstIpChangeStartItem = NULL;

				//
				FinishMacTimer();
				//OnBnClickedButSearch();
				OnBnClickedBtnListCtl(IDC_BTN_SEARCH);
				return;
			}

			// Send Auto IP Set
			m_uiMacSendCount++;
			OperationList(LST_OP_MAC_CHANGE);
			break; }

		default: { break; }
	}
	CDialogEx::OnTimer(nIDEvent);
}

BOOL CIPCamSearchDlg::ChkLstDuplicationDevice(void)
{
	// Check Duplication IP & MAC
	if ( (TRUE == ChkLstDuplicationItem(1)) && (TRUE == ChkLstDuplicationItem(2)) )
		return TRUE;

	return FALSE;
}

BOOL CIPCamSearchDlg::ChkLstDuplicationItem(int _iSubItem)
{
	// Make Element List
	CStringList lstElement;
	lstElement.RemoveAll();

	// Get Item Info
	for (int i=0; i < m_cList.GetItemCount(); i++){
		CString strItem = _T("");
		strItem = m_cList.GetItemText(i, _iSubItem);
		lstElement.AddTail(strItem);
	}

	// Check Search Item Info
	POSITION posLstElementHead = lstElement.GetHeadPosition(),
		posTarget         = posLstElementHead;

	for (int i=0; i<lstElement.GetCount(); i++){
		CString  strTarget = lstElement.GetNext(posTarget),
			strItem   = _T("");
		POSITION posItem   = posLstElementHead;

		for (int j = 0; j < lstElement.GetCount(); j++) {
			// Get Item
			strItem = lstElement.GetNext(posItem);
			// Pass Same Item
			if (j <= i) continue;
			// Comapre Item
			if (0 == strTarget.Compare(strItem))
				return TRUE;
		}
	}
	return FALSE;
}
BOOL CIPCamSearchDlg::IsNewVersion(CString &_strVersion)
{
/*
	CString version_string = _strVersion;
	int     parse_pos      = 0;
	CString model          = version_string.Tokenize(_T("-"), parse_pos);
	if (parse_pos < 0){
		// If Get No Data... Bagic Value Set (Harace)
		return TRUE;
	}
	else {
		CString date_string = version_string.Mid(parse_pos);
		ASSERT(date_string.GetLength() > 0);
		if (date_string.Compare(_T("14-04-28-1")) < 0)
			return FALSE;
	}
*/
	return TRUE;
}

BOOL CIPCamSearchDlg::UpdateFinishCheck(void)
{
	if (m_bUpdateStartFlg){
		int iChkCount = 0;
		for (int i =0; i < (int)m_uiItemCount; i++) {
			if (m_arrppUpdateItem[i] == NULL)
				iChkCount++;
		}

		if (iChkCount == m_uiItemCount) {
			
			// Enable UI Button
			SetUiBtnEnableWindow();

			// Delete All Update Item
			if (m_arrppUpdateItem) {
				delete[] m_arrppUpdateItem;
				m_arrppUpdateItem = NULL;
			}
			m_bUpdateStartFlg = FALSE;
		}
	}
	else
		return FALSE;

	return TRUE;
}

BOOL CIPCamSearchDlg::UpdateStart(void)
{
	// Get Selected Item Count
	m_uiItemCount = m_cList.GetSelectedCount();
	if ( 0 == m_uiItemCount )
		return FALSE;

	// Update Dlg Notify
	CNotifyDlg Dlg;
	Dlg.setInitStyle( m_uiItemCount, 0 );
	if (IDCANCEL == Dlg.DoModal()){
		m_uiItemCount = 0;
		return FALSE;
	}

	// Disable UI Button
	SetUiBtnEnableWindow( FALSE );

	// Update Flg
	m_bUpdateStartFlg = TRUE;

	// Make Update Items    
	m_arrppUpdateItem = new CKOUpdater*[m_uiItemCount];

	// Update Start
	POSITION pos = m_cList.GetFirstSelectedItemPosition();
	int iItemIdx  = 0;
	do{
		if (NULL == pos) break;

		int iListIndex = m_cList.GetNextSelectedItem(pos);

		m_arrppUpdateItem[iItemIdx] = new CKOUpdater();
		m_arrppUpdateItem[iItemIdx]->Create(this);
		m_arrppUpdateItem[iItemIdx]->SetIndex(iItemIdx, iListIndex, TRUE);
		m_arrppUpdateItem[iItemIdx]->SetUpdateInfo(m_cList.GetItemText(iListIndex, DEVICE_LIST_IP_ADDRESS), // IP
												  _ttoi(m_cList.GetItemText(iListIndex, DEVICE_LIST_HTTP_PORT)), // Port
												  m_strFilePath, // File Path
												  IsNewVersion(m_cList.GetItemText(iListIndex, DEVICE_LIST_FW_VERSION))); // Version
		m_arrppUpdateItem[iItemIdx]->UpdateStart();

		iItemIdx++;
	} while (pos);

	return TRUE;
}

BOOL CIPCamSearchDlg::SetUiBtnEnableWindow( const BOOL _bLock, const int _iOption )
{
	CWnd* pWnd = NULL;
	switch (_iOption){
		case 0:{ // Update Doing
			pWnd = GetDlgItem(IDC_BTN_PATH);
			if (pWnd) pWnd->EnableWindow(_bLock); }
		
		case 1:{ // Search
			pWnd = GetDlgItem(IDC_BTN_IP_CHANGE);
			if (pWnd) pWnd->EnableWindow(_bLock);
			pWnd = GetDlgItem(IDC_BTN_UPDATE);
			if (pWnd) pWnd->EnableWindow(_bLock);
			pWnd = GetDlgItem(IDC_BTN_CLEAR);
			if (pWnd) pWnd->EnableWindow(_bLock);
			pWnd = GetDlgItem(IDC_BTN_DELETE);
			if (pWnd) pWnd->EnableWindow(_bLock);
			pWnd = GetDlgItem(IDC_BTN_WEB);
			if (pWnd) pWnd->EnableWindow(_bLock);
			pWnd = GetDlgItem(IDC_BTN_DHCP);
			if (pWnd) pWnd->EnableWindow(_bLock);
			// Not Support
			pWnd = GetDlgItem(IDC_BTN_REBOOT);
			if (pWnd) pWnd->EnableWindow(_bLock);			
			
			pWnd = GetDlgItem(IDC_BTN_AUTOIP);
			if (pWnd) pWnd->EnableWindow(_bLock);
			pWnd = GetDlgItem(IDC_BTN_MAC);
			if (pWnd) pWnd->EnableWindow(_bLock);
			break; }

		default: { break; }	
	}
	
	return TRUE;
}

BOOL CIPCamSearchDlg::UpdateCancel(int _iItemIdx)
{
	if (m_arrppUpdateItem[_iItemIdx]) {
		delete m_arrppUpdateItem[_iItemIdx];
		m_arrppUpdateItem[_iItemIdx] = NULL;
	}

	return TRUE;
}

BOOL CIPCamSearchDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
		pMsg->wParam = NULL;

	return CDialogEx::PreTranslateMessage(pMsg);
}

int CALLBACK MyCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	netMsg*          net_msg_ptr1 = (netMsg*)lParam1;
	netMsg*          net_msg_ptr2 = (netMsg*)lParam2;
	CIPCamSearchDlg* pParent      = (CIPCamSearchDlg*)lParamSort;

	int iResult = 0;
	int sort_column = pParent->GetSelectedColumnIndex();
	switch (sort_column)
	{
		case DEVICE_LIST_FW_VERSION:
			iResult = strcmp((char*)net_msg_ptr1->fw_ver, (char*)net_msg_ptr2->fw_ver);
			break;
		case DEVICE_LIST_MAC_ADDRESS:
			iResult = compareMacAddress(net_msg_ptr1->chaddr, net_msg_ptr2->chaddr);
			break;
		case DEVICE_LIST_IP_ADDRESS:
			iResult = compareIPAddress(net_msg_ptr1->ciaddr, net_msg_ptr2->ciaddr);
			break;
		case DEVICE_LIST_GATEWAY:
			iResult = compareIPAddress(net_msg_ptr1->giaddr, net_msg_ptr2->giaddr);
			break;
		case DEVICE_LIST_NETMASK:
			iResult = compareIPAddress(net_msg_ptr1->miaddr, net_msg_ptr2->miaddr);
			break;
		case DEVICE_LIST_DNS1:
			iResult = compareIPAddress(net_msg_ptr1->d1iaddr, net_msg_ptr2->d1iaddr);
			break;
		case DEVICE_LIST_DNS2:
			iResult = compareIPAddress(net_msg_ptr1->d2iaddr, net_msg_ptr2->d2iaddr);
			break;
		case DEVICE_LIST_HTTP_PORT:
			iResult = (int)(ntohl(net_msg_ptr1->http) - ntohl(net_msg_ptr2->http));
			break;
		case DEVICE_LIST_RTSP_PORT:
			iResult = (int)(ntohl(net_msg_ptr1->stream) - ntohl(net_msg_ptr2->stream));
			break;
		case DEVICE_LIST_RUN_TIME:
			iResult = compareRunningTime(net_msg_ptr1->runningtime, net_msg_ptr2->runningtime);
			break;
		default:
			return 0; // not sort
	}
	if (pParent->IsSortAscendingOrder())
	{
		iResult = iResult * (-1); // reverse plus && minus
	}
	if (iResult > 0)
		return -1;

	if (iResult < 0)
		return 1;

	return 0;
}

void CIPCamSearchDlg::OnLvnColumnclickListIpcam(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	if (pNMLV->iSubItem == DEVICE_LIST_UPDATE) // ignore column click
	{
		*pResult = 0;
		return;
	}
	// Select Column Number
	TRACE(_T("%d \n"), pNMLV->iSubItem);
	// SortType
	if (m_iOldColumn == pNMLV->iSubItem)
	{
		m_bSortAscending = !m_bSortAscending;

	}
	else {
		m_bSortAscending = TRUE;
	}

	UpdateColumnName(pNMLV->iSubItem, m_bSortAscending);
	// Save Old Column
	m_iOldColumn = pNMLV->iSubItem;

	// Sort Data
	m_cList.SortItems(MyCompareProc, (DWORD_PTR)this);

	*pResult = 0;
}

HBRUSH CIPCamSearchDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO:
	if (nCtlColor == CTLCOLOR_STATIC ||COLOR_BACKGROUND) {
		pDC->SetBkColor(BACK_COLOR);
		pDC->SetTextColor(TEXT_COLOR);
		hbr = (HBRUSH)m_cBrush.GetSafeHandle();
	}

	// Set Text Color
	if ( IDC_STT_VERSION == pWnd->GetDlgCtrlID() )
		pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);
	if ( IDC_STT_MODE == pWnd->GetDlgCtrlID() )
		pDC->SetTextColor(COLOR_CUSTOMIZE_RED);
		//pDC->SetTextColor(COLOR_CUSTOMIZE_ORANGE);

	return hbr;
}

void CIPCamSearchDlg::OnNMDblclkListIpcam(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	OperationList( LST_OP_WEB_OPEN );

	*pResult = 0;
}

void CIPCamSearchDlg::OnLvnDeleteitemListIpcam(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	netMsg* net_msg_ptr = (netMsg*)pNMLV->lParam;
	
	// Delete NetMsg Item
	if (net_msg_ptr) {
		delete net_msg_ptr;
		net_msg_ptr = NULL;
	}

	*pResult = 0;
}

void CIPCamSearchDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	// TODO:
	// Kill Timer
	FinishMacTimer();
	FinishDhcpTimer();
	FinishAutoIpTimer();
	FinishIpChangeTimer();
	FinishSearchTimer();
	FinishStopTimer();

	// Clear First Change Item
	if (m_pstIpChangeStartItem){
		delete m_pstIpChangeStartItem;
		m_pstIpChangeStartItem = NULL;
	}

	if (m_hWake){
		CloseHandle(m_hWake);
		m_hWake = NULL;
	}

	if (m_sockSend){
		closesocket(m_sockSend);
		m_sockSend = NULL;
	}

	// Destory Brush
	m_cBrush.DeleteObject();
}

void CIPCamSearchDlg::OnBnClickedBtnListCtl(UINT _uiID)
{
	ASSERT((_uiID >= IDC_BTN_SEARCH) && (_uiID <= IDC_BTN_MAC));
	switch (_uiID){
		case IDC_BTN_SEARCH:{
			if (m_bSearch)
			{
				// Search Prograss Kill
				FinishSearchTimer();

				GetDlgItem(IDC_BTN_SEARCH)->EnableWindow(FALSE);
				StartStopTimer();
			}
			else
			{
				/* Search Start */

				// Receive broadcast UDP socket Create and setup
				if ((m_sockRecv = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
					AfxMessageBox(L"Recv Socket Create Failed!"); return;
				}

				m_addrRecv.sin_family      = AF_INET;
				m_addrRecv.sin_port        = htons(LISTEN_PORT);
				m_addrRecv.sin_addr.s_addr = htonl(INADDR_ANY);

				if (INVALID_SOCKET == bind(m_sockRecv, (struct sockaddr*)&m_addrRecv, sizeof(SOCKADDR_IN))) {
					AfxMessageBox(L"Recv Socket Binding Failed!"); return;
				}

				m_bSearch = TRUE;
				DWORD dwThreadId = 0L;
				m_hThread =
					CreateThread(NULL, 0, recv_run_thread, (LPVOID)this, NULL, &dwThreadId);


				// Search Prograss & Serch MSG Start
				StartSearchTimer();

				// Btn and Status UI Change
				SetDlgItemText(IDC_BTN_SEARCH, L"STOP");
				m_cList.DeleteAllItems();

				// UI Button Enable
				SetUiBtnEnableWindow(FALSE, 1);

				// List Heaer Lock
				CHeaderCtrl* pCLstCtrlHeader = m_cList.GetHeaderCtrl();
				pCLstCtrlHeader->EnableWindow(FALSE);
			}
			break; }
		case IDC_BTN_IP_CHANGE:{
			// Get First Pos IP
			POSITION pos = m_cList.GetFirstSelectedItemPosition();
			if (NULL == pos) return;

			//=========
			BOOL bDhcpMode(FALSE);
			do{
				if (NULL == pos) break;

				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));

				if (pstItem->flag & (htonl(((0x1)<<FLAG_STATUS_DHCP)))){
					bDhcpMode = TRUE;
					break;
				}
			} while (pos);
			//=========
			if (bDhcpMode){
				AfxMessageBox(_T("Please Change DHCP Mode OFF"));
				return;
			}

			ShowIPChangeDlg();
			break; }
		case IDC_BTN_UPDATE:{
			// If don`t get update file path return.

			//struct ip_mreq	group;
			POSITION pos = m_cList.GetFirstSelectedItemPosition();
			int iListIndex = m_cList.GetNextSelectedItem(pos);
			GetDlgItemText(IDC_EDT_PATH, m_strFilePath);
			if (0 == m_strFilePath.Compare(_T(""))) {
				AfxMessageBox(_T(" Not Found Update File Path "));
				return;
			}

			// Check Same Device Element
			if (ChkLstDuplicationDevice()){
				AfxMessageBox(_T("Please Check Same IP or MAC"));
				return;
			}

			// Start Update
			UpdateStart();
			break; }
		case IDC_BTN_CLEAR :{ OperationList(LST_OP_ALL_CLEAR); break; }
		case IDC_BTN_DELETE:{ OperationList(LST_OP_DELETE)   ; break; }
		case IDC_BTN_WEB   :{ OperationList(LST_OP_WEB_OPEN) ; break; }
		case IDC_BTN_DHCP:{
			// Get First Pos IP
			POSITION pos = m_cList.GetFirstSelectedItemPosition();
			if (NULL == pos) return;

			//
			CSelectDlg dlg;
			dlg.SetInit(m_cList.GetSelectedCount(), 0);

			if (IDOK == dlg.DoModal()){
				SetUiBtnEnableWindow(FALSE, 1);
				// Get DHCP Status
				m_bDhcp = dlg.GetSetupStatus();

				StartDhcpTimer();
			}
			else {
				// Set Disable Buttons
				SetUiBtnEnableWindow(TRUE, 1);
			}
			break; }
		case IDC_BTN_REBOOT:{
			OperationList(LST_OP_REBOOT);
			OperationList(LST_OP_DELETE);
			OnBnClickedBtnListCtl(IDC_BTN_SEARCH);
			break; }
		case IDC_BTN_AUTOIP:{
			// Get First Pos IP
			POSITION pos = m_cList.GetFirstSelectedItemPosition();
			if (NULL == pos) return;

			CSelectDlg dlg;
			dlg.SetInit(m_cList.GetSelectedCount(), 1);

			if (IDOK == dlg.DoModal()){

				// Set Disable Buttons
				SetUiBtnEnableWindow(FALSE, 1);

				m_AutoIP = dlg.GetSetupStatus();

				StartAutoIpTimer();

			}
			else {
				// Set Disable Buttons
				SetUiBtnEnableWindow(TRUE, 1);
			}
			break; }
		case IDC_BTN_MAC:{
			POSITION pos = m_cList.GetFirstSelectedItemPosition();
			if (NULL == pos) return;

			// Get Selected Item Count
			m_uiItemCount = m_cList.GetSelectedCount();
			if (m_uiItemCount != 1)
			{
				AfxMessageBox(_T("MAC Change is possible just One Selected."));
				return;
			}
			netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));

			// Create MAC Change Dlg
			CMacChangeDlg dlg;
			dlg.SetInitInfo(m_uiItemCount, pstItem);

			if (IDOK == dlg.DoModal()){
				// Delete the registered MAC address.
				ShellExecute(NULL, _T("open"), _T("arp"), _T("-d"), NULL, SW_HIDE);

				// Set Disable Buttons
				SetUiBtnEnableWindow(FALSE, 1);

				// Make First Chagne
				m_pstIpChangeStartItem = new netMsg;
				memset(m_pstIpChangeStartItem, 0, sizeof(netMsg));

				dlg.GetSetupInfoResult(m_pstIpChangeStartItem);

				StartMacTimer();
				//OperationList(LST_OP_MAC_CHANGE);
				//OnBnClickedButSearch();
			}
			else {

				// Set Disable Buttons
				SetUiBtnEnableWindow(TRUE, 1);
			}

			break; }
		default:{ ASSERT(0); break; }
	}
}
void CIPCamSearchDlg::OnBnClickedBtnPath()
{
	TCHAR szFilter[] = _T("System Files(*.bin)|*.bin||");
	CFileDialog fileDlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, szFilter);

	if (fileDlg.DoModal() == IDOK)
		SetDlgItemText(IDC_EDT_PATH, fileDlg.GetPathName());
}

BOOL CIPCamSearchDlg::OperationList( int _iOption )
{
	if ( LST_OP_ALL_CLEAR == _iOption ){
		m_cList.DeleteAllItems();
		return TRUE;
	}
	// Get Selected Item Count
	m_uiItemCount = m_cList.GetSelectedCount();
	if ( 0 == m_uiItemCount )
		return FALSE;
	
	POSITION pos = NULL;
	pos = m_cList.GetFirstSelectedItemPosition();
	do{
		if (NULL == pos) break;

		switch (_iOption){
			case LST_OP_WEB_OPEN: {
				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));
				
				CString strAddr(_T(""));
				MakeIPAddressToString(pstItem->ciaddr, strAddr);
				CString strPort(_T(""));
				strPort.Format(L"%d", ntohl(pstItem->http));// Change Byte Order
				CString strWebAddress(_T(""));
				strWebAddress = _T("http://") + strAddr + _T(":") + strPort;

				ShellExecute(NULL, _T("open"), _T("iexplore"), strWebAddress, NULL, SW_SHOW);
				break; }

			case LST_OP_DELETE: {
				m_cList.DeleteItem(m_cList.GetNextSelectedItem(pos));
				pos = m_cList.GetFirstSelectedItemPosition();
				break; }

			case LST_OP_REBOOT: {
				CString strDebug = (_T(""));;
				strDebug.Format(_T("LST_OP_REBOOT \n"));
				ShowDebugMSG(strDebug);

				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));
				
				pstItem->opcode = MSG_IP_REBOOT;

				// Send Reboot MSG
#if (1)
				AllinterfaceSend(pstItem);
#else
				int iSendLen = sendto(m_sockSend, (char*)pstItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
				break; }

			case LST_OP_UPDATE : { 
				CString strDebug = (_T(""));;
				strDebug.Format(_T("LST_OP_UPDATE \n"));
				ShowDebugMSG(strDebug);

				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));

				pstItem->opcode = MSG_IP_UPDATE;

				// Send Update MSG
#if (1)
				AllinterfaceSend(pstItem);
#else
				int iSendLen = sendto(m_sockSend, (char*)pstItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
				break; }

			case LST_OP_AUTOIP: {
				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));
				// Set Auto IP Mode
				pstItem->opcode  = MSG_CAM_AUTOIP;
				pstItem->nettype = NET_STATIC;

				if (m_AutoIP){
					pstItem->flag |= htonl( ((0x1)<<FLAG_STATUS_AUTOIP) );
				} else {
					pstItem->flag &= htonl( ~((0x1)<<FLAG_STATUS_AUTOIP) );
				}
#if (1)
				AllinterfaceSend(pstItem);
#else
				int iSendLen = sendto(m_sockSend, (char*)pstItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
				break; }

			case LST_OP_DHCP : {
				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));
				// Set DHCP Mode
				pstItem->opcode  = MSG_IP_DHCP;
				pstItem->nettype = NET_STATIC;

				if(m_bDhcp)
					pstItem->flag |= htonl(((0x1)<<FLAG_STATUS_DHCP));
				else
					pstItem->flag &= htonl(~((0x1)<<FLAG_STATUS_DHCP));

#if (1)
				AllinterfaceSend(pstItem);
#else
				int iSendLen = sendto(m_sockSend, (char*)pstItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
				break; }

			case LST_OP_MAC_CHANGE: {
				netMsg* pstItem = (netMsg*)m_cList.GetItemData(m_cList.GetNextSelectedItem(pos));

				m_pstIpChangeStartItem->opcode  = MSG_IP_MACUPDATE;
				m_pstIpChangeStartItem->nettype = NET_STATIC;

#if (1)
				AllinterfaceSend(m_pstIpChangeStartItem);
#else
				int iSendLen = sendto(m_sockSend, (char*)m_pstIpChangeStartItem, sizeof(netMsg), 0, (struct sockaddr*)&m_addrSend, sizeof(SOCKADDR_IN));
#endif
				break; }

			default: { break; }
		}
	} while (pos);

	return TRUE;
}


/////
inline VOID CIPCamSearchDlg::ShowDebugMSG(LPCWSTR _DebugMSG)
{
	return;
	// Show Debug MSG
	OutputDebugString( _DebugMSG );
}
