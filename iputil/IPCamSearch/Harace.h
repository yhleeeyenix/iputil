#pragma once
#pragma comment(lib, "version.lib")

namespace AHP
{

#define MD_MAX_MSG_BUFFER		1024
void DebugMsgBox(const TCHAR * pMsg, ...)
{
#ifndef _DEBUG
	return;			// When if release mode return.
#endif

	TCHAR szBuffer[MD_MAX_MSG_BUFFER];
	memset(szBuffer, 0x00, sizeof(szBuffer));

	va_list vaList;
	va_start(vaList, pMsg);
	_vsntprintf_s(szBuffer, sizeof(szBuffer) - 1, pMsg, vaList);        
	va_end(vaList);

	AfxMessageBox(szBuffer, MB_TOPMOST);
}

CString GetAppPath()
{
	CString strModulePath = _T("");
	TCHAR szModuleFileName[MAX_PATH];
	memset(szModuleFileName, 0x00, sizeof(szModuleFileName));
	if (GetModuleFileName(NULL, szModuleFileName, sizeof(szModuleFileName)) == 0)
	{
		TRACE(_T("GetModuleFileName failed. Error(%d) \n"), GetLastError()); 
		return strModulePath;
	}

	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szPath[_MAX_PATH];
	TCHAR szFilename[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	memset(szDrive, 0x00, sizeof(szDrive));
	memset(szPath, 0x00, sizeof(szPath));
	memset(szFilename, 0x00, sizeof(szFilename));
	memset(szExt, 0x00, sizeof(szExt));
	_wsplitpath_s(szModuleFileName, szDrive, szPath, szFilename, szExt);

	strModulePath.Format(_T("%s%s"), szDrive, szPath);
	if (strModulePath.Right(1).Compare(_T("\\")) == 0)
		strModulePath.Delete(strModulePath.GetLength() - 1);
	return strModulePath; // has trailing backslash
}


DWORD GetRegDword(LPCTSTR lpRegKey, LPCTSTR lpValueName, LPSTR lpData, DWORD dwDataSize)
{
	DWORD dwData = 0;
	HKEY hKey = NULL;
	BOOL bSuccess = FALSE;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpRegKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD cbData = sizeof(dwData);
		if (RegQueryValueEx(hKey, lpValueName, NULL, NULL, (LPBYTE)&dwData, &cbData) == ERROR_SUCCESS)
		{
			// 레지스트리값 읽기 성공
			bSuccess = TRUE;
		}

		RegCloseKey(hKey);
	}

	if (bSuccess == FALSE)
	{
		TRACE(_T("GetRegDword Failed!! Regkey=%s, ValueName=%s \n"), lpRegKey, lpValueName);
	}

	return dwData;
}

BOOL GetRegString(LPCTSTR lpRegKey, LPCTSTR lpValueName, LPTSTR lpData, DWORD dwDataSize)
{
	CString strData = _T("");
	HKEY hKey = NULL;
	BOOL bSuccess = FALSE;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpRegKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD dwType = REG_SZ;
		//		BYTE szInstallDir[MAX_PATH];
		//		memset(szValue, 0x00, sizeof());
		//		DWORD cbData = sizeof(dwLanguage);
		if (RegQueryValueEx(hKey, lpValueName, NULL, NULL, (LPBYTE)lpData, &dwDataSize) == ERROR_SUCCESS)
		{
			// 레지스트리값 읽기 성공
			bSuccess = TRUE;
		}

		RegCloseKey(hKey);
	}

	if (bSuccess == FALSE)
	{
		TRACE(_T("GetRegString Failed!! Regkey=%s, ValueName=%s \n"), lpRegKey, lpValueName);
	}

	return bSuccess;
}


BOOL GetFileVersion(LPTSTR lpLibName, WORD * pMajorVersion, WORD * pMinorVersion, WORD * pBuildNumber, WORD * pRevisionNumber)
{
	if (pMajorVersion == NULL)		return FALSE;
	if (pMinorVersion == NULL)		return FALSE;
	if (pBuildNumber == NULL)		return FALSE;
	if (pRevisionNumber == NULL)	return FALSE;

	TCHAR szLibName[MAX_PATH] = {0};
	if (lpLibName == NULL)
		::GetModuleFileName(NULL, szLibName, MAX_PATH);
	else
		::_tcscpy_s(szLibName, lpLibName);

	DWORD dwHandle = 0;
	DWORD dwLen = 0;
	UINT BufLen = 0;
	LPTSTR lpData = NULL;
	VS_FIXEDFILEINFO * pFileInfo = NULL;

	dwLen = GetFileVersionInfoSize(szLibName, &dwHandle);
	if (!dwLen)
		return FALSE;

	lpData = (LPTSTR)malloc(dwLen);
	if (!lpData)
		return FALSE;

	if (GetFileVersionInfo(szLibName, dwHandle, dwLen, lpData) == FALSE) 
	{
		free (lpData);
		return FALSE;
	}

	if (VerQueryValue(lpData, _T("\\"), (LPVOID *)&pFileInfo, (PUINT)&BufLen) == FALSE)
	{
		free (lpData);
		return FALSE;
	}

	*pMajorVersion = HIWORD(pFileInfo->dwFileVersionMS);
	*pMinorVersion = LOWORD(pFileInfo->dwFileVersionMS);
	*pBuildNumber = HIWORD(pFileInfo->dwFileVersionLS);
	*pRevisionNumber = LOWORD(pFileInfo->dwFileVersionLS);

	free (lpData);
	return TRUE;
}

CString GetFileVersion(LPTSTR lpLibName)
{
	CString strFileVersion = _T("");
	WORD wMajorVersion = 0;
	WORD wMinorVersion = 0;
	WORD wBuildNumber = 0;
	WORD wRevisionNumber = 0;

	if (GetFileVersion(lpLibName, &wMajorVersion, &wMinorVersion, &wBuildNumber, &wRevisionNumber))
	{
		strFileVersion.Format(_T("%d.%d.%d.%d"), wMajorVersion, wMinorVersion, wBuildNumber, wRevisionNumber);
	}

	return strFileVersion;
}

template<class T> 
void swap( T& lhs, T& rhs ) 
{ 
    T temp = lhs; 
    lhs = rhs; 
    rhs = temp; 
}
 
} // Namespace AHP

using namespace AHP;
