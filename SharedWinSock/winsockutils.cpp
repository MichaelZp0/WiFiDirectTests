#include "pch.h"

#include "winsockutils.h"

#include <vector>
#include <sstream>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x)) 
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#include "globalOutput.h"

std::array<uint8_t, 4> winsockutils::IpFromString(std::string ip)
{
    std::array<uint8_t, 4> serverIpArray = { 0, 0, 0, 0 };

    for (size_t i = ip.size() - 1, k = 0, u = 3; i <= ip.size(); --i)
    {
        if (ip[i] == '.')
        {
            k = 0;
            --u;
            continue;
        }

        serverIpArray[u] += static_cast<uint8_t>((ip[i] - '0') * std::pow(10, k));
        ++k;
    }

    return serverIpArray;
}

std::array<uint8_t, 4> winsockutils::IpFromString(std::wstring ip)
{
    std::array<uint8_t, 4> serverIpArray = { 0, 0, 0, 0 };

    for (size_t i = ip.size() - 1, k = 0, u = 3; i <= ip.size(); --i)
    {
        if (ip[i] == L'.')
        {
            k = 0;
            --u;
            continue;
        }

        serverIpArray[u] += static_cast<uint8_t>((ip[i] - L'0') * std::pow(10, k));
        ++k;
    }

    return serverIpArray;
}

std::optional<std::vector<winsockutils::MyAdapterInfo>> winsockutils::GetAdapterInfos()
{
    std::vector<MyAdapterInfo> adapterInfos;

    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;

    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO*)MALLOC(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL)
    {
        printf("Error allocating memory needed to call GetAdaptersinfo\n");
        return std::nullopt;
    }

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
    {
        FREE(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)MALLOC(ulOutBufLen);
        if (pAdapterInfo == NULL)
        {
            printf("Error allocating memory needed to call GetAdaptersinfo\n");
            return std::nullopt;
        }
    }

    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
    {
        pAdapter = pAdapterInfo;
        while (pAdapter)
        {
            adapterInfos.push_back(MyAdapterInfo{
                pAdapter->AdapterName,
                pAdapter->IpAddressList.IpAddress.String,
                pAdapter->IpAddressList.IpMask.String
                });
            pAdapter = pAdapter->Next;
        }
    }
    else
    {
        GlobalOutput::WriteLocked("GetAdaptersInfo failed with error: " + std::to_string(dwRetVal), true);
    }

    if (pAdapterInfo)
    {
        FREE(pAdapterInfo);
    }

    return adapterInfos;
}

std::optional<std::string> winsockutils::GetOwnIpInMatchingAdapter(std::vector<MyAdapterInfo>& adapterInfos, std::array<uint8_t, 4> serverIp)
{
    for (auto adapterInfo : adapterInfos)
    {
        std::array<uint8_t, 4> ipMask = IpFromString(adapterInfo.IPMask);
        std::array<uint8_t, 4> myIpAddr = IpFromString(adapterInfo.IPAddress);

        if (ipMask[3] == 0 && ipMask[2] == 0 && ipMask[1] == 0 && ipMask[0] == 0)
        {
            continue;
        }

        if (myIpAddr[3] == 0 && myIpAddr[2] == 0 && myIpAddr[1] == 0 && myIpAddr[0] == 0)
        {
            continue;
        }

        std::array<uint8_t, 4> myIpMasked;
        std::array<uint8_t, 4> serverIpMasked;

        bool areTheSame = true;

        for (int i = 0; i < 4; ++i)
        {
            myIpMasked[i] = myIpAddr[i] & ipMask[i];
            serverIpMasked[i] = serverIp[i] & ipMask[i];

            if (myIpMasked[i] != serverIpMasked[i])
            {
                areTheSame = false;
                break;
            }
        }

        if (areTheSame)
        {
            return adapterInfo.IPAddress;
            break;
        }
    }

    return std::nullopt;
}

std::optional<winsockutils::Error> winsockutils::InitializeWinSock()
{
    // Initialize WinSock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        return Error(iResult, "Startup failed");
    }
    return std::nullopt;
}

std::optional<winsockutils::Error> winsockutils::OpenServer(std::string clientIp, uint32_t port)
{
    int iResult = 0;
    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    iResult = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &result);
    if (iResult != 0)
    {
        WSACleanup();
        return Error(iResult, "getaddrinfo failed");
    }

    // Create a socket
    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        WSACleanup();
        return Error(-1, "Error at socket(): " + std::to_string(WSAGetLastError()));
    }

    GlobalOutput::WriteLocked("Binding...", true);

    sockaddr_in* localAddress = (sockaddr_in*)result->ai_addr;
    inet_pton(AF_INET, clientIp.c_str(), &localAddress->sin_addr); // Replace with your local interface IP

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return Error(-1, "bind failed with error: " + std::to_string(WSAGetLastError()));
    }

    freeaddrinfo(result);

    GlobalOutput::WriteLocked("Start listening...", true);

    // Listen on the socket
    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(ListenSocket);
        WSACleanup();
        return Error(-1, "Listen failed with error: " + std::to_string(WSAGetLastError()));
    }

    SOCKET ClientSocket = INVALID_SOCKET;

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET)
    {
        closesocket(ListenSocket);
        WSACleanup();
        return Error(-1, "accept failed: " + std::to_string(WSAGetLastError()));
    }

    // No longer need server socket
    closesocket(ListenSocket);


    GlobalOutput::WriteLocked("Accepted connection!", true);

    // Clean up
    closesocket(ClientSocket);
    WSACleanup();

    return std::nullopt;
}

std::optional<winsockutils::Error> winsockutils::OpenClient(std::string serverIp, uint32_t port)
{
    // Create a socket
    SOCKET ConnectSocket = INVALID_SOCKET;
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET)
    {
        WSACleanup();
        return Error(-1, "Error at socket(): " + std::to_string(WSAGetLastError()));
    }

    // Bind the socket to a specific network interface
    sockaddr_in localAddress;
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = 0; // Any available port
    inet_pton(AF_INET, serverIp.c_str(), &localAddress.sin_addr); // Replace with your local interface IP

    if (bind(ConnectSocket, (sockaddr*)&localAddress, sizeof(localAddress)) == SOCKET_ERROR)
    {
        closesocket(ConnectSocket);
        WSACleanup();
        return Error(-1, "Bind failed: " + std::to_string(WSAGetLastError()));
    }

    // Resolve the server address and port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr);

    // Connect to the server
    int iResult = connect(ConnectSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
    if (iResult == SOCKET_ERROR)
    {
        closesocket(ConnectSocket);
        WSACleanup();
        return Error(-1, "Unable to connect to server: " + std::to_string(WSAGetLastError()));
    }

	GlobalOutput::WriteLocked("Connected to server!", true);

    // Clean up
    closesocket(ConnectSocket);
    WSACleanup();

    return std::nullopt;
}