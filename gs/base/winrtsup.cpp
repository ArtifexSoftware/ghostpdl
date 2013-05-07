/* Copyright (C) 2001-2013 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "windows_.h"
#include <string>
#include <sstream>
#include <windows.storage.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

extern "C" DWORD GetTempPathWRT(DWORD nBufferLength, LPSTR lpBuffer)
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
        std::string s;
        s.assign(ws.begin(), ws.end());
        strncpy(lpBuffer, s.c_str(), nBufferLength);
        WindowsDeleteString(folderName);
        return s.length();
    }
    catch(...)
    {
        return 0;
    }
}

extern "C" UINT GetTempFileNameWRT(LPCSTR lpPathName, LPCSTR lpPrefixString, LPSTR lpTempFileName)
{
    try
    {
       std::string path(lpPathName);
       std::string prefix(lpPrefixString);

       if (!path.empty() && path.back() != '\\')
           path.push_back('\\');

       if (path.length() > _MAX_PATH - 14)
           return ERROR_BUFFER_OVERFLOW;

       DWORD time = GetTickCount();
       while(true)
       {
           // Create file name of at most 13 characters, using at most 10
           // digits of time, and as many of the prefix characters as can fit
           std::stringstream str;
           str << (UINT)time;
           std::string num(str.str());
           if (num.length() > 10)
               num = num.substr(num.length() - 10);
           std::string pf(prefix);
           if (num.length() + pf.length() > 13)
               pf.resize(13 - num.length());
           std::string fullpath = path+pf+num;

           // Test that the file doesn't already exist
           std::wstring wfullpath(fullpath.begin(), fullpath.end());
           LPWIN32_FIND_DATA find_data;
           HANDLE h = FindFirstFileExW(wfullpath.c_str(), FindExInfoStandard, &find_data, FindExSearchNameMatch, NULL, 0);
           if (h == INVALID_HANDLE_VALUE)
           {
               // File doesn't exist
               strcpy(lpTempFileName, fullpath.c_str());
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