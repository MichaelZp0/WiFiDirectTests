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

    winsockutils::ConnectionResult openServerResult = winsockutils::OpenServer("0.0.0.0", 50011);
    //winsockutils::ConnectionResult openServerResult = winsockutils::OpenServer("192.168.0.114", 50011);
    //winsockutils::ConnectionResult openServerResult = winsockutils::OpenServer("192.168.137.1", 50051);
    if (!openServerResult.success)
    {
        std::cout << "OpenServer failed: " << std::to_string(openServerResult.error.errorCode) << " - " << openServerResult.error.errorMessage << std::endl;
        return 1;
    }

	SOCKET serverSocket = openServerResult.socket.value();

    winsockutils::Recv(serverSocket); // Wait for "Hello from client"
    winsockutils::Send(serverSocket, "Hello from server");

    std::string input = "";

    do
    {
        winsockutils::Recv(serverSocket); // Wait for client message
        winsockutils::Send(serverSocket, "Got a message");
    } while (input != "q");

    winsockutils::CloseSocketAndCleanUp(serverSocket);

    return 0;
}