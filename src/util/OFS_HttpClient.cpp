#include "OFS_HttpClient.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <iostream>
#include <sstream>

OFS_HttpClient::OFS_HttpClient() noexcept
{
}

OFS_HttpClient::~OFS_HttpClient() noexcept
{
}

bool OFS_HttpClient::Get(const std::string& url, std::string& response) noexcept
{
#ifdef _WIN32
    // Parse URL using WinHttp
    URL_COMPONENTS components;
    ZeroMemory(&components, sizeof(components));
    components.dwStructSize = sizeof(components);
    
    wchar_t scheme[32] = {};
    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    
    components.lpszScheme = scheme;
    components.dwSchemeLength = ARRAYSIZE(scheme);
    components.lpszHostName = hostName;
    components.dwHostNameLength = ARRAYSIZE(hostName);
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = ARRAYSIZE(urlPath);
    
    // Convert URL to wstring
    std::wstring wurl(url.begin(), url.end());
    
    if (!WinHttpCrackUrl(wurl.c_str(), wurl.length(), 0, &components)) {
        lastError_ = "Failed to parse URL";
        return false;
    }
    
    // Open session
    HINTERNET session = WinHttpOpen(L"OFS_HttpClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        lastError_ = "Failed to open WinHTTP session";
        return false;
    }
    
    // Connect
    HINTERNET connection = WinHttpConnect(session, hostName, components.nPort, 0);
    if (!connection) {
        lastError_ = "Failed to connect to server";
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Request
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
        (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        lastError_ = "Failed to create request";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Set timeout
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeoutMs_, sizeof(timeoutMs_));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutMs_, sizeof(timeoutMs_));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeoutMs_, sizeof(timeoutMs_));
    
    // Send request
    if (!WinHttpSendRequest(request, NULL, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        lastError_ = "Failed to send request";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Receive response
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode != 200) {
        lastError_ = "HTTP error: " + std::to_string(statusCode);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Read response body
    std::string body;
    char buffer[4096];
    DWORD bytesRead = 0;
    
    while (WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        body.append(buffer, bytesRead);
    }
    
    response = body;
    
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    
    return true;
#else
    lastError_ = "HTTP not supported on this platform";
    return false;
#endif
}

bool OFS_HttpClient::Post(const std::string& url, const std::string& body, std::string& response, const std::string& contentType) noexcept
{
#ifdef _WIN32
    // Parse URL using WinHttp
    URL_COMPONENTS components;
    ZeroMemory(&components, sizeof(components));
    components.dwStructSize = sizeof(components);
    
    wchar_t scheme[32] = {};
    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    
    components.lpszScheme = scheme;
    components.dwSchemeLength = ARRAYSIZE(scheme);
    components.lpszHostName = hostName;
    components.dwHostNameLength = ARRAYSIZE(hostName);
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = ARRAYSIZE(urlPath);
    
    // Convert URL to wstring
    std::wstring wurl(url.begin(), url.end());
    
    if (!WinHttpCrackUrl(wurl.c_str(), wurl.length(), 0, &components)) {
        lastError_ = "Failed to parse URL";
        return false;
    }
    
    // Open session
    HINTERNET session = WinHttpOpen(L"OFS_HttpClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        lastError_ = "Failed to open WinHTTP session";
        return false;
    }
    
    // Connect
    HINTERNET connection = WinHttpConnect(session, hostName, components.nPort, 0);
    if (!connection) {
        lastError_ = "Failed to connect to server";
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Request
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
        (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        lastError_ = "Failed to create request";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Set headers
    std::wstring wcontentType(contentType.begin(), contentType.end());
    std::wstring headers = L"Content-Type: " + wcontentType;
    WinHttpAddRequestHeaders(request, headers.c_str(), headers.length(), WINHTTP_ADDREQ_FLAG_ADD);
    
    // Set timeout
    WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeoutMs_, sizeof(timeoutMs_));
    WinHttpSetOption(request, WINHTTP_OPTION_SEND_TIMEOUT, &timeoutMs_, sizeof(timeoutMs_));
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeoutMs_, sizeof(timeoutMs_));
    
    // Send request
    if (!WinHttpSendRequest(request, NULL, 0, (LPVOID)body.c_str(), body.length(), body.length(), 0)) {
        lastError_ = "Failed to send request";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Receive response
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode != 200 && statusCode != 201) {
        lastError_ = "HTTP error: " + std::to_string(statusCode);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Read response body
    std::string respBody;
    char buffer[4096];
    DWORD bytesRead = 0;
    
    while (WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        respBody.append(buffer, bytesRead);
    }
    
    response = respBody;
    
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    
    return true;
#else
    lastError_ = "HTTP not supported on this platform";
    return false;
#endif
}
