#include "pch.h"
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Security.Credentials.h>
#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Networking::Sockets;
using namespace Windows::Security::Credentials;

std::vector<DeviceInformation> foundDevices;
winrt::hstring serverPort = L"50001";

void OnDeviceAdded(DeviceWatcher const& watcher, DeviceInformation const& deviceInfo)
{
    std::wcout << L"Device found: " << deviceInfo.Name().c_str() << std::endl;
    foundDevices.push_back(deviceInfo);
}

void OnDeviceRemoved(DeviceWatcher const& watcher, DeviceInformationUpdate const& deviceInfoUpdate)
{
    std::wcout << L"Device removed: " << deviceInfoUpdate.Id().c_str() << std::endl;
}

void OnDeviceUpdated(DeviceWatcher const& watcher, DeviceInformationUpdate const& deviceInfoUpdate)
{
    std::wcout << L"Device updated: " << deviceInfoUpdate.Id().c_str() << std::endl;
}

void OnEnumerationCompleted(DeviceWatcher const& watcher, winrt::Windows::Foundation::IInspectable const& deviceInfo)
{
    std::wcout << L"Device enumeration completed: " << std::endl;
}

void OnStopped(DeviceWatcher const& watcher, winrt::Windows::Foundation::IInspectable const& deviceInfo)
{
    std::wcout << L"DeviceWatcher stopped: " << std::endl;
}

void ConnectToDevice(DeviceInformation& info)
{
    DeviceInformationCustomPairing customPairingInfo = info.Pairing().Custom();

    customPairingInfo.PairingRequested([](DeviceInformationCustomPairing const& sender, DevicePairingRequestedEventArgs const& args) {
        PasswordCredential credential;
        credential.UserName(L"testUser");
        credential.Password(L"testPass1234");
        args.AcceptWithPasswordCredential(credential);
        });

    //customPairingInfo.PairAsync(DevicePairingKinds::ProvidePasswordCredential).get();

    return;

    bool gotDevice = false;
    IAsyncOperation<WiFiDirectDevice> task = WiFiDirectDevice::FromIdAsync(info.Id());
    task.wait_for(TimeSpan(5s));
    //task.Completed([&gotDevice](auto const& handler) {
    //    gotDevice = true;
    //    });

    //while (!gotDevice)
    //{
    //    std::this_thread::sleep_for(10ms);
    //}

    WiFiDirectDevice wfdDevice = task.GetResults();

    wfdDevice.ConnectionStatusChanged([](WiFiDirectDevice const& sender, winrt::Windows::Foundation::IInspectable const& args)
    {
        std::wcout << "Connection status changed: " << static_cast<int32_t>(sender.ConnectionStatus()) << std::endl;
    });

    auto endpointPairs = wfdDevice.GetConnectionEndpointPairs();

    std::wcout << "Connected to device with IP " << endpointPairs.GetAt(0).RemoteHostName().DisplayName().c_str() << " on port " << serverPort.c_str() << std::endl;

    std::cout << "Waiting for server to start listening...";

    // Wait for server to start listening on a socket
    std::this_thread::sleep_for(5s);

    std::cout << "Connecting to server..." << std::endl;

    StreamSocket clientSocket;
    clientSocket.ConnectAsync(endpointPairs.GetAt(0).RemoteHostName(), serverPort).get();

    std::cout << "Connected to server!" << std::endl;
}

int main()
{
    init_apartment();

    winrt::hstring deviceSelector = WiFiDirectDevice::GetDeviceSelector(WiFiDirectDeviceSelectorType::DeviceInterface);
    std::vector<winrt::hstring> requestProperties;
    requestProperties.push_back(L"System.Devices.WiFiDirect.InformationElements");

    DeviceWatcher deviceWatcher = DeviceInformation::CreateWatcher(deviceSelector, requestProperties);

    deviceWatcher.Added(OnDeviceAdded);
    deviceWatcher.Removed(OnDeviceRemoved);
    deviceWatcher.Updated(OnDeviceUpdated);
    deviceWatcher.EnumerationCompleted(OnEnumerationCompleted);
    deviceWatcher.Stopped(OnStopped);

    deviceWatcher.Start();

    std::wcout << L"Searching for WiFi Direct devices..." << std::endl;

    std::cout << "1 - List Devices" << std::endl;
    std::cout << "2 x - Connect to Device number x" << std::endl;

    while (true)
    {
        // Keep the application running to allow asynchronous operations to complete
        std::string input;
        std::getline(std::cin, input);

        if (input == "1")
        {
            for (auto& device : foundDevices)
            {
                std::wcout << device.Name().c_str() << std::endl;
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
                std::cout << "std::invalid_argument::what(): " << ex.what() << '\n';
                continue;
            }
            catch (std::out_of_range const& ex)
            {
                std::cout << "std::out_of_range::what(): " << ex.what() << '\n';
                continue;
            }

            std::wcout << "Connect to device number " << idx << " with name " << foundDevices[idx - 1].Name().c_str() << std::endl;
            ConnectToDevice(foundDevices[idx - 1]);
        }
        else if (input == "q" || input == "quit" || input == "e" || input == "exit")
        {
            break;
        }
    }


    deviceWatcher.Stop();

    return 0;
}