/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

#include "windows_.h"
#include <string>
#include <sstream>
#include <windows.storage.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

extern "C" DWORD GetTempPathWRT(DWORD nBufferLength, LPWSTR lpBuffer)
{
    try
    {
        HRESULT hr;
        Microsoft::WRL::ComPtr<ABI::Windows::Storage::IApplicationDataStatics> applicationDataStatics;

        hr = Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(
                 RuntimeClass_Windows_Storage_ApplicationData).Get(), &applicationDataStatics);
        if (FAILED(hr))
            return 0;

        Microsoft::WRL::ComPtr<ABI::Windows::Storage::IApplicationData> applicationData;
        hr = applicationDataStatics->get_Current(&applicationData);
        if (FAILED(hr))
            return 0;

        Microsoft::WRL::ComPtr<ABI::Windows::Storage::IStorageFolder> storageFolder;
        hr = applicationData->get_TemporaryFolder(&storageFolder);
        if (FAILED(hr))
            return 0;

        Microsoft::WRL::ComPtr<ABI::Windows::Storage::IStorageItem> storageItem;
        hr = storageFolder.As(&storageItem);
        if (FAILED(hr))
            return 0;

        HSTRING folderName;
        hr = storageItem->get_Path(&folderName);
        if (FAILED(hr))
            return 0;

        UINT32 length;
        PCWSTR value = WindowsGetStringRawBuffer(folderName, &length);
        std::wstring ws(value);
        wcsncpy(lpBuffer, ws.c_str(), nBufferLength);
        lpBuffer[nBufferLength-1] = 0;
        WindowsDeleteString(folderName);
        return ws.length();
    }
    catch(...)
    {
        return 0;
    }
}

extern "C" UINT GetTempFileNameWRT(LPCWSTR lpPathName, LPCWSTR lpPrefixString, LPWSTR lpTempFileName)
{
    try
    {
       std::wstring path(lpPathName);
       std::wstring prefix(lpPrefixString);
       FILETIME systemTimeAsFileTime;

       if (!path.empty() && path.back() != '\\')
           path.push_back('\\');

       if (path.length() > _MAX_PATH - 14)
           return ERROR_BUFFER_OVERFLOW;

       GetSystemTimeAsFileTime(&systemTimeAsFileTime);

       DWORD time = systemTimeAsFileTime.dwLowDateTime;
       while(true)
       {
           // Create file name of at most 13 characters, using at most 10
           // digits of time, and as many of the prefix characters as can fit
           std::wstring num(std::to_wstring((UINT)time));
           if (num.length() > 10)
               num = num.substr(num.length() - 10);
           std::wstring pf(prefix);
           if (num.length() + pf.length() > 13)
               pf.resize(13 - num.length());
           std::wstring fullpath = path+pf+num;

           // Test that the file doesn't already exist
           LPWIN32_FIND_DATA find_data;
           HANDLE h = FindFirstFileExW(fullpath.c_str(), FindExInfoStandard, &find_data, FindExSearchNameMatch, NULL, 0);
           if (h == INVALID_HANDLE_VALUE)
           {
               // File doesn't exist
               wcscpy(lpTempFileName, fullpath.c_str());
               return fullpath.length();
           }

           // File exists. Increment time and try again
           FindClose(h);
           time++;
       }

       return ERROR_BUFFER_OVERFLOW;
    }
    catch(...)
    {
        return ERROR_BUFFER_OVERFLOW;
    }
}

extern "C" void OutputDebugStringWRT(LPCSTR str, DWORD len)
{
	try
	{
		std::string s(str, len);
		std::wstring w(s.begin(), s.end());
		OutputDebugStringW(w.c_str());
	}
	catch(...)
	{
	}
}
