﻿#include "pch.h"

// WinRT includes
// If you ever find weird link issues, it might be that you didn't include the specific WinRT header file
// Everything will show up fine, as I think most things are defined in some base headers, but some impls will just be missed
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

#include "socketReaderWriter.h"
#include "constants.h"
#include "globalOutput.h"
#include "pairing.h"
#include "winsockutils.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Networking;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Security::Credentials;

using namespace std::chrono_literals;

StreamSocketListener listener; // This object has to live until the end of the program otherwise things will break apart, thus keep it global
std::thread listeningThread;
std::shared_ptr<bool> shouldClose = std::make_shared<bool>(false);
std::optional<SocketReaderWriter> sockReadWrite;

IAsyncAction OnConnectionRequested(WiFiDirectConnectionListener const &sender, WiFiDirectConnectionRequestedEventArgs const &connectionEventArgs)
{
    WiFiDirectConnectionRequest connectionRequest = connectionEventArgs.GetConnectionRequest();

    GlobalOutput::WriteLocked([&connectionRequest]() {
        std::wcout << L"Trying to pair with: " << connectionRequest.DeviceInformation().Name().c_str() << std::endl;
        });

    // Pairing is pretty similar between server and client, thus it is done in shared code
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

    winrt::hstring serverIp = endpointPairs.GetAt(0).LocalHostName().DisplayName();

    winsockutils::ConnectionResult openServerResult = winsockutils::OpenServer(winrt::to_string(serverIp), winSockPort);

    if (!openServerResult.success)
    {
        GlobalOutput::WriteLocked(
            "OpenServer failed: " +
            std::to_string(openServerResult.error.errorCode) +
            " - " +
            openServerResult.error.errorMessage);
        co_return;
    }
    else
    {
        GlobalOutput::WriteLocked("OpenServer succeeded");
        winsockutils::CloseSocketAndCleanUp(openServerResult.socket.value());
    }
}

int main()
{
    init_apartment();

    std::optional<winsockutils::WinSockUtilsError> initError = winsockutils::InitializeWinSock();
    if (initError.has_value())
    {
		GlobalOutput::WriteLocked(
            "WSAStartup failed: " +
            std::to_string(initError->errorCode) +
            " - " +
            initError->errorMessage);
        return 1;
    }

    WiFiDirectAdvertisementPublisher publisher;

    // Set the preferred pairing procedure
    WiFiDirectConnectionParameters connectionParams;
    connectionParams.PreferredPairingProcedure(WiFiDirectPairingProcedure::GroupOwnerNegotiation); // We want to be the group owner, so we don't want Invitation mode
	connectionParams.GroupOwnerIntent(15); // Set the highst intent to be the group owner (0 - 15)
	connectionParams.PreferenceOrderedConfigurationMethods().Clear(); // We only want PushButton as this allows for the most seamless operation
    connectionParams.PreferenceOrderedConfigurationMethods().Append(WiFiDirectConfigurationMethod::PushButton);

    // Legacy settings for testing purpose. Should not be used in the optimal case
    //publisher.Advertisement().LegacySettings().Ssid(L"DIRECT-MrLinkNetwork");
    publisher.Advertisement().LegacySettings().Ssid(L"DIRECT-Meta-9x8f");
    PasswordCredential credential;
    //credential.UserName(L"testUser");
    credential.Password(L"tempPass");
    publisher.Advertisement().LegacySettings().Passphrase(credential);
	publisher.Advertisement().LegacySettings().IsEnabled(true);

	// No idea what this does - copied from sample
    publisher.Advertisement().IsAutonomousGroupOwnerEnabled(true);
    //publisher.Advertisement().IsAutonomousGroupOwnerEnabled(false);

    // Also never investigated this - copied from sample
    //publisher.Advertisement().ListenStateDiscoverability(WiFiDirect::WiFiDirectAdvertisementListenStateDiscoverability::None);
    publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::Normal);
    //publisher.Advertisement().ListenStateDiscoverability(WiFiDirect::WiFiDirectAdvertisementListenStateDiscoverability::Intensive);

    publisher.StatusChanged([](WiFiDirectAdvertisementPublisher const& publisher, WiFiDirectAdvertisementPublisherStatusChangedEventArgs const& args)
        {
            GlobalOutput::WriteLocked([&args]() { std::wcout << L"Advertisement status: " << static_cast<int32_t>(args.Status()) << std::endl; });
        });

    // Add publisher IE if needed. Currently not, so leave empty
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

	// Setup the listener to handle incoming connections
    WiFiDirectConnectionListener listener;
    listener.ConnectionRequested(OnConnectionRequested);

    // Start advertising
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
