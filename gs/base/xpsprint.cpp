#include <stdio.h>
#include <windows.h>
#include <XpsObjectModel.h>
#include <XpsPrint.h>
#include <prntvpt.h>

#include "string_.h"
#include "gx.h"
#include "gserrors.h"

#undef eprintf1
#define eprintf1(str, arg) {}
#undef eprintf
#define eprintf(str) {}
#undef gs_note_error
#define gs_note_error(error) return(error)

extern "C" int XPSPrint(char *FileName, char *PrinterName, int *result)
{
    HRESULT hr = S_OK;
    HANDLE completionEvent = NULL;
    IXpsPrintJob* job = NULL;
    IXpsPrintJobStream* jobStream = NULL;
    IXpsOMObjectFactory* xpsFactory = NULL;
    IOpcPartUri* partUri = NULL;
    IXpsOMPackage* package = NULL;
    IXpsOMPackageWriter* packageWriter = NULL;
    IXpsOMPage* xpsPage = NULL;
    IXpsOMFontResource* fontResource = NULL;
    XPS_JOB_STATUS jobStatus = {};
    HPTPROVIDER pProvider;
    HGLOBAL hGlobal = NULL;
    IStream *pCapabilities;
    BSTR *pbstrErrorMessage = NULL;
    int code = 0;

    if (FAILED(hr = CoInitializeEx(0, COINIT_MULTITHREADED)))
    {
        *result = hr;
        code = -1;
    }

    if (SUCCEEDED(hr))
    {
        completionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!completionEvent)
        {
            *result = hr;
            code = -2;
        }
    }

    /* Need code for PrintTIcket here */

    if (SUCCEEDED(hr))
    {
        WCHAR MBStr[64];
        int code;

        code = MultiByteToWideChar(CP_ACP, 0, PrinterName, -1, MBStr, 64);
        if (code != 0) {
            if (FAILED(hr = StartXpsPrintJob(
                        (LPCWSTR)MBStr,
                        NULL,
                        NULL,
                        NULL,
                        completionEvent,
                        NULL,
                        0,
                        &job,
                        &jobStream,
                        NULL
                        )))
            {
                *result = hr;
                code = -4;
            }
        } else {
            *result = 0;
            code = -3;
            hr = -1;
        }
    }

    if (SUCCEEDED(hr))
    {
        if (FAILED(hr = CoCreateInstance(
                    __uuidof(XpsOMObjectFactory),
                    NULL,
                    CLSCTX_INPROC_SERVER,
                    __uuidof(IXpsOMObjectFactory),
                    reinterpret_cast<void**>(&xpsFactory)
                    )
                )
            )
        {
            *result = hr;
            code = -5;
        }
    }

    if (SUCCEEDED(hr))
    {
        WCHAR MBStr[MAX_PATH];
        int code;

        code = MultiByteToWideChar(CP_ACP, 0, FileName, -1, MBStr, MAX_PATH);
        if (code != 0) {
            if (FAILED(hr = xpsFactory->CreatePackageFromFile((LPCWSTR)MBStr, false, &package))){
                *result = hr;
                code = -7;
            }
        } else {
            hr = -1;
            *result = 0;
            code = -6;
        }
    }
    if (SUCCEEDED(hr))
    {
        if (FAILED(hr = package->WriteToStream(jobStream, FALSE))) {
            *result = hr;
            code = -8;
        }
    }

    if (SUCCEEDED(hr))
    {
        if (FAILED(hr = jobStream->Close()))
        {
            *result = hr;
            code = -9;
        }
    }
    else
    {
        // Only cancel the job if we succeeded in creating one in the first place.
        if (job)
        {
            // Tell the XPS Print API that we're giving up.  Don't overwrite hr with the return from
            // this function.
            job->Cancel();
        }
    }


    if (SUCCEEDED(hr))
    {
        if (WaitForSingleObject(completionEvent, INFINITE) != WAIT_OBJECT_0)
        {
            *result = hr = HRESULT_FROM_WIN32(GetLastError());
            code = -10;
        }
    }

    if (SUCCEEDED(hr))
    {
        if (FAILED(hr = job->GetJobStatus(&jobStatus)))
        {
            *result = hr;
            code = -11;
        }
    }

    if (SUCCEEDED(hr))
    {
        switch (jobStatus.completion)
        {
            case XPS_JOB_COMPLETED:
                break;
            case XPS_JOB_CANCELLED:
                hr = E_FAIL;
                code = -12;
                break;
            case XPS_JOB_FAILED:
                hr = E_FAIL;
                *result = jobStatus.jobStatus;
                code = -13;
                break;
            default:
                hr = E_UNEXPECTED;
                code = -14;
                break;
        }
    }

    if (fontResource)
    {
        fontResource->Release();
        fontResource = NULL;
    }

    if (xpsPage)
    {
        xpsPage->Release();
        xpsPage = NULL;
    }

    if (packageWriter)
    {
        packageWriter->Release();
        packageWriter = NULL;
    }

    if (partUri)
    {
        partUri->Release();
        partUri = NULL;
    }

    if (xpsFactory)
    {
        xpsFactory->Release();
        xpsFactory = NULL;
    }

    if (jobStream)
    {
        jobStream->Release();
        jobStream = NULL;
    }

    if (job)
    {
        job->Release();
        job = NULL;
    }

    if (completionEvent)
    {
        CloseHandle(completionEvent);
        completionEvent = NULL;
    }

    CoUninitialize();

    return code;
}
