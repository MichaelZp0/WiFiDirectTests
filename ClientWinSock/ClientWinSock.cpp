// ClientWinSock.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <optional>
#include <string>

#include "winsockutils.h"

int main()
{
    // Initialize WinSock
    std::optional<winsockutils::Error> initError = winsockutils::InitializeWinSock();
    if (initError.has_value())
    {
        std::cout << "WSAStartup failed: " << std::to_string(initError->code) << " - " << initError->message << std::endl;
        return 1;
    }

    //std::optional<winsockutils::Error> openClientError = winsockutils::OpenClient("127.0.0.1", 50011);
    std::optional<winsockutils::Error> openClientError = winsockutils::OpenClient("192.168.0.114", "192.168.0.177", 50011);
    if (openClientError.has_value())
    {
        std::cout << "OpenClient failed: " << std::to_string(openClientError->code) << " - " << openClientError->message << std::endl;
        return 1;
    }

    return 0;
}