// ClientWinSock.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <optional>
#include <string>

#include "winsockutils.h"

int main()
{
    // Initialize WinSock
    std::optional<winsockutils::WinSockUtilsError> initError = winsockutils::InitializeWinSock();
    if (initError.has_value())
    {
        std::cout << "WSAStartup failed: " << std::to_string(initError->errorCode) << " - " << initError->errorMessage << std::endl;
        return 1;
    }

    //std::optional<winsockutils::Error> openClientError = winsockutils::OpenClient("127.0.0.1", 50011);
    //std::optional<winsockutils::Error> openClientError = winsockutils::OpenClient("192.168.0.114", "192.168.0.177", 50011);
    winsockutils::ConnectionResult openClientResult = winsockutils::OpenClient("192.168.137.1", "192.168.137.247", 50051);
    if (!openClientResult.success)
    {
        std::cout << "OpenClient failed: " << std::to_string(openClientResult.error.errorCode) << " - " << openClientResult.error.errorMessage << std::endl;
        return 1;
    }

	winsockutils::CloseSocketAndCleanUp(openClientResult.socket.value());

    return 0;
}