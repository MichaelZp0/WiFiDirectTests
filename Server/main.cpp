#include "pch.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Credentials.h>
#include <iostream>
#include <sstream>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x)) 
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#include "socketReaderWriter.h"
#include "constants.h"
#include "globalOutput.h"
#include "pairing.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Networking;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Security::Credentials;

using namespace std::chrono_literals;

StreamSocketListener listener;
std::thread listeningThread;
std::shared_ptr<bool> shouldClose = std::make_shared<bool>(false);
std::optional<SocketReaderWriter> sockReadWrite;

IAsyncAction OnConnectionRequested(WiFiDirectConnectionListener const &sender, WiFiDirectConnectionRequestedEventArgs const &connectionEventArgs)
{
    WiFiDirectConnectionRequest connectionRequest = connectionEventArgs.GetConnectionRequest();

    GlobalOutput::WriteLocked([&connectionRequest]() {
        std::wcout << L"Trying to pair with: " << connectionRequest.DeviceInformation().Name().c_str() << std::endl;
        });

    co_await Pairing::PairIfNeeded(connectionRequest.DeviceInformation());

    GlobalOutput::WriteLocked([&connectionRequest]() {
        std::wcout << L"Connection requested from: " << connectionRequest.DeviceInformation().Name().c_str() << std::endl;
    });

    DeviceInformation info = connectionRequest.DeviceInformation();

	GlobalOutput::WriteLocked("Trying to get WiFiDirectDevice from connection request...", true);
    WiFiDirectDevice wfdDevice = co_await WiFiDirectDevice::FromIdAsync(connectionRequest.DeviceInformation().Id());
	GlobalOutput::WriteLocked("Got WiFiDirectDevice from connection request!", true);

    if (wfdDevice == nullptr)
    {
	    GlobalOutput::WriteLocked("WiFiDirectDevice from connection request is null!", true);
        co_return;
    }
	

    wfdDevice.ConnectionStatusChanged([](WiFiDirectDevice const& sender, auto const& args) {
        uint32_t status = static_cast<uint32_t>(sender.ConnectionStatus());
        GlobalOutput::WriteLocked([&status]() { std::cout << "Connection status changed: " << status << "\n"; });
    });

    GlobalOutput::WriteLocked("Setting up listener...", true);

    GlobalOutput::WriteLocked("Getting endpoint pairs...", true);
    Collections::IVectorView<EndpointPair> endpointPairs = wfdDevice.GetConnectionEndpointPairs();

    listener = nullptr;
	listener = StreamSocketListener();

    bool connectionReceived = false;
    listener.ConnectionReceived([&connectionReceived](StreamSocketListener const& listener, StreamSocketListenerConnectionReceivedEventArgs const& args)
        {
            GlobalOutput::WriteLocked(L"Connection received!\n");
            connectionReceived = true;
            // Handle the incoming connection

            sockReadWrite = SocketReaderWriter(args.Socket(), shouldClose);
            sockReadWrite->WriteMessage(L"Client says hello.");
            sockReadWrite->ReadMessage();

            GlobalOutput::WriteLocked("HELO done between server/client.\n");

            while (!(*shouldClose))
            {
                sockReadWrite->ReadMessage();
            }
        });

    GlobalOutput::WriteLocked(endpointPairs.GetAt(0).LocalHostName().ToString().c_str(), true);
    co_await listener.BindEndpointAsync(endpointPairs.GetAt(0).LocalHostName(), serverPort);
    GlobalOutput::WriteLocked([]() { std::wcout << L"Listening for incoming connections on port " << serverPort.c_str() << " ..." << std::endl; });

    int iResult = 0;
    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    // Resolve the local address and port to be used by the server
    iResult = getaddrinfo(NULL, std::to_string(winSockPort).c_str(), &hints, &result);
    if (iResult != 0)
    {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        co_return;
    }

    // Create a socket
    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        GlobalOutput::WriteLocked("Error at socket(): " + std::to_string(WSAGetLastError()), true);
        WSACleanup();
        co_return;
    }

    GlobalOutput::WriteLocked("Binding...", true);

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR)
    {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        co_return;
    }

    freeaddrinfo(result);

    GlobalOutput::WriteLocked("Start listenion...", true);

    // Listen on the socket
    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("Listen failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        co_return;
    }

    SOCKET ClientSocket = INVALID_SOCKET;

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET)
    {
        printf("accept failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        co_return;
    }

    // No longer need server socket
    closesocket(ListenSocket);


    GlobalOutput::WriteLocked("Accepted connection!", true);

    // Clean up
    closesocket(ClientSocket);
    WSACleanup();
}

int main()
{
    init_apartment();

    // Initialize WinSock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    WiFiDirectAdvertisementPublisher publisher;

    // Set the preferred pairing procedure
    WiFiDirectConnectionParameters connectionParams;
    connectionParams.PreferredPairingProcedure(WiFiDirectPairingProcedure::GroupOwnerNegotiation);

    //publisher.Advertisement().IsAutonomousGroupOwnerEnabled(true);
    publisher.Advertisement().IsAutonomousGroupOwnerEnabled(false);

    //publisher.Advertisement().ListenStateDiscoverability(WiFiDirect::WiFiDirectAdvertisementListenStateDiscoverability::None);
    publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::Normal);
    //publisher.Advertisement().ListenStateDiscoverability(WiFiDirect::WiFiDirectAdvertisementListenStateDiscoverability::Intensive);

    publisher.StatusChanged([](WiFiDirectAdvertisementPublisher const& publisher, WiFiDirectAdvertisementPublisherStatusChangedEventArgs const& args)
        {
            GlobalOutput::WriteLocked([&args]() { std::wcout << L"Advertisement status: " << static_cast<int32_t>(args.Status()) << std::endl; });
        });

    // Add publisher IE
    //WiFiDirectInformationElement IE;

    //param::hstring IE_String = L"IE_String";

    //DataWriter dataWriter;
    //dataWriter.UnicodeEncoding(UnicodeEncoding::Utf8);
    //dataWriter.ByteOrder(ByteOrder::LittleEndian);
    //dataWriter.WriteUInt32(dataWriter.MeasureString(IE_String));
    //dataWriter.WriteString(IE_String);
    //IE.Value(dataWriter.DetachBuffer());

    //const char* customOuiStr = "CustomOui_Txt";
    //std::array<uint8_t, 13> customOuiStdArr{};
    //for (int i = 0; i < customOuiStdArr.size(); ++i)
    //{
    //    customOuiStdArr[i] = static_cast<uint8_t>(customOuiStr[i]);
    //}

    //DataWriter dataWriterOUI;
    //dataWriterOUI.WriteBytes(customOuiStdArr);
    //IE.Oui(dataWriterOUI.DetachBuffer());

    //IE.OuiType(0xDD);

    //publisher.Advertisement().InformationElements().Append(IE);

    WiFiDirectConnectionListener listener;
    listener.ConnectionRequested(OnConnectionRequested);

    publisher.Start();
    GlobalOutput::WriteLocked(L"Started WiFi Direct advertisement...\n");

    // Keep the application running to allow asynchronous operations to complete
    std::string input;
    std::getline(std::cin, input);

    publisher.Stop();
    GlobalOutput::WriteLocked(L"Stopped WiFi Direct advertisement.\n");

	if (sockReadWrite.has_value())
	{
		*shouldClose = true;
		sockReadWrite->Close();
        if (listeningThread.joinable()) {
            listeningThread.join();
        }
	}

    return 0;
}
