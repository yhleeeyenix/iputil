#pragma once
#include "afxwin.h"


static unsigned long CRC32(unsigned long inCrc32, const void *buf, int bufLen);

 //static char sHeader1[512], sHeader2[256], sTail[128];

class CUpdate : public CWnd
{
    DECLARE_DYNAMIC(CUpdate)

public:
    CUpdate();
    ~CUpdate();

    DECLARE_MESSAGE_MAP()

private:
    CWnd*        m_Parent_window;
    
    char         m_szIPAddr[20];
    short        m_shPort;

    char         m_szFilePath[BUFLEN];
    
    int          m_iItemIdx;
    int          m_iLstIdx;
    int          m_iVolume;
    bool         m_bUpdateStatus;
    bool         m_bThreadFinish;
    unsigned int m_uiUpdateTimeID;
    
    CCriticalSection m_csLock;

    SOCKET             m_sock;        
    char               m_sendBuf[MAX_SEND_ONE_BLOCK_SIZE+4];
    UPLOAD_HEADER       m_stUpLoadHeader[3];

    HANDLE              m_hUpdateThread;    
    DWORD               m_ThreadID;

    
    static DWORD WINAPI UpdateThread(void* pData);
    

public:
    virtual BOOL Create(CWnd* _parent_window);

    bool         SetIndex(int _iItemIdx, int _iLstIdx);
    bool         GetIndex(int& _iItemIdx, int& _iLstIdx);
    void         setDeviceUpdate(LPCTSTR _lpszIP, short _shPort, LPCTSTR _lpszFilePath); // Device Update

    //void End();

    afx_msg void OnTimer(UINT_PTR nIDEvent);

private:
    
    void UpdateStart();
};