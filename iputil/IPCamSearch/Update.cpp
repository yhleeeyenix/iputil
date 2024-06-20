#include "stdafx.h"
#include "Update.h"

IMPLEMENT_DYNAMIC(CUpdate, CWnd)

BEGIN_MESSAGE_MAP(CUpdate, CWnd)
    ON_WM_TIMER()
END_MESSAGE_MAP()

CUpdate::CUpdate()
:m_Parent_window(NULL)
,m_hUpdateThread(NULL)
,m_ThreadID(0)
,m_shPort(0)
,m_iItemIdx(0)
,m_iLstIdx(0)
,m_uiUpdateTimeID(0)
,m_iVolume(0)
,m_bUpdateStatus(false)
,m_bThreadFinish(false)
{
    m_sock = 0;
    memset(m_szIPAddr, 0, sizeof(char)*20);
    memset(m_szFilePath, 0, sizeof(char)*BUFLEN);

    memset( m_sendBuf, 0, MAX_SEND_ONE_BLOCK_SIZE+4 );

    memset(m_stUpLoadHeader, 0, sizeof(UPLOAD_HEADER) * 3 );
}


CUpdate::~CUpdate()
{
    if (NULL != m_hUpdateThread)
    {
        WaitForSingleObject(m_hUpdateThread, INFINITE);
        CloseHandle(m_hUpdateThread);
        m_hUpdateThread = NULL;
    }


    if (this->GetSafeHwnd())
        this->DestroyWindow();
}


DWORD WINAPI CUpdate::UpdateThread(void* pData)
{
    CUpdate* pCUpdate = (CUpdate*)pData;

    ASSERT( pCUpdate != NULL );
        
    pCUpdate->UpdateStart();
    
    return 0L;
}

BOOL CUpdate::Create( CWnd* _parent_window  )
{
    ASSERT(_parent_window  != NULL && _parent_window ->GetSafeHwnd());
	m_Parent_window = _parent_window ;
	
	RECT rc = { 0 };
	LPCTSTR lpszClassName =
		AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW)); // CS_DBLCLKS
	return CreateEx(0, lpszClassName, L"CUpdate", WS_CHILD|WS_VISIBLE, rc, _parent_window, 0);
}
bool CUpdate::SetIndex( int _iItemIdx, int _iLstIdx )
{
    ASSERT( this != NULL && this->GetSafeHwnd() != NULL );

    m_iItemIdx = _iItemIdx;
    m_iLstIdx  = _iLstIdx;
    return true;
}
bool CUpdate::GetIndex( int& _iItemIdx, int& _iLstIdx )
{
    _iItemIdx = m_iItemIdx;
    _iLstIdx  = m_iLstIdx;
    return true;
}

void CUpdate::setDeviceUpdate( LPCTSTR _lpszIP, short _shPort, LPCTSTR _lpszFilePath )
{
    // Check Parameter
    ASSERT( this->GetSafeHwnd()!= NULL  && NULL != _lpszIP
                                        && NULL != _lpszFilePath
                                        && 0    != _shPort       );

    // Setup Connect Info
    memset(m_szIPAddr, 0, sizeof(char)*20);
    USES_CONVERSION;
    memcpy( m_szIPAddr, W2A(_lpszIP), 20);

    memset( m_szFilePath, 0, sizeof(char)*BUFLEN);
    int ii = strlen(m_szFilePath);
    memcpy( m_szFilePath, W2A(_lpszFilePath), 255);

    m_shPort = _shPort;
    
    // Make Update Thread
    if (NULL != m_hUpdateThread)
    {
        WaitForSingleObject(m_hUpdateThread, INFINITE);
        CloseHandle(m_hUpdateThread);
        m_hUpdateThread = NULL;
    }

    // Update Start !!
    m_iVolume       = 0;
    m_bUpdateStatus = true;
    m_uiUpdateTimeID = SetTimer( (unsigned int)this, 500, NULL );
            
    m_hUpdateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, this, 0, &m_ThreadID );    
}

////////////////////////////////////////////////////////////////////////
static unsigned long CRC32( unsigned long inCrc32, const void *buf, int bufLen )
{
    unsigned long crc32;
    unsigned char *byteBuf;
    int i;

    /** accumulate crc32 for buffer **/
    crc32 = inCrc32 ^ 0xFFFFFFFF;
    byteBuf = (unsigned char*)buf;
    for (i=0; i < bufLen; i++) {
        crc32 = (crc32 >> 8) ^ s_crcTable[(crc32 ^ byteBuf[i]) & 0xFF];
    }
    return(crc32 ^ 0xFFFFFFFF);
}

void CUpdate::UpdateStart()
{
    m_csLock.Lock();   // CriticalSession Lock
    
    struct sockaddr_in serverAddr;
    memset( &serverAddr,0, sizeof(sockaddr_in) );

    int i       = 0,
        ret     = 0,
        totRead = 0,
        totSend = 0,
        nWrite  = 0;

    char *pContentData = NULL,
         *pStart       = NULL;

    char sHeader1[512],
         sHeader2[256],
         sTail[128]   ;
    memset( sHeader1, 0, sizeof(char)*512 );
    memset( sHeader2, 0, sizeof(char)*256 );
    memset(sTail, 0, sizeof(char)*128);

    long nContentLength = 0;
    int  nFile    = 0,
         nHeader1 = 0,
         nHeader2 = 0,
         nTail    = 0;

    FILE *fp = NULL;
    unsigned long nCrc32   = 0;
    int           nSend    = 0,
                  nSendErr = 0,
                  nRecv    = 0,
                  nRecvErr = 0;
    unsigned long nSeq     = 0;
    int           nUploadImage = 0,
                  nIdx         = 0;
    char          maxFile      = 0;

    memset( m_stUpLoadHeader, 0, sizeof( m_stUpLoadHeader) );

    WSADATA wsa;
    memset( &wsa, 0, sizeof(WSADATA) );
    int retVal = 0;
    
    //Initialise winsock
    TRACE( _T( "\nInitialising Winsock..." ) );
    if (WSAStartup( MAKEWORD( 2, 2 ), &wsa ) != 0)
    {
        TRACE( _T( "Failed. Error Code : %d" ), WSAGetLastError() );
        
        //========================== Escape
        m_bUpdateStatus = false;
        nIdx            = 4;
        goto END_FUNC;
        //========================== Escape
    }
    
    if ( fopen_s( &fp, m_szFilePath, "rb" ) )
    {
        TRACE( _T( "File Not Opened \n" ) );
        //========================== Escape
        m_bUpdateStatus = false;
        nIdx            = 4;
        goto END_FUNC;
        //========================== Escape
    }

    ret = fread( m_stUpLoadHeader, sizeof(char), sizeof(m_stUpLoadHeader), fp );
    if (ret != sizeof(m_stUpLoadHeader))
    {
        //========================== Escape
        m_bUpdateStatus = false;
        nIdx            = 4;
        goto END_FUNC;
        //========================== Escape
    }
        

    fseek( fp, 0, SEEK_END );
    nFile = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    TRACE( _T( "UPLAOD HEADER SIZE (%d)\n" ), sizeof(m_stUpLoadHeader) );
    //========================== Escape
    /*m_bUpdateStatus = false;
    nIdx            = 4;
    goto END_FUNC;*/
    //========================== Escape 


    // 3 File !! ******************************************************
    for (i=0; i<3; i++)
    {
        if (m_stUpLoadHeader[i].fileSize != 0)
            maxFile = i;
    }

    // 3 File Send !! ******************************************************
    for (nIdx=0; nIdx<3; nIdx++)
    {
        if (m_stUpLoadHeader[nIdx].fileSize == 0 || m_stUpLoadHeader[nIdx].startAddr == 0)
            continue;


        //--- set filename -------------------------------------/        
        nHeader2 = 0;
        memset(sHeader2, 0, sizeof(sHeader2));
        nHeader2 += sprintf_s( sHeader2, "%s", "------WebKitFormBoundaryvnNH4vwmzxuEZHIc\r\n" );
        nHeader2 += sprintf_s( sHeader2+nHeader2, sizeof(sHeader2)-nHeader2, "Content-Disposition: form-data; name=\"upfile\"; filename=\"%s\"; fileidx=\"%02d%02d\"\r\n", m_stUpLoadHeader[nIdx].filename, maxFile, nIdx);
        nHeader2 += sprintf_s( sHeader2+nHeader2, sizeof(sHeader2)-nHeader2, "Content-Type: application/octet-stream%d\r\n", nIdx );
        nHeader2 += sprintf_s( sHeader2+nHeader2, sizeof(sHeader2)-nHeader2, "%s", "\r\n" );
        sHeader2[nHeader2] = 0;

        nTail = sprintf_s( sTail, sizeof(sTail), "%s", "\r\n------WebKitFormBoundaryvnNH4vwmzxuEZHIc--\r\n" );
        sTail[nTail] = 0;
        //------------------------------------------------------/

        nContentLength = nHeader2 + sizeof(UPLOAD_HEADER)+m_stUpLoadHeader[nIdx].fileSize + nTail;
        TRACE( _T( "CONTENTLENGTGH = %d, %d, %d, %d = %d\n" ), sizeof(UPLOAD_HEADER), nHeader2, m_stUpLoadHeader[nIdx].fileSize, nTail, nContentLength );

        if ( NULL != pContentData) {
            free(pContentData);
            pContentData = NULL;
        }
        pContentData = (char *)malloc( nContentLength );
        memset( pContentData, 0, nContentLength );
        if ( NULL == pContentData )
        {
            //========================== Escape
            m_bUpdateStatus = false;
            nIdx            = 4;
            goto END_FUNC;
            //========================== Escape 
        }

        fseek( fp, m_stUpLoadHeader[nIdx].startAddr, SEEK_SET );

        pStart = pContentData;
        fseek( fp, m_stUpLoadHeader[nIdx].startAddr, SEEK_SET );
        memcpy( pContentData, sHeader2, nHeader2 );
        pContentData += nHeader2;

        m_stUpLoadHeader[nIdx].startAddr = sizeof(UPLOAD_HEADER);
        m_stUpLoadHeader[nIdx].fileType = (0x11+nIdx);

        memcpy( pContentData, (char *)&m_stUpLoadHeader[nIdx], sizeof(UPLOAD_HEADER) );
        pContentData += sizeof(UPLOAD_HEADER);

        totRead = 0;
        //(Harace) Post Message Prograss 각 인덱스별 파일 사이즈 종합해서 보내주기
        for (i=0; i<100; i++)
        {
            if ((ret=fread( pContentData+totRead, 1, m_stUpLoadHeader[nIdx].fileSize - totRead, fp )) > 0)
            {
                TRACE( _T( "%d: nRead (%d), ret (%d)\n" ), i, totRead, ret );
                totRead += ret;
            }

            if (ret <= 0 || totRead == m_stUpLoadHeader[nIdx].fileSize)
            {
                break;
            }
        }

        if (totRead != m_stUpLoadHeader[nIdx].fileSize)
        {
            //========================== Escape
            m_bUpdateStatus = false;
            nIdx            = 4;
            goto END_FUNC;
            //========================== Escape 
        }
            

        pContentData += totRead;
        memcpy( pContentData, sTail, nTail );
   
        // ?????????????????????????????????
        m_sock = socket( AF_INET, SOCK_STREAM, 0 );
        // ?????????????????????????????????
        
        if (m_sock == INVALID_SOCKET)
        {
            TRACE( _T( "socket error \n" ) );
            //========================== Escape
            m_bUpdateStatus = false;
            nIdx            = 4;
            goto END_FUNC;
            //========================== Escape 
        }

        nHeader1 = 0;
        nHeader1 += sprintf_s( sHeader1, "%s", "POST /appupdate.cgi HTTP/1.1\r\n" );
        nHeader1 += sprintf_s( sHeader1+nHeader1, sizeof(sHeader1)-nHeader1, "Host: %s\r\n", m_szIPAddr );
        nHeader1 += sprintf_s( sHeader1+nHeader1, sizeof(sHeader1)-nHeader1, "Content-Length: %d\r\n", nContentLength );
        nHeader1 += sprintf_s( sHeader1+nHeader1, sizeof(sHeader1)-nHeader1, "%s", "Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryvnNH4vwmzxuEZHIc\r\n" );
        nHeader1 += sprintf_s( sHeader1+nHeader1, sizeof(sHeader1)-nHeader1, "%s", "\r\n\r\n" );
        sHeader1[nHeader1] = 0;

        serverAddr.sin_family=AF_INET;
        serverAddr.sin_port = htons( 80 );
        serverAddr.sin_addr.s_addr = inet_addr( m_szIPAddr );
        retVal=connect( m_sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr) );

        if (retVal==SOCKET_ERROR)
        {
            TRACE( _T( "connect error\n" ) );
            //========================== Escape
            m_bUpdateStatus = false;
            nIdx            = 4;
            goto END_FUNC;
            //========================== Escape 
        }

        if (send( m_sock, sHeader1, nHeader1, 0 ) < 0)
        {
            TRACE( _T( "error : send update header 1\n" ) );
            //========================== Escape
            m_bUpdateStatus = false;
            nIdx            = 4;
            goto END_FUNC;
            //========================== Escape 
        }

        totSend = nSeq = nSend = nSendErr = nRecv = nRecvErr = 0;
        pContentData = pStart;
        //continue;

        i = 0;
        while (i++ < 20000)
        {
            //------ send file ----------------------------------/	
            nWrite = nContentLength - totSend;
            if (nWrite > MAX_SEND_ONE_BLOCK_SIZE)
                nWrite = MAX_SEND_ONE_BLOCK_SIZE;

            if (totSend >= nContentLength)
                break;

            memcpy( m_sendBuf+8, pContentData+totSend, nWrite );

            nCrc32 = CRC32( 0, pContentData+totSend, nWrite );
            memcpy(  m_sendBuf, &nCrc32, 4 );
            memcpy(  m_sendBuf+4, &nSeq, 4 );

            nSend = send( m_sock, m_sendBuf, 8+nWrite, 0 );
            if (nSend <= 0)
            {
                TRACE( _T( "error : send file data - nsend(%d) \n" ), nSend );
                if (nSendErr++ > 10)
                {
                    //========================== Escape
                    m_bUpdateStatus = false;
                    nIdx            = 4;
                    goto END_FUNC;
                    //========================== Escape 
                }
                else
                {
                    Sleep( 3000 );
                    continue;
                }
            }

            nSeq++;
            totSend += (nSend - 8);
            nSendErr = 0;

            TRACE( _T( "cur nSeq = %d, totsize = %x, %d\n" ), nSeq, totSend, totSend );

            nRecv = recv( m_sock, sHeader1, sizeof(sHeader1), 0 );

            if (nRecv > 0)
            {
                
                //(Harace) Post Message Prograss 진행도 표기 전체 크기 대비
                sHeader1[nRecv] = 0;
                if (strncmp( sHeader1+9, "400", 3 ) == 0)
                {
                    char *pTok = strstr( sHeader1+9+3, "ERROR:" );
                    if (pTok)
                    {
                        TRACE( _T( "ERROR:%s\n"), pTok );

                        if (nRecvErr++ > 10)
                        {
                    //========================== Escape
                    m_bUpdateStatus = false;
                    nIdx            = 4;
                    goto END_FUNC;
                    //========================== Escape 
                        }

                        if (strncmp( pTok+6, "CRC", 3 ) == 0)
                        {
                            int toSeq = atol( pTok+6+4 );
                            TRACE( _T( "TOCRC : %d, curSeq %d, totSend %x, \n" ), toSeq, nSeq, totSend );
                            totSend -= (nSeq - toSeq-1)*MAX_SEND_ONE_BLOCK_SIZE;

                            nSeq = toSeq+1;
                            TRACE( _T( "CHANGE : %d,curSeq %d, totSend %x, \n" ), nSeq, nSeq, totSend );
                        }
                        else if (strncmp( pTok+6, "SEQ", 3 ) == 0)
                        {
                            int toSeq = atol( pTok+6+4 );
                            TRACE( _T( "TOSEQ : %d,curSeq %d, totSend %x, \n" ), toSeq, nSeq, totSend );
                            totSend -= (nSeq - toSeq-1)*MAX_SEND_ONE_BLOCK_SIZE;

                            nSeq = toSeq+1;
                            TRACE( _T( "CHANGE : %d,curSeq %d, totSend %x, \n" ), nSeq, nSeq, totSend );
                        }
                    }
                    Sleep( 200 );
                    continue;
                }
            }

            nRecvErr = 0;
        }

        TRACE( _T( "RESULT nSeq = %d, totsize = %x, %d\n" ), nSeq, totSend, totSend );
        //m_bUpdateStatus = true;

    END_FUNC:
        Sleep( 2000 );

        if (pStart)
        {
            free( pStart );
            pStart = NULL;
            TRACE( _T( "Content Data Free !!\n" ) );
            if (pContentData)
                pContentData = NULL;
        }

        if (m_sock)
        {
            closesocket( m_sock );
            m_sock = 0;
            TRACE( _T( "Close Socket !!\n" ) );
        }
    }

    if (fp)
    {
        fclose( fp );
        TRACE( _T( "File Close !!\n" ) );
    }

    WSACleanup();
    TRACE( _T( "Socket Clean Up !!!\n" ) );

    m_bThreadFinish = true;
    
    m_hUpdateThread = NULL;

    m_csLock.Unlock(); // CriticalSession UnLock

   
    //WaitForSingleObject( m_hUpdateThreadExit, INFINITE );
    //m_Parent_window->PostMessage(, , , (LPARAM)this)
    
}


void CUpdate::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
    if ( 0 != m_uiUpdateTimeID && nIDEvent == m_uiUpdateTimeID )
    {
        if ( NULL != this->GetSafeHwnd() )
        {
            if ( m_iVolume == 95 )
            {
                if (m_bThreadFinish)
                    m_iVolume += 1;
                else
                    return;
            }
            else if ( m_iVolume == 101 )
            {
                KillTimer( m_uiUpdateTimeID );
                m_uiUpdateTimeID = 0;

                m_bUpdateStatus = false;
                m_bThreadFinish = false;
            }
            else
            {
                m_iVolume += 1;
            }


            if ( m_bUpdateStatus )
            {
                UPDATE_INFO_ITEM *pUpdateItem = new UPDATE_INFO_ITEM;                
                pUpdateItem->iUpdateStatus    = ((m_iVolume == 101)? UPDATE_FINISH : UPDATE_START);
                pUpdateItem->iItemIdx         = m_iItemIdx;
                pUpdateItem->iLstIdx          = m_iLstIdx;
                pUpdateItem->iVolume          = m_iVolume;
                ::PostMessage(this->m_Parent_window->GetSafeHwnd(), WM_UPDATE_DATA,(WPARAM)this, (LPARAM)pUpdateItem );
            }
            else
            {
                UPDATE_INFO_ITEM *pUpdateItem = new UPDATE_INFO_ITEM;                
                pUpdateItem->iUpdateStatus    = UPDATE_FAIL;
                pUpdateItem->iItemIdx         = m_iItemIdx;
                pUpdateItem->iLstIdx          = m_iLstIdx;
                pUpdateItem->iVolume          = m_iVolume;
                ::PostMessage(this->m_Parent_window->GetSafeHwnd(), WM_UPDATE_DATA,(WPARAM)this, (LPARAM)pUpdateItem );

                KillTimer(m_uiUpdateTimeID);
                m_uiUpdateTimeID = 0;

                m_bUpdateStatus = false;
                m_bThreadFinish = false;
            }
        }
    }

    CWnd::OnTimer(nIDEvent);
}
