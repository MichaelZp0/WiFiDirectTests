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

    //winsockutils::ConnectionResult openClientResult = winsockutils::OpenClient("127.0.0.1", "127.0.0.1", 50011);
    //winsockutils::ConnectionResult openClientResult = winsockutils::OpenClient("192.168.0.114", "192.168.0.177", 50011);
    winsockutils::ConnectionResult openClientResult = winsockutils::OpenClient("192.168.137.247", "192.168.137.1", 50011);
    //winsockutils::ConnectionResult openClientResult = winsockutils::OpenClient("192.168.137.247", "192.168.137.1", 50011);
    //winsockutils::ConnectionResult openClientResult = winsockutils::OpenClient("192.168.0.126", "192.168.0.177", 50011);
    if (!openClientResult.success)
    {
        std::cout << "OpenClient failed: " << std::to_string(openClientResult.error.errorCode) << " - " << openClientResult.error.errorMessage << std::endl;
        return 1;
    }

	SOCKET clientSocket = openClientResult.socket.value();

	winsockutils::Send(clientSocket, "Hello from client");
	winsockutils::Recv(clientSocket); // Wait for "Hello from server"

    std::string input = "";

    do
    {
        std::cout << "Type message: ";
		std::getline(std::cin, input);
        
		if (input != "q" && input.size() > 0)
		{
			winsockutils::Send(clientSocket, input);
			winsockutils::Recv(clientSocket); // Wait for server ack on message
		}

    } while (input != "q");

    winsockutils::CloseSocketAndCleanUp(clientSocket);

    return 0;
}