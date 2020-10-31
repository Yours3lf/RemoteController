#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <Windows.h>
#include <http.h>
#include <stdio.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <locale>
#include <codecvt>

std::string webpage;

void check(ULONG ret)
{
    if (ret != NO_ERROR)
    {
        char buf[1024];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, ret, 0, buf, 1024, 0);
        MessageBoxA(0, buf, 0, MB_OK);
        exit(0);
    }
}

void handleAction(WORD key)
{
    INPUT i = {};
    i.type = INPUT_KEYBOARD;

    i.ki.wVk = key;

    SendInput(1, &i, sizeof(i));
}

DWORD sendHttpResponse(
    HANDLE                  reqHandle,
    PHTTP_REQUEST           request,
    USHORT                  statusCode,
    const std::string&     reason,
    const std::string&     entityString
)
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    std::string header = "text/html";

    memset(&response, 0, sizeof(response));
    response.StatusCode = statusCode;
    response.pReason = reason.c_str();
    response.ReasonLength = reason.length();

    response.Headers.KnownHeaders[HttpHeaderContentType].pRawValue = header.c_str();
    response.Headers.KnownHeaders[HttpHeaderContentType].RawValueLength = header.length();

    if (!entityString.empty())
    {
        dataChunk.DataChunkType = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer = (PVOID)entityString.c_str();
        dataChunk.FromMemory.BufferLength = entityString.length();
        
        response.EntityChunkCount = 1;
        response.pEntityChunks = &dataChunk;
    }

    result = HttpSendHttpResponse(reqHandle, request->RequestId, 0, &response, 0, &bytesSent, 0, 0, 0, 0); check(result);

    return result;
}

ULONG handleRequest(HANDLE reqHandle)
{
    ULONG ret = 0;

    HTTP_REQUEST_ID requestId;
    HTTP_SET_NULL_ID(&requestId);

    ULONG requestBufferLength = sizeof(HTTP_REQUEST) + 2048;

    std::vector<char> requestBuffer;
    requestBuffer.resize(requestBufferLength);

    PHTTP_REQUEST request = (PHTTP_REQUEST)requestBuffer.data();

    DWORD bytesRead;

    while (true)
    {
        memset(request, 0, requestBufferLength);

        ret = HttpReceiveHttpRequest(reqHandle, requestId, 0, request, requestBufferLength, &bytesRead, 0);

        if (ret == NO_ERROR)
        {
            if (request->Verb == HttpVerbGET)
            {         
                //MessageBoxW(0, request->CookedUrl.pAbsPath, 0, MB_OK);
                //MessageBoxW(0, request->CookedUrl.pFullUrl, 0, MB_OK);

                if (request->CookedUrl.AbsPathLength > 2)
                {
                    std::wstring absPath(request->CookedUrl.pAbsPath);

                    //i.ki.wVk = VK_MEDIA_PLAY_PAUSE;
                    //i.ki.wVk = VK_MEDIA_STOP;
                    //i.ki.wVk = VK_MEDIA_NEXT_TRACK;
                    //i.ki.wVk = VK_MEDIA_PREV_TRACK;

                    //i.ki.wVk = VK_VOLUME_MUTE;
                    //i.ki.wVk = VK_VOLUME_DOWN;
                    //i.ki.wVk = VK_VOLUME_UP;

                    if (absPath == L"/playPause")
                    {
                        handleAction(VK_MEDIA_PLAY_PAUSE);
                    }
                    else if (absPath == L"/stop")
                    {
                        handleAction(VK_MEDIA_STOP);
                    }
                    else if (absPath == L"/next")
                    {
                        handleAction(VK_MEDIA_NEXT_TRACK);
                    }
                    else if (absPath == L"/previous")
                    {
                        handleAction(VK_MEDIA_PREV_TRACK);
                    }
                    else if (absPath == L"/muteToggle")
                    {
                        handleAction(VK_VOLUME_MUTE);
                    }
                    else if (absPath == L"/volumeUp")
                    {
                        handleAction(VK_VOLUME_UP);
                    }
                    else if (absPath == L"/volumeDown")
                    {
                        handleAction(VK_VOLUME_DOWN);
                    }
                    else if (absPath == L"/exit")
                    {
                        break;
                    }

                    ret = sendHttpResponse(
                        reqHandle,
                        request,
                        200,
                        "OK",
                        "OK"
                    );
                }
                else
                {
                    ret = sendHttpResponse(
                        reqHandle,
                        request,
                        200,
                        "OK",
                        webpage.c_str()
                    );
                }
            }
            //else if (request->Verb == HttpVerbPOST)
            //{
            //    ret = sendHttpPostResponse(reqHandle, request);
            //}
            else
            {
                ret = sendHttpResponse(
                    reqHandle,
                    request,
                    503,
                    "Not Implemented",
                    0
                );
            }

            check(ret);

            HTTP_SET_NULL_ID(&requestId);
        }
        else if (ret == ERROR_MORE_DATA)
        {
            requestId = request->RequestId;

            requestBufferLength = bytesRead;
            requestBuffer.resize(requestBufferLength);

            request = (PHTTP_REQUEST)requestBuffer.data();
        }
        else if (ret == ERROR_CONNECTION_INVALID && !HTTP_IS_NULL_ID(&requestId))
        {
            // The TCP connection was corrupted by the peer when
            // attempting to handle a request with more buffer. 
            // Continue to the next request.

            HTTP_SET_NULL_ID(&requestId);
        }
        else
        {
            break;
        }
    }

    return ret;
}

std::wstring getIPaddress()
{
    std::string ipAddress;

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    std::vector<char> adapter_info;
    adapter_info.resize(sizeof(IP_ADAPTER_INFO));
    ULONG adapterLen = sizeof(IP_ADAPTER_INFO);
    while (GetAdaptersInfo((PIP_ADAPTER_INFO)adapter_info.data(), &adapterLen) == ERROR_BUFFER_OVERFLOW)
    {
        adapter_info.resize(adapterLen);
    }

    PIP_ADAPTER_INFO adapter = (PIP_ADAPTER_INFO)adapter_info.data();
    while (adapter)
    {
        if (adapter->Type == MIB_IF_TYPE_ETHERNET)
        {
            ipAddress = adapter->IpAddressList.IpAddress.String;
            if (ipAddress != "0.0.0.0")
            {
                break;
            }
        }

        adapter = adapter->Next;
    }

    return converter.from_bytes(ipAddress);
}

INT __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    ////////////////////
    ////////////////////
    // Init server
    ////////////////////
    ////////////////////

    HTTPAPI_VERSION httpApiVersion = HTTPAPI_VERSION_1;

    ULONG ret = 0;
    ret = HttpInitialize(httpApiVersion, HTTP_INITIALIZE_SERVER, 0); check(ret);

    HANDLE reqHandle = 0;
    ret = HttpCreateHttpHandle(&reqHandle, 0); check(ret);

    std::wstring ip = getIPaddress();
    std::wstring wstr = L"http://" + ip + L":80/";
    //MessageBoxW(0, wstr.c_str(), 0, MB_OK);
    ret = HttpAddUrl(reqHandle, wstr.c_str(), 0); check(ret);

    { //read in webpage
        std::ifstream t("Remote.htm");
        std::string str((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());
        webpage = str;
    }

    ////////////////////
    ////////////////////
    // Handle requests
    ////////////////////
    ////////////////////

    ret = handleRequest(reqHandle); check(ret);

    ////////////////////
    ////////////////////
    // Cleanup
    ////////////////////
    ////////////////////

    HttpRemoveUrl(reqHandle, wstr.c_str());

    if (reqHandle)
    {
        CloseHandle(reqHandle);
    }

    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

    return 0;
}