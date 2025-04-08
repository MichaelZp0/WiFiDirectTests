#include "pch.h"
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Security.Credentials.h>
#include <iostream>
#include <chrono>
#include <optional>
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

using namespace std::chrono_literals;

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Networking::Sockets;
using namespace Windows::Security::Credentials;

std::shared_ptr<bool> shouldClose = std::make_shared<bool>(false);
std::vector<DeviceInformation> foundDevices;
std::optional<SocketReaderWriter> sockReadWrite;

void OnDeviceAdded(DeviceWatcher const& watcher, DeviceInformation const& deviceInfo)
{
    GlobalOutput::WriteLocked([&deviceInfo]() { std::wcout << L"Device found: " << deviceInfo.Name().c_str() << " - " << deviceInfo.Id().c_str() << std::endl; });
    foundDevices.push_back(deviceInfo);
}

void OnDeviceRemoved(DeviceWatcher const& watcher, DeviceInformationUpdate const& deviceInfoUpdate)
{
    GlobalOutput::WriteLocked([&deviceInfoUpdate]() { std::wcout << L"Device removed: " << deviceInfoUpdate.Id().c_str() << std::endl; });
}

void OnDeviceUpdated(DeviceWatcher const& watcher, DeviceInformationUpdate const& deviceInfoUpdate)
{
    GlobalOutput::WriteLocked([&deviceInfoUpdate]() { std::wcout << L"Device updated: " << deviceInfoUpdate.Id().c_str() << std::endl; });
}

void OnEnumerationCompleted(DeviceWatcher const& watcher, winrt::Windows::Foundation::IInspectable const& deviceInfo)
{
    GlobalOutput::WriteLocked([&deviceInfo]() { std::wcout << L"Device enumeration completed: " << std::endl; });
}

void OnStopped(DeviceWatcher const& watcher, winrt::Windows::Foundation::IInspectable const& deviceInfo)
{
    GlobalOutput::WriteLocked([&deviceInfo]() { std::wcout << L"DeviceWatcher stopped: " << std::endl; });
}

struct MyAdapterInfo
{
    std::string Name;
    std::string IPAddress;
    std::string IPMask;
};

std::optional<std::vector<MyAdapterInfo>> GetAdapterInfos()
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

std::array<uint8_t, 4> IpFromString(std::string ip)
{
    std::array<uint8_t, 4> serverIpArray = { 0, 0, 0, 0 };

    for (int i = ip.size() - 1, k = 0, u = 3; i >= 0; --i)
    {
        if (ip[i] == '.')
        {
            k = 0;
            --u;
            continue;
        }

        serverIpArray[u] += (ip[i] - '0') * std::pow(10, k);
        ++k;
    }

    return serverIpArray;
}

std::array<uint8_t, 4> IpFromString(std::wstring ip)
{
    std::array<uint8_t, 4> serverIpArray = { 0, 0, 0, 0 };

    for (int i = ip.size() - 1, k = 0, u = 3; i >= 0; --i)
    {
        if (ip[i] == L'.')
        {
            k = 0;
            --u;
            continue;
        }

        serverIpArray[u] += (ip[i] - L'0') * std::pow(10, k);
        ++k;
    }

    return serverIpArray;
}

std::array<uint8_t, 4> IpFromString(winrt::hstring ip)
{
    std::array<uint8_t, 4> serverIpArray = { 0, 0, 0, 0 };

    for (int i = ip.size() - 1, k = 0, u = 3; i >= 0; --i)
    {
        if (ip[i] == L'.')
        {
            k = 0;
            --u;
            continue;
        }

        serverIpArray[u] += (ip[i] - L'0') * std::pow(10, k);
        ++k;
    }

    return serverIpArray;
}

IAsyncAction ConnectToDevice(DeviceInformation& info)
{
    co_await Pairing::PairIfNeeded(info);

    WiFiDirectDevice wfdDevice = nullptr;
    try
    {
        wfdDevice = co_await WiFiDirectDevice::FromIdAsync(info.Id());
    }
    catch (hresult_error const& ex)
    {
        GlobalOutput::WriteLocked([&ex]() { std::wcout << L"Failed to create WiFiDirectDevice: " << ex.message().c_str() << std::endl; });
        co_return;
    }

    if (wfdDevice == nullptr)
    {
        GlobalOutput::WriteLocked(L"Failed to create WiFiDirectDevice: Device is null\n");
        co_return;
    }

    wfdDevice.ConnectionStatusChanged([](WiFiDirectDevice const& sender, winrt::Windows::Foundation::IInspectable const& args)
    {
        GlobalOutput::WriteLocked([&sender]() { std::wcout << "Connection status changed: " << static_cast<int32_t>(sender.ConnectionStatus()) << std::endl; });
    });

    auto endpointPairs = wfdDevice.GetConnectionEndpointPairs();
    if (endpointPairs.Size() == 0)
    {
        GlobalOutput::WriteLocked(L"No endpoint pairs found\n");
        co_return;
    }

    GlobalOutput::WriteLocked([&endpointPairs]() {
        std::wcout << "Connected to device with IP " << endpointPairs.GetAt(0).RemoteHostName().DisplayName().c_str() << " on port " << serverPort.c_str() << std::endl;
    });

    winrt::hstring serverIp = endpointPairs.GetAt(0).RemoteHostName().DisplayName();
	std::array<uint8_t, 4> serverIpArray = IpFromString(serverIp);

    GlobalOutput::WriteLocked("Waiting for server to start listening...\n");

    // Wait for server to start listening on a socket
    std::this_thread::sleep_for(5s);

    GlobalOutput::WriteLocked("Connecting to server...\n");

    StreamSocket clientSocket;
    try
    {
        co_await clientSocket.ConnectAsync(endpointPairs.GetAt(0).RemoteHostName(), serverPort);
    }
    catch (hresult_error const& ex)
    {
        GlobalOutput::WriteLocked([&ex]() { std::wcout << L"Failed to connect to server: " << ex.message().c_str() << std::endl; });
        co_return;
    }

    GlobalOutput::WriteLocked("Connected to server!\n");

    sockReadWrite = SocketReaderWriter(clientSocket, shouldClose);
    sockReadWrite->ReadMessage();
    sockReadWrite->WriteMessage(L"Server says hello.");

    GlobalOutput::WriteLocked("HELO done between server/client.\n");

    // Create a socket
    SOCKET ConnectSocket = INVALID_SOCKET;
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET) {
		GlobalOutput::WriteLocked("Error at socket(): " + std::to_string(WSAGetLastError()), true);
        WSACleanup();
        co_return;
    }

    std::string localAddr;
    auto adapterInfos = GetAdapterInfos();
	if (!adapterInfos.has_value())
	{
		GlobalOutput::WriteLocked("Failed to get adapter info", true);
		closesocket(ConnectSocket);
		WSACleanup();
		co_return;
	}

    for (auto adapterInfo : adapterInfos.value())
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
			serverIpMasked[i] = serverIpArray[i] & ipMask[i];

			if (myIpMasked[i] != serverIpMasked[i])
			{
				areTheSame = false;
                break;
			}
        }

		if (areTheSame)
		{
			localAddr = adapterInfo.IPAddress;
			break;
		}
    }

    // Bind the socket to a specific network interface
    sockaddr_in localAddress;
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = 0; // Any available port
    inet_pton(AF_INET, localAddr.c_str(), &localAddress.sin_addr); // Replace with your local interface IP

    if (bind(ConnectSocket, (sockaddr*)&localAddress, sizeof(localAddress)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(ConnectSocket);
        WSACleanup();
        co_return;
    }

    // Resolve the server address and port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(winSockPort);

	std::stringstream ipStream;
	ipStream << serverIpArray[0] << "." << serverIpArray[1] << "." << serverIpArray[2] << "." << serverIpArray[3];

    inet_pton(AF_INET, ipStream.str().c_str(), &serverAddress.sin_addr);

    // Connect to the server
    int iResult = connect(ConnectSocket, (sockaddr*)&serverAddress, sizeof(serverAddress));
    if (iResult == SOCKET_ERROR) {
		GlobalOutput::WriteLocked("Unable to connect to server: " + std::to_string(WSAGetLastError()), true);
        closesocket(ConnectSocket);
        WSACleanup();
        co_return;
    }

    GlobalOutput::WriteLocked([&serverIp]() { std::wcout << L"Connected to server at" << serverIp.c_str() << " on port " << winSockPort << std::endl; });

    // Clean up
    closesocket(ConnectSocket);
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

    winrt::hstring deviceSelector = WiFiDirectDevice::GetDeviceSelector(WiFiDirectDeviceSelectorType::AssociationEndpoint);
    std::vector<winrt::hstring> requestProperties;
    requestProperties.push_back(L"System.Devices.WiFiDirect.InformationElements");

    DeviceWatcher deviceWatcher = DeviceInformation::CreateWatcher(deviceSelector, requestProperties);

    deviceWatcher.Added(OnDeviceAdded);
    deviceWatcher.Removed(OnDeviceRemoved);
    deviceWatcher.Updated(OnDeviceUpdated);
    deviceWatcher.EnumerationCompleted(OnEnumerationCompleted);
    deviceWatcher.Stopped(OnStopped);

    deviceWatcher.Start();
    bool watcherIsStarted = true;

    GlobalOutput::WriteLocked(L"Searching for WiFi Direct devices...\n");

    GlobalOutput::WriteLocked("1 - List Devices\n");
    GlobalOutput::WriteLocked("2 x - Connect to Device number x\n");
    GlobalOutput::WriteLocked("3 [start|stop] - Start/stop the device watcher\n");
    GlobalOutput::WriteLocked("4 <Message> - Send a message to the server\n");

    while (true)
    {
        // Keep the application running to allow asynchronous operations to complete
        std::string input;
        std::getline(std::cin, input);

        if (input == "1")
        {
            int counter = 1;
            for (auto& device : foundDevices)
            {
                std::wcout << counter << " - " << device.Name().c_str() << std::endl;
                ++counter;
            }
        }
        else if (input._Starts_with("2"))
        {
            int idx;
            try
            {
                idx = std::stoi(input.substr(2, 1));
            }
            catch (std::invalid_argument const& ex)
            {
                GlobalOutput::WriteLocked([&ex]() { std::cout << "std::invalid_argument::what(): " << ex.what() << '\n'; });
                continue;
            }
            catch (std::out_of_range const& ex)
            {
                GlobalOutput::WriteLocked([&ex]() { std::cout << "std::out_of_range::what(): " << ex.what() << '\n'; });
                continue;
            }

            GlobalOutput::WriteLocked([&idx]() {
                std::wcout << "Connect to device number " << idx << " with name " << foundDevices[idx - 1].Name().c_str() << std::endl;
            });
            ConnectToDevice(foundDevices[idx - 1]);
        }
        else if (input._Starts_with("3"))
        {
            std::string command = input.substr(2);

            if (command == "stop")
            {
                if (watcherIsStarted)
                {
                    deviceWatcher.Stop();
                    watcherIsStarted = false;
                    GlobalOutput::WriteLocked("Stopped watcher\n");
                }
                else
                {
                    GlobalOutput::WriteLocked("Watcher was already stopped\n");
                }
            }
            else if (command == "start")
            {
                if (!watcherIsStarted)
                {
                    deviceWatcher.Start();
                    watcherIsStarted = true;
                    GlobalOutput::WriteLocked("Started watcher\n");
                }
                else
                {
                    GlobalOutput::WriteLocked("Watcher was already started\n");
                }
            }
            else
            {
                GlobalOutput::WriteLocked("Unknown argument. Please only use stop or start.\n");
            }
        }
        else if (input._Starts_with("4"))
        {
            std::string message = input.substr(2);
			if (sockReadWrite.has_value())
			{
				sockReadWrite->WriteMessage(winrt::to_hstring(message));
			}
			else
			{
				GlobalOutput::WriteLocked("Not connected to a server", true);
			}
        }
        else if (input == "q" || input == "quit" || input == "e" || input == "exit")
        {
            break;
        }
    }

    deviceWatcher.Stop();

    GlobalOutput::WriteLocked("Exiting");

	if (sockReadWrite.has_value())
	{
        *shouldClose = true;
		sockReadWrite->Close();
	}

    return 0;
}