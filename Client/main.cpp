#include "pch.h"
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Security.Credentials.h>
#include <iostream>
#include <chrono>
#include <optional>
#include <sstream>

#include "winsockutils.h"
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
    std::stringstream serverIpStr;
    serverIpStr << serverIp.c_str();
	
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

    auto adapterInfos = winsockutils::GetAdapterInfos();
    if (!adapterInfos.has_value())
    {
        GlobalOutput::WriteLocked("Failed to get adapter info", true);
        co_return;
    }

    std::array<uint8_t, 4> serverIpArray = winsockutils::IpFromString(serverIp.c_str());
    std::optional<std::string> localAddr = winsockutils::GetOwnIpInMatchingAdapter(adapterInfos.value(), serverIpArray);
    std::optional<winsockutils::Error> openClientError = winsockutils::OpenClient(serverIpStr.str(), "localAddr", winSockPort);

    if (openClientError.has_value())
    {
        GlobalOutput::WriteLocked(
            "OpenServer failed: " +
            std::to_string(openClientError->code) +
            " - " +
            openClientError->message);
        co_return;
    }
    else
    {
        GlobalOutput::WriteLocked([&serverIp]() { std::wcout << L"Connected to server at" << serverIp.c_str() << " on port " << winSockPort << std::endl; });
    }
}

int main()
{
    init_apartment();

    // Initialize WinSock
    std::optional<winsockutils::Error> initError = winsockutils::InitializeWinSock();
    if (initError.has_value())
    {
        GlobalOutput::WriteLocked(
            "WSAStartup failed: " +
            std::to_string(initError->code) +
            " - " +
            initError->message);
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