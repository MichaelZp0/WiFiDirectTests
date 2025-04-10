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

    std::optional<winsockutils::Error> openServerError = winsockutils::OpenClient("127.0.0.1", 50011);
    if (openServerError.has_value())
    {
        std::cout << "OpenServer failed: " << std::to_string(openServerError->code) << " - " << openServerError->message << std::endl;
        return 1;
    }

    return 0;
}