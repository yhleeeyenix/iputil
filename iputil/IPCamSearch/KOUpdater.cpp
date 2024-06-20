#include "stdafx.h"
#include "KOUpdater.h"

IMPLEMENT_DYNAMIC(CKOUpdater, CWnd)

CKOUpdater::CKOUpdater()
:m_Parent_window(NULL)
,m_iItemIdx(0)
,m_iLstIdx(0)
,m_bMessage(FALSE)
,m_bNewVersion(FALSE)
,m_shPort(0)
,m_bUpdateFileOpen(FALSE)
,m_iContentItemLen(0)
,m_iContentItemHeaderLen(0)
,m_Socket(INVALID_SOCKET)
,m_ullUpdateStartPos(0) // First Header
,m_ullUpdateEndPos(0) // End of Update File.
,m_ThreadID(0)
,m_hUpdateThread(NULL)
,m_bUpdateStats(FALSE)
,m_pszSendBuffer(NULL)        // Send Buffer
,m_pszRecvBuffer(NULL)        // Recv Buffer
,m_pszContentItemHeader(NULL) // Contens Header Item
,m_pszContentItem(NULL)       // Contens Item
,m_iCRC_Error_Count(0)
,m_iTotalContentLen(0)        // Total Update Content Len
,m_iTotalSendContentLen(0)    // Total Send Update Content Len
{
    // Init IPAddr
	memset(m_szIP, 0, sizeof(m_szIP));// sizeof(char)*MAX_IPADDR_LEN);
    
    // Update File Path
	memset(m_szFilePath, 0, sizeof(m_szFilePath));// sizeof(char)*MAX_SEND_ONE_BLOCK_SIZE);

    // Init Update File Header
//    for (int i=0; i<MAX_HEADER_COUNT; i++)
//        memset( &m_stUpdateHeader[i], 0, sizeof(UPLOAD_HEADER) );
}

CKOUpdater::~CKOUpdater()
{
    // Clear Updater Class
    UpdateCancel();
     
    if(this->GetSafeHwnd())
        this->DestroyWindow();
}

BEGIN_MESSAGE_MAP(CKOUpdater, CWnd)
//ON_WM_TIMER()
END_MESSAGE_MAP()

inline unsigned long CRC32( unsigned long _ulCRC32, const void* _pbtBuffer, int _iBufferLen )
{
    unsigned long   ulCRC32   = 0;
    unsigned char*  pbtBuffer = NULL;

    /** accumulate crc32 for buffer **/
    ulCRC32 = _ulCRC32 ^ 0xFFFFFFFF;
    pbtBuffer = (unsigned char*)_pbtBuffer;
    for (int i=0; i < _iBufferLen; i++) {
        ulCRC32 = (ulCRC32 >> 8) ^ s_crcTable[(ulCRC32 ^ pbtBuffer[i]) & 0xFF];
    }
    return(ulCRC32 ^ 0xFFFFFFFF);
}

BOOL CKOUpdater::Create(CWnd* _parent_window)
{
    ASSERT(_parent_window  != NULL && _parent_window->GetSafeHwnd());
    m_Parent_window = _parent_window;

    RECT rc ={ 0 };
    LPCTSTR lpszClassName =
        AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW)); // CS_DBLCLKS
    return CreateEx(0, lpszClassName, L"CUpdate", WS_CHILD|WS_VISIBLE, rc, _parent_window, 0);
}
BOOL CKOUpdater::SetIndex( int _iItemIdx, int _iLstIdx, BOOL _bMessage )
{
    ASSERT( NULL != m_Parent_window || NULL != this->GetSafeHwnd() );
    if ( this->GetSafeHwnd() == NULL ) return FALSE;

    m_iItemIdx = _iItemIdx;
    m_iLstIdx  = _iLstIdx;
    m_bMessage = _bMessage;

    return TRUE;
}
BOOL CKOUpdater::GetIndex( int& _iItemIdx, int& _iLstIdx )
{
    ASSERT( NULL != m_Parent_window || NULL != this->GetSafeHwnd() );
    if ( this->GetSafeHwnd() == NULL ) return FALSE;

    _iItemIdx = m_iItemIdx;
    _iLstIdx  = m_iLstIdx;

    return TRUE;
}
BOOL CKOUpdater::SetUpdateInfo( LPCTSTR _lpszIP, short _shPort, LPCTSTR _lpszFilePath, BOOL _bNewVersion )
{
    ASSERT( NULL != m_Parent_window || NULL != this->GetSafeHwnd() );
    if ( this->GetSafeHwnd() == NULL ) return FALSE;

    // Set Device Version
    m_bNewVersion = _bNewVersion;

    // Setup Connect Info
	memset( m_szIP, 0, sizeof(m_szIP) );
    USES_CONVERSION;
	strcpy_s(m_szIP, sizeof(m_szIP), W2A(_lpszIP));
    m_shPort = (short)_shPort;

    // Setup Update File Path
	memset( m_szFilePath, 0, sizeof(m_szFilePath) );
	strcpy_s( m_szFilePath, sizeof(m_szFilePath), W2A(_lpszFilePath) );

    return TRUE;
}

BOOL CKOUpdater::UpdateStart ( void )
{
    if ( m_bUpdateStats ) return FALSE;

    // Update File Open
    if (m_bUpdateFileOpen) return FALSE;

	// FIX ME : check update file is valid Package file before upgrade
    m_bUpdateFileOpen = OpenUpdateFile(m_szFilePath);
    if ( FALSE == m_bUpdateFileOpen ) {
        CloseUpdateFile();
        NotifyMessage( UPDATE_FAIL, m_iItemIdx, m_iLstIdx, m_iVolume ); // Message Operation
        return FALSE;
    }
        
    // Get Update File Start and End Pos
    m_ullUpdateStartPos = m_ullUpdateEndPos = 0;
//    m_iUpdateFileCount  = 0;
    m_ullUpdateStartPos = m_UpdateFile.GetPosition();
    m_ullUpdateEndPos   = m_UpdateFile.GetLength();

/*
    // Make Update File Header.
    unsigned int uiRead     = 0; // Read Buffer
    ULONGLONG    ullNextPos = 0; // Current File Position    
    int          iPassIdx   = 0; // Bypass Index
    
    for (int i=0; i<MAX_HEADER_COUNT; i++){
        memset(&m_stUpdateHeader[i], 0, sizeof(UPLOAD_HEADER));
        uiRead     = m_UpdateFile.Read(&m_stUpdateHeader[i], sizeof(UPLOAD_HEADER));
        ullNextPos = m_UpdateFile.GetPosition();

        // Notify = UPLOAD_HEADER Size is 92 Byte.
        if (uiRead != sizeof(UPLOAD_HEADER)) {
            CloseUpdateFile();
            NotifyMessage( UPDATE_FAIL, m_iItemIdx, m_iLstIdx, m_iVolume ); // Message Operation
            return FALSE;
        }

        if (0 == m_stUpdateHeader[i].fileSize || 0 == m_stUpdateHeader[i].startAddr){
            iPassIdx++;
            continue;
        }
        
        // Get Total File Count
        m_iUpdateFileCount++;
    }
	//Debug String
	CString strDebug(_T(""));

    // Check Update File Count
    if ( m_iUpdateFileCount == 0 ){
        //TRACE( _T( "Update File Error \n" ) );
		strDebug = _T( "Update File Error \n" );
		ShowDebugMSG(strDebug);

        CloseUpdateFile();
        NotifyMessage( UPDATE_FAIL, m_iItemIdx, m_iLstIdx, m_iVolume ); // Message Operation
        return FALSE;
    }
*/
    // Get Total Content Size
//    m_iTotalContentLen  = 0;    
//    for ( int i = 0; i<MAX_HEADER_COUNT; i++ ){
//        // Get Total Contens Len
//        m_iTotalContentLen += GetContensLen( i, i-iPassIdx );
//    }
    
    // Make Update Thread
    if ( NULL != m_hUpdateThread ) {
        WaitForSingleObject( m_hUpdateThread, INFINITE );
        CloseHandle( m_hUpdateThread );
        m_hUpdateThread = NULL;
    }

    //// Update Start !!
    m_hUpdateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, this, 0, &m_ThreadID );    
    return TRUE;
}
int CKOUpdater::GetContensLen(int _iIndex, int _iTransIndex){
    ASSERT(_iIndex >= 0 && _iIndex < 3); // 0, 1, 2
    // Make Data Header
    char szHeaderData[MAX_FILE_LEN] = {0,}; // Max Size 256
    int iHeaderBuffLen = 0;
    iHeaderBuffLen += sprintf_s( szHeaderData               , sizeof(szHeaderData)               , START_TOKKEN ); // 42?
//    iHeaderBuffLen += sprintf_s( szHeaderData+iHeaderBuffLen, sizeof(szHeaderData)-iHeaderBuffLen, CONTENT_HEADER, m_stUpdateHeader[_iIndex].filename,
//                                                                                                                   m_iUpdateFileCount - 1            , // Index
//                                                                                                                   _iTransIndex                           ,
//                                                                                                                   _iTransIndex                           );
    // Make Data Tail
    char szTailData[MAX_FILE_LEN] ={0,};// Tail
    int iTailBuffLen  = 0;
    iTailBuffLen += sprintf_s(szTailData, sizeof(szTailData), END_TOKKEN );
    
    // Get Total Contents Length
    int iResultContentsLen = 0;
 //   iResultContentsLen = iHeaderBuffLen
 //                        + sizeof(UPLOAD_HEADER)
 //                        + m_stUpdateHeader[_iIndex].fileSize
//                         + iTailBuffLen;

    // Make Html Post Header
    char szHtmlPost[MAX_FILE_LEN] ={0,};    
    iResultContentsLen += sprintf_s(szHtmlPost, sizeof(szHtmlPost), HTTP_HEADER, m_szIP             ,
                                                                                 iResultContentsLen );

    return iResultContentsLen;
}
BOOL CKOUpdater::CheckUpdateStatus( int _iTotalContent, int _iTotalSendContent )
{
    ASSERT( 0 != _iTotalContent );
    
    // Calc File Send Result
    int iResultSend = (int)(((double)_iTotalSendContent / _iTotalContent) * (double)100);
    if ( m_iVolume >= iResultSend ) return FALSE;

    m_iVolume = iResultSend;

    // Post Message Update Status
    NotifyMessage( UPDATE_START, m_iItemIdx, m_iLstIdx, m_iVolume );
    
    return TRUE;
}

BOOL CKOUpdater::UpdateCancel( void )
{
    // Close Conncetion
    Disconnect();

    // Make Update Thread
    if ( NULL != m_hUpdateThread )
    {
        WaitForSingleObject(m_hUpdateThread, INFINITE);
        CloseHandle(m_hUpdateThread);
        m_hUpdateThread = NULL;
    }

    // Delete Send Buffer
    if (m_pszSendBuffer) {
        delete[] m_pszSendBuffer;
        m_pszSendBuffer = NULL;
    }
    // Delete Recv Buffer
    if (m_pszRecvBuffer){
        delete[] m_pszRecvBuffer;
        m_pszRecvBuffer = NULL;
    }

    // Delete Update Item
        if (m_pszContentItemHeader) {
        delete[] m_pszContentItemHeader;
        m_pszContentItemHeader = NULL;
    }
    
    if (m_pszContentItem) {
        delete[] m_pszContentItem;
        m_pszContentItem = NULL;
    }
    
    // Close File;
    CloseUpdateFile();

    return TRUE;
}
DWORD WINAPI CKOUpdater::UpdateThread(void* pData)
{
    ASSERT( pData != NULL );
    if ( NULL == pData ) return 0;

    CKOUpdater* pCKoUpdater = (CKOUpdater*)pData;        
    pCKoUpdater->FileUpdateThread();
    
    return 0L;
}
void CKOUpdater::FileUpdateThread(void)
{
	//==============
	USES_CONVERSION;
	CString strDebug(_T(""));
	//==============

    // Init Update Status Element
    m_iVolume              = 0;
    m_iTotalSendContentLen = 0;
    m_bUpdateStats         = true;
    
    // Post Message Update Start
    NotifyMessage( UPDATE_START, m_iItemIdx, m_iLstIdx, m_iVolume );
    
    // Thread Function !!
    int iPassIdx = 0;
	UINT uiRead = 0;
    //for (int i=0; i<MAX_HEADER_COUNT; i++)
    //{
        // Pass Empty Data
        //if (0 == m_stUpdateHeader[i].fileSize || 0 == m_stUpdateHeader[i].startAddr) {
        //    iPassIdx++;
        //    continue;
        //}

        // Make Update File
        //MakeFileData(m_stUpdateHeader[i].startAddr, i , i - iPassIdx );
		   
	
		//char* tailstr = END_TOKKEN;
		//int tailsz = strlen(tailstr);

		m_iContentItemLen = m_UpdateFile.GetLength();
		m_iTotalContentLen = m_iContentItemLen;
		/////////////////////////////////////////////////////////////////
		// Make Content Header Data

		char szHtmlPost[MAX_FILE_LEN] ={0,};
		m_iContentItemHeaderLen = 0;
		m_iContentItemHeaderLen += sprintf_s(szHtmlPost, sizeof(szHtmlPost), HTTP_HEADER, m_szIP            ,m_szIP           ,
																						  m_iContentItemLen  + END_TOKKEN_LEN ,m_UpdateFile.GetFileName());
		
		if ( NULL != m_pszContentItemHeader ) {
			delete [] m_pszContentItemHeader;
			m_pszContentItemHeader = NULL;
		}

		int iRealSize = m_iContentItemHeaderLen;
		if (m_iContentItemHeaderLen < MAX_SEND_ONE_BLOCK_SIZE)
			m_iContentItemHeaderLen = MAX_SEND_ONE_BLOCK_SIZE + SEND_BLOCK_DUMMY_SIZE;
		else
			ASSERT(0);

		m_pszContentItemHeader = new char[m_iContentItemHeaderLen];
		memset(m_pszContentItemHeader, 0, sizeof(char)*m_iContentItemHeaderLen );
		memcpy( m_pszContentItemHeader, szHtmlPost, iRealSize );

		uiRead = m_UpdateFile.Read(m_pszContentItemHeader + iRealSize , sizeof(char) * (m_iContentItemHeaderLen - iRealSize) ); // Update File

		m_iContentItemLen = m_iContentItemLen - (m_iContentItemHeaderLen - iRealSize);

		m_pszContentItem = NULL;
		m_pszContentItem = new char[m_iContentItemLen+ END_TOKKEN_LEN];

		memset(m_pszContentItem, 0, sizeof(char)*(m_iContentItemLen+END_TOKKEN_LEN));
		m_UpdateFile.Seek(sizeof(char)*(m_iContentItemHeaderLen - iRealSize), CFile::begin);
		uiRead = m_UpdateFile.Read(m_pszContentItem, sizeof(char) * m_iContentItemLen); // Update File

		memcpy(m_pszContentItem+m_iContentItemLen, END_TOKKEN, END_TOKKEN_LEN);
		m_iContentItemLen += END_TOKKEN_LEN;

		//for(int tl = 0; tl<m_iContentItemLen; tl++)
		//	m_pszContentItem[tl];

        // Send Update Data
        if (FALSE == SendUpdateFileData())
        {
            // File Send Fail ...
            //TRACE( _T("%s File Index :%d FAIL !! \n"), A2W(m_szIP), i );
			//strDebug.Format( _T("[%s] File Index :%d FAIL !! \n"), A2W(m_szIP), i );
			//ShowDebugMSG(strDebug);

            if (m_pszContentItemHeader) {
                delete[] m_pszContentItemHeader;
                m_pszContentItemHeader = NULL;
            }

            if (m_pszContentItem) {
                delete[] m_pszContentItem;
                m_pszContentItem = NULL;
            }

            // Post Message Update Fail
            NotifyMessage(UPDATE_FAIL, m_iItemIdx, m_iLstIdx, m_iVolume);
            m_bUpdateStats = false;
            return;
        }
        //TRACE(_T("%s, File Index :%d Success !! \n"),A2W(m_szIP), i);
		//strDebug.Format( _T("[%s], File Index :%d Success !! \n"),A2W(m_szIP), i);
		//ShowDebugMSG(strDebug);
    //}

#if 1
		SOCKET m_sockupdate;
		SOCKADDR_IN m_addupdate, m_addupdateto;
		char buf[10];
		int nLen;
		ZeroMemory(&buf, 10);
		if ((m_sockupdate = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
			AfxMessageBox(L"Update Socket Create Failed!"); return;
		}

		memset(&m_addupdate, 0, sizeof(m_addupdate));
		m_addupdate.sin_family = AF_INET;
		m_addupdate.sin_port = htons(UPDATE_PORT);
		m_addupdate.sin_addr.s_addr = htonl(INADDR_ANY);
		//m_addupdate.sin_addr.s_addr = inet_addr(m_szIP);
		//m_addupdate.sin_addr.s_addr = inet_addr(LPSTR(LPCTSTR(m_cList.GetItemText(iListIndex, DEVICE_LIST_IP_ADDRESS))));
#if 0
		if (SOCKET_ERROR == connect(m_sockupdate, (SOCKADDR*)&m_addupdate, sizeof(m_addupdate))){
			AfxMessageBox(L"Update Socket connect Failed!");
			closesocket(m_sockupdate);
		}
#endif
		if (SOCKET_ERROR == bind(m_sockupdate, (struct sockaddr*)&m_addupdate, sizeof(m_addupdate))) {
			AfxMessageBox(L"Update Socket Binding Failed!");
		}

#if 1
		int clntAdrSz = sizeof(m_addupdateto);
		NotifyMessage(UPDATE_WRITE, m_iItemIdx, m_iLstIdx, m_iVolume);
		while(1){
			nLen = recvfrom(m_sockupdate, buf, 10, 0, (SOCKADDR*)&m_addupdateto, &clntAdrSz);
			strDebug.Format(_T("Update Write [%d] \n"), buf[0]);
			ShowDebugMSG(strDebug);
			if (buf[0] == 100)
				break;
		}
#endif
		closesocket(m_sockupdate);
#endif	//ksh
    // Post Message Update Finish
    NotifyMessage( UPDATE_FINISH, m_iItemIdx, m_iLstIdx, 100 );

    // Update Finish
    m_bUpdateStats = false;
}
BOOL CKOUpdater::OpenUpdateFile(char* pszFilePath )
{
    if (m_bUpdateFileOpen)
        return FALSE;
    
    USES_CONVERSION;
    return m_UpdateFile.Open( A2W(pszFilePath), CFile::modeRead | CFile::shareDenyNone, NULL );
}
BOOL CKOUpdater::CloseUpdateFile(void)
{
    // Delete Open Update File
    if ( m_bUpdateFileOpen ) {
        m_UpdateFile.Close();
        m_bUpdateFileOpen = FALSE;
    }

    return TRUE;
}
BOOL CKOUpdater::MakeFileData( ULONGLONG _ullDataPos, int _iIndex, int _iTransIndex )
{
    /*****************************************************************
    Notify : Data => System Data, BackupData, ActiveX Data
    System Data(Always Fix), Backup Data(UnFix),ActiveX Data(UnFix).
    ******************************************************************/
 /*   
	ASSERT(_iIndex >= 0 && _iIndex < 3); // 0, 1, 2
    // Make Data Header
    char szHeaderData[MAX_FILE_LEN] = {0,}; // Max Size 256
    int iHeaderBuffLen = 0;
    iHeaderBuffLen += sprintf_s( szHeaderData               , sizeof(szHeaderData)               , START_TOKKEN ); // 42?
    iHeaderBuffLen += sprintf_s( szHeaderData+iHeaderBuffLen, sizeof(szHeaderData)-iHeaderBuffLen, CONTENT_HEADER, m_stUpdateHeader[_iIndex].filename,
                                                                                                                   m_iUpdateFileCount - 1            , // Index
                                                                                                                   _iTransIndex                           ,
                                                                                                                   _iTransIndex                           );
    // Make Data Tail
    char szTailData[MAX_FILE_LEN] ={0,};// Tail
    int iTailBuffLen  = 0;
    iTailBuffLen += sprintf_s(szTailData, sizeof(szTailData), END_TOKKEN );
    
    // Get Total Contet Length
    m_iContentItemLen = 0;
    m_iContentItemLen = iHeaderBuffLen
                        + sizeof(UPLOAD_HEADER)
                        + m_stUpdateHeader[_iIndex].fileSize
                        + iTailBuffLen;
    
    
    // Make Total Data Item
    if( NULL != m_pszContentItem ) {
        delete [] m_pszContentItem;
        m_pszContentItem = NULL;
    }

    m_pszContentItem = new char[m_iContentItemLen];
    memset(m_pszContentItem, 0, sizeof(char)*m_iContentItemLen );          // Content Header
       
    // Copy Header Data
    char* pNextPositon = NULL;
    memcpy( m_pszContentItem, szHeaderData, iHeaderBuffLen );
    pNextPositon = m_pszContentItem + iHeaderBuffLen;

    m_stUpdateHeader[_iIndex].startAddr = sizeof(UPLOAD_HEADER);
    m_stUpdateHeader[_iIndex].fileType  = ( 0x11 + _iIndex );
    memcpy( pNextPositon, &m_stUpdateHeader[_iIndex], sizeof(UPLOAD_HEADER) ); //  Update Structure

    pNextPositon += sizeof(UPLOAD_HEADER);

    // Copy and Read File Data
    if (m_UpdateFile.GetPosition() != _ullDataPos) {
        ULONGLONG ullFilePos = MoveSetFilePos(_ullDataPos);
        ASSERT( ullFilePos == m_UpdateFile.GetPosition() );
    }

    unsigned int uiRead     = 0; // Read Buffer
    ULONGLONG    ullNextPos = 0; // Current File Position

    uiRead = m_UpdateFile.Read(pNextPositon, m_stUpdateHeader[_iIndex].fileSize ); // Update File

    ASSERT( uiRead == m_stUpdateHeader[_iIndex].fileSize );

    pNextPositon += uiRead;

    // Copy Tail Data
    memcpy( pNextPositon, szTailData, iTailBuffLen ); // Tail Data
  

    // Result Data Checksum.......
    pNextPositon += iTailBuffLen;
    ASSERT( m_iContentItemLen == pNextPositon - m_pszContentItem );
    if ( m_iContentItemLen != pNextPositon - m_pszContentItem ) return FALSE;

    // Init NextPosition.
    pNextPositon = NULL;
 */   
    /////////////////////////////////////////////////////////////////
    // Make Content Header Data
    char szHtmlPost[MAX_FILE_LEN] ={0,};
    m_iContentItemHeaderLen = 0;
    m_iContentItemHeaderLen += sprintf_s(szHtmlPost, sizeof(szHtmlPost), HTTP_HEADER, m_szIP            ,m_szIP            , 
                                                                                      m_iContentItemLen );

    if ( NULL != m_pszContentItemHeader ) {
        delete [] m_pszContentItemHeader;
        m_pszContentItemHeader = NULL;
    }

	int iRealSize = m_iContentItemHeaderLen;
	if (m_iContentItemHeaderLen < MAX_SEND_ONE_BLOCK_SIZE)
		m_iContentItemHeaderLen = MAX_SEND_ONE_BLOCK_SIZE + SEND_BLOCK_DUMMY_SIZE;
	else
		ASSERT(0);

    m_pszContentItemHeader = new char[m_iContentItemHeaderLen];
    memset(m_pszContentItemHeader, 0, sizeof(char)*m_iContentItemHeaderLen );
    memcpy( m_pszContentItemHeader, szHtmlPost, iRealSize );

    return TRUE;
}
ULONGLONG CKOUpdater::MoveSetFilePos(ULONGLONG _ullSetPos)
{
    ULONGLONG ullActualPos = 0;
    ASSERT(FALSE != m_bUpdateFileOpen);

    ullActualPos = m_UpdateFile.Seek((LONGLONG)_ullSetPos, CFile::begin);

    // Return Set Current Pos
    return ullActualPos;
}
/////
inline void CKOUpdater::ShowDebugMSG(LPCWSTR _DebugMSG)
{
	//return;
	// Show Debug MSG
	OutputDebugString( _DebugMSG );
}
BOOL CKOUpdater::SendUpdateFileData()
{
	//=====================
	USES_CONVERSION;
	CString strDebug(_T(""));
	//=====================

	//ASSERT(NULL != m_pszContentItemHeader || NULL != m_pszContentItem ||
	//	0 != m_iContentItemLen || 0 != m_iContentItemHeaderLen);

	// Conncet Socket
	if (FALSE == Connect())
		return FALSE;

	// Send Html Header Post.
	strDebug.Format( _T("[%s] Html Header Post : File Size=%d \n"), A2W(m_szIP), m_iContentItemHeaderLen );
	ShowDebugMSG(strDebug);
	
	if (m_iContentItemHeaderLen !=
		SendData(m_Socket, (BYTE*)m_pszContentItemHeader, m_iContentItemHeaderLen)){
		//TRACE(_T("[%s] HTML POST SEND ERROR \n"), A2W(m_szIP) );
		strDebug.Format( _T("[%s] HTML POST SEND ERROR(%s) \n"), A2W(m_szIP) );
		ShowDebugMSG(strDebug);

		return FALSE;
	}

    // Update Status
    m_iTotalSendContentLen += m_iContentItemHeaderLen;
    CheckUpdateStatus( m_iTotalContentLen, m_iTotalSendContentLen );
    
    // Escape Error Count Check
    m_iCRC_Error_Count = 0;
    
	/*************************/
	// Send split Content Data
	/*************************/
	unsigned long ulCRC32   = 0;
	unsigned long ulIndex   = 0;
	int           iWriteLen = MAX_SEND_ONE_BLOCK_SIZE;
	int           iNextPos  = 0;
	int           iSendData = 0;
			
	//TRACE(_T("%s Content Data : File Index =%d, ContentSize = %d \n"), A2W(m_szIP), _iIndex, m_iContentItemLen );
	strDebug.Format(_T("[%s] Content Data : File  ContentSize = %d \n"), A2W(m_szIP), m_iContentItemLen );
	ShowDebugMSG(strDebug);
	do{
		iWriteLen = (MAX_SEND_ONE_BLOCK_SIZE < (m_iContentItemLen - iNextPos)) ?
		MAX_SEND_ONE_BLOCK_SIZE : (m_iContentItemLen - iNextPos);

		/***********************************************/
		/* [ CRC 4byte | Index 4Byte | Data 1452Byte ] */
		/***********************************************/
		// Make & Init Send Buffer
		if (m_pszSendBuffer) {
			delete[] m_pszSendBuffer;
			m_pszSendBuffer = NULL;
		}
		m_pszSendBuffer = new char[sizeof(char)*(MAX_SEND_ONE_BLOCK_SIZE + 4 + 4)];
		memset(m_pszSendBuffer, 0, sizeof(char)*(MAX_SEND_ONE_BLOCK_SIZE + 4 + 4));

		// Add Item Data
		memcpy(m_pszSendBuffer, m_pszContentItem + iNextPos, iWriteLen);
		// Add CRC Code
//		ulCRC32 = CRC32(0, m_pszContentItem + iNextPos, iWriteLen);
//		memcpy(m_pszSendBuffer, &ulCRC32, sizeof(unsigned long));
		// Add Index
//		memcpy(m_pszSendBuffer + 4, &ulIndex, sizeof(unsigned long));

		// Send Update Data
//		iSendData = 4 + 4 + iWriteLen;
		iSendData = iWriteLen;

		if (iSendData !=
			SendData(m_Socket, (BYTE*)m_pszSendBuffer, iSendData)){
			//TRACE(_T("%s while Data Send Error \n"), A2W(m_szIP));
			strDebug.Format(_T("[%s] while Data Send Error \n"), A2W(m_szIP));
			ShowDebugMSG(strDebug);
			
			if (m_pszSendBuffer) {
				delete[] m_pszSendBuffer;
				m_pszSendBuffer = NULL;
			}
			return FALSE;
		}
                
        // Update Status
//      m_iTotalSendContentLen += iSendData-4-4;
		m_iTotalSendContentLen += iSendData;

        CheckUpdateStatus(m_iTotalContentLen, m_iTotalSendContentLen );

		//TRACE(_T("%s Send Data : Index = %d, CRC=%d, Len=%d Pos=%d \n"), A2W(m_szIP), ulIndex, ulCRC32, iWriteLen, iNextPos);
		strDebug.Format( _T("[%s] Send Data : Index = %d, CRC=%d, Len=%d Pos=%d \n"), A2W(m_szIP), ulIndex, ulCRC32, iWriteLen, iNextPos );
		ShowDebugMSG(strDebug);
		
//		iNextPos += (iSendData - 4 - 4);
		iNextPos += (iSendData);
		ulIndex++;

		/********************************************************/
		/* Last Send Data is not give Recv Data and Disconnect. */
		/********************************************************/
		if (/*(2 == _iIndex) && */(m_iContentItemLen == iNextPos)) {
			//TRACE(_T("%s Last File Data FINISH!!! ::::  %d, %d\n"), A2W(m_szIP), _iIndex, m_iContentItemLen - iNextPos);
			//strDebug.Format( _T("[%s] Last File Data FINISH!!! ::::  %d, %d\n"), A2W(m_szIP), _iIndex, m_iContentItemLen - iNextPos);
			//ShowDebugMSG(strDebug);
			break;
		}

		// Make & Init Recv Buffer
		//if (m_pszRecvBuffer){
		//	delete[] m_pszRecvBuffer;
		//	m_pszRecvBuffer = NULL;
		//}
		//m_pszRecvBuffer = new char[sizeof(char)*(MAX_SEND_ONE_BLOCK_SIZE)];
		//memset(m_pszRecvBuffer, 0, sizeof(char)*(MAX_SEND_ONE_BLOCK_SIZE));

		// Recv Update Result
		/*if (FALSE == RecvData(m_Socket, (BYTE*)m_pszSendBuffer, MAX_SEND_ONE_BLOCK_SIZE, ulIndex, iNextPos)) {
			//TRACE(_T("%s while Data Recv Error \n"), A2W(m_szIP) );
			strDebug.Format( _T("[%s] while Data Recv Error \n"), A2W(m_szIP) );
			ShowDebugMSG(strDebug);

			if (m_pszSendBuffer) {
				delete[] m_pszSendBuffer;
				m_pszSendBuffer = NULL;
			}
			if (m_pszRecvBuffer){
				delete[] m_pszRecvBuffer;
				m_pszRecvBuffer = NULL;
			}
			return FALSE;
		}*/
	} while (m_iContentItemLen > iNextPos);
#if 1
	do{
				// Make & Init Recv Buffer
		if (m_pszRecvBuffer){
			delete[] m_pszRecvBuffer;
			m_pszRecvBuffer = NULL;
		}
		m_pszRecvBuffer = new char[sizeof(char)*(MAX_SEND_ONE_BLOCK_SIZE)];
		memset(m_pszRecvBuffer, 0, sizeof(char)*(MAX_SEND_ONE_BLOCK_SIZE));

		// Recv Update Result
		if (FALSE == RecvData(m_Socket, (BYTE*)m_pszSendBuffer, MAX_SEND_ONE_BLOCK_SIZE, ulIndex, iNextPos)) {
			//TRACE(_T("%s while Data Recv Error \n"), A2W(m_szIP) );
			strDebug.Format( _T("[%s] while Data Recv Error \n"), A2W(m_szIP) );
			ShowDebugMSG(strDebug);

			if (m_pszSendBuffer) {
				delete[] m_pszSendBuffer;
				m_pszSendBuffer = NULL;
			}
			if (m_pszRecvBuffer){
				delete[] m_pszRecvBuffer;
				m_pszRecvBuffer = NULL;
			}
			return TRUE;
		}

	}while(1);
#endif
	Disconnect();

	if (m_pszSendBuffer) {
		delete[] m_pszSendBuffer;
		m_pszSendBuffer = NULL;
	}
	if (m_pszRecvBuffer){
		delete[] m_pszRecvBuffer;
		m_pszRecvBuffer = NULL;
	}

	return TRUE;
}
 BOOL CKOUpdater::Connect(void)
 {
     Disconnect();

	 // Debug STring
	 CString strDebug(_T(""));
	 // Init Socket
     m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if (INVALID_SOCKET == m_Socket){
         Disconnect();
         return FALSE;
     }

     // Set Recv Time Out
     int iOptionVal = 6000; // msec
     int iOptionLen = sizeof(DWORD);
     int iResult = getsockopt( m_Socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&iOptionVal, &iOptionLen ); //setsocoption 일건데? 잘못했네 2016.05 오늘알았뜸..
	 
	 //getaddrinfo()
	 // DNS Resolve Apply
	 struct hostent * addrResolved = gethostbyname(m_szIP);
	 if (addrResolved == NULL) {
		 //TRACE("CKOUpdater::Connect DNS address resolve error\n");
		 strDebug = _T("CKOUpdater::Connect DNS address resolve error\n");
		 ShowDebugMSG(strDebug);

		 return FALSE;
	 }
     
     sockaddr_in stServer;
     memset(&stServer, 0, sizeof(stServer));
     stServer.sin_family      = AF_INET;
     stServer.sin_port        = htons((unsigned short)m_shPort);
     stServer.sin_addr.s_addr = inet_addr(m_szIP);

     if (SOCKET_ERROR == connect(m_Socket, (SOCKADDR*)&stServer, sizeof(stServer))){
         Disconnect();
         return FALSE;
     }

     return TRUE;
 }
 BOOL CKOUpdater::Disconnect(void)
 {
     // Close Socket
     if (INVALID_SOCKET != m_Socket)
         closesocket(m_Socket);
     m_Socket = INVALID_SOCKET;

	 // FIX ME : uninitilzie window sock on exitinstance
     // WinSock Clean UP
     //WSACleanup();

     return TRUE;
 }

//#define __BEFORE__
 int CKOUpdater::SendData(SOCKET _sock, BYTE* _pbtSendPacket, int _iBytesToSend)
 {
	 int iSendBytes   = 0,
		 iRemainBytes = 0,
		 iSend        = 0;

	 iRemainBytes = _iBytesToSend;

	 // Debug String
	 CString strDebug(_T(""));

	 while (iRemainBytes > 0)
	 {
		 iSend = send(_sock, (char*)(_pbtSendPacket + iSendBytes), iRemainBytes, 0);
#ifdef __BEFORE__
		 if (iSend <= 0) {
			 int nError = ::WSAGetLastError();
			 if (nError == WSAEINPROGRESS) {
				 Sleep(1); continue;
			 }
			 break;
		 }
#else
		 if (iSend < 0) {
			 int nError = ::WSAGetLastError();
			 if (nError == WSAEINPROGRESS) {
				 //TRACE( _T("Send Data -1 WSAEINPROGRESS Continue !!! \n") );
				 strDebug = _T("Send Data -1 WSAEINPROGRESS Continue !!! \n") ;
				 ShowDebugMSG(strDebug);
				 Sleep(1); continue;
			 }
			 //TRACE( _T("Send Data -1 break !!! \n") );
			 strDebug = _T("Send Data -1 break !!! \n");
			 ShowDebugMSG(strDebug);
			 break;
		 }
		 if (iSend == 0){
			 //TRACE( _T("Send Data 0 Continue !!! \n") );
			 strDebug = _T("Send Data 0 Continue !!! \n") ;
			 ShowDebugMSG(strDebug);
			 Sleep(1); continue;
		 }
#endif
		 iSendBytes    += iSend;
		 iRemainBytes  -= iSendBytes;
	 }

	 return iSendBytes;
 }

 BOOL CKOUpdater::RecvData( SOCKET _sock, BYTE* _pbtRecvPacket, int _iBytesToRecv, unsigned long& _ulIndex, int& _iNextPos )
 {     
     int iRecvBytes   = 0;
     int iRemainBytes = 0;
     int iRecv        = 0;
     iRemainBytes     = _iBytesToRecv;

	 // Debug STring
	 CString strDebug(_T(""));

     while ( iRemainBytes > 0)
     {
         iRecv = recv(_sock, (char*)_pbtRecvPacket, MAX_FILE_LEN, 0);
        
		 if (iRecv > 0){
			 strDebug.Format(_T("Recv Data %d !!! \n"), iRecv);
			 ShowDebugMSG(strDebug);
			 return TRUE;
		 }

#ifdef __BEFORE__
		 if (iRecv <= 0){
			 int iError = ::WSAGetLastError();
			 if (iError == WSAEINPROGRESS){
				 Sleep(1); continue;
			 }
			 TRACE(_T("Recv Data Error = %d\n"), iError);
			 TRACE(_T("Recv Data = %d\n"), iRecv);
			 return FALSE;
		 }
#else
		 if(iRecv < 0){
			 int iError = ::WSAGetLastError();
			 if (iError == WSAEINPROGRESS){
				 //TRACE( _T("Recv Data WSAEINPROGRESS Continue \n") );
				 strDebug = _T("Recv Data WSAEINPROGRESS Continue \n");
				 ShowDebugMSG(strDebug);
				 Sleep(1); continue;
			 }
			 //TRACE( _T("Recv Data Error = %d\n"), iError );
			 strDebug.Format(_T("Recv Data Error = %d\n"), iError);
			 ShowDebugMSG(strDebug);

			 return FALSE;
		 }
		 if (iRecv == 0){
			 //TRACE( _T("Recv Data 0 Continue !!! \n") );
			 strDebug = ( _T("Recv Data 0 Continue !!! \n") );
			 ShowDebugMSG(strDebug);
			 
			 Disconnect();
			 Sleep(1000); continue;
		 }
#endif
         iRecvBytes   += iRecv;
         iRemainBytes -= iRecvBytes;         
         /************************************************
        * Return Success Value !!
        * After 14-04-28-1
        * HTTP/1.0 200 0K/r/nTOTAL_LENGTH:2166384/r/n/r/n
        /***********************************************/
         if( m_bNewVersion ){
             if ( 0 != strncmp( (char*)_pbtRecvPacket+( iRecvBytes-4), "\r\n\r\n", 4 ) )
             continue;         
         }

         // Escape CRC Error OverFlow
         if ( 10 == m_iCRC_Error_Count ){
             //TRACE( _T("CRC Error Count Over = %d \n"), m_iCRC_Error_Count );
			 strDebug.Format(_T("CRC Error Count Over = %d \n"), m_iCRC_Error_Count);
			 ShowDebugMSG(strDebug);
            return FALSE;
         }
         

         /************************************************
        * Return CRC Error Value !!
        * HTTP/1.0 400 FAIL\r\nERROR:CRC:%d\r\n\r\n
        ***********************************************/
         if ( 0 == strncmp( (char*)_pbtRecvPacket+9, "400", 3 ) ){
             // CRC Error Count Check
             m_iCRC_Error_Count++;

             char* pchTok = NULL;
             pchTok       = strstr( (char*)_pbtRecvPacket+9+3, "ERROR:" );
             if (pchTok)
             {
                 //TRACE( _T( "ERROR:%s\n"), pchTok );
				 strDebug.Format(_T( "ERROR:%s\n"), pchTok );
				 ShowDebugMSG(strDebug);

                 if ( 0 == strncmp( pchTok+6, "CRC", 3 ) )
                 {
                     int toSeq = atol( pchTok+6+4 );
                     //TRACE( _T( "TOCRC : %d, curSeq %d, totSend %x, \n" ), toSeq, _ulIndex, _iNextPos );
					 strDebug.Format(_T( "TOCRC : %d, curSeq %d, totSend %x, \n" ), toSeq, _ulIndex, _iNextPos );
					 ShowDebugMSG(strDebug);

                     _iNextPos -= (_ulIndex - toSeq-1)*MAX_SEND_ONE_BLOCK_SIZE;
                     _ulIndex  = toSeq+1;
                     //TRACE( _T( "CHANGE : %d,curSeq %d, totSend %x, \n" ), _ulIndex, _ulIndex, _iNextPos );
					 strDebug.Format(_T( "CHANGE : %d,curSeq %d, totSend %x, \n" ), _ulIndex, _ulIndex, _iNextPos );
					 ShowDebugMSG(strDebug);
                 }
                 else if (strncmp( pchTok+6, "SEQ", 3 ) == 0)
                 {
                     int toSeq = atol( pchTok+6+4 );
                     //TRACE( _T( "TOSEQ : %d,curSeq %d, totSend %x, \n" ), toSeq, _ulIndex, _iNextPos );
					 strDebug.Format(_T( "TOSEQ : %d,curSeq %d, totSend %x, \n" ), toSeq, _ulIndex, _iNextPos );
					 ShowDebugMSG(strDebug);

                     _iNextPos -= (_ulIndex - toSeq-1)*MAX_SEND_ONE_BLOCK_SIZE;
                     _ulIndex = toSeq+1;
                     //TRACE( _T( "CHANGE : %d,curSeq %d, totSend %x, \n" ), _ulIndex, _ulIndex, _iNextPos );
					 strDebug.Format(_T( "CHANGE : %d,curSeq %d, totSend %x, \n" ), _ulIndex, _ulIndex, _iNextPos );
					 ShowDebugMSG(strDebug);
                 }
             }
         }
         // Escape Recv
         iRemainBytes = -1;
     }
     return TRUE;
 }
void CKOUpdater::NotifyMessage(unsigned int _uiMessageOption, int _iItemIdx, int _iLstIdx, int _iVolume )
{
    if ( FALSE == m_bMessage )
        return;
    
    if ( NULL == this                           ||
         NULL == this->GetSafeHwnd()            ||
         NULL == m_Parent_window                ||
         NULL == m_Parent_window->GetSafeHwnd() ) return;
    
    UPDATE_INFO_ITEM* pUpdateItem = new UPDATE_INFO_ITEM;
    pUpdateItem->iItemIdx         = _iItemIdx;
    pUpdateItem->iLstIdx          = _iLstIdx;
    pUpdateItem->iVolume          = _iVolume;
    switch (_uiMessageOption)
    {
        case UPDATE_START   : { pUpdateItem->iUpdateStatus = UPDATE_START ; break; }
		case UPDATE_WRITE	: { pUpdateItem->iUpdateStatus = UPDATE_WRITE ;	break; }
        case UPDATE_FINISH  : { pUpdateItem->iUpdateStatus = UPDATE_FINISH; break; }
        case UPDATE_FAIL    : { pUpdateItem->iUpdateStatus = UPDATE_FAIL  ; break; }
        default : { break; }
    }    
    
    ::PostMessage( m_Parent_window->GetSafeHwnd(), WM_UPDATE_DATA, (WPARAM)this, (LPARAM)pUpdateItem);
}

