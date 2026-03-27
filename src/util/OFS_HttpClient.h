#pragma once

#include <string>
#include <functional>
#include <vector>

// Simple HTTP client using Windows WinHTTP
// Note: This header can be included without Windows.h, implementation uses it
#ifdef _WIN32

// Forward declare for header-only compatibility
struct winhttp_h;

class OFS_HttpClient
{
public:
    OFS_HttpClient() noexcept;
    ~OFS_HttpClient() noexcept;
    
    // HTTP methods
    bool Get(const std::string& url, std::string& response) noexcept;
    bool Post(const std::string& url, const std::string& body, std::string& response, const std::string& contentType = "application/json") noexcept;
    
    // Set timeout (in milliseconds)
    void SetTimeout(int timeoutMs) noexcept { timeoutMs_ = timeoutMs; }
    
    // Get last error
    const std::string& GetLastError() const noexcept { return lastError_; }
    
private:
    int timeoutMs_ = 5000;
    std::string lastError_;
};

#else

// Stub for non-Windows
class OFS_HttpClient
{
public:
    OFS_HttpClient() noexcept {}
    ~OFS_HttpClient() noexcept {}
    
    bool Get(const std::string& url, std::string& response) noexcept { 
        lastError_ = "Not implemented";
        return false; 
    }
    bool Post(const std::string& url, const std::string& body, std::string& response, const std::string& contentType = "application/json") noexcept { 
        lastError_ = "Not implemented";
        return false; 
    }
    void SetTimeout(int timeoutMs) noexcept { timeoutMs_ = timeoutMs; }
    const std::string& GetLastError() const noexcept { return lastError_; }
    
private:
    int timeoutMs_ = 5000;
    std::string lastError_;
};

#endif
