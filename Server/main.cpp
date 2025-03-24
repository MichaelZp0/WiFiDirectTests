#include "pch.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Credentials.h>
#include <iostream>

#include "socketReaderWriter.h"
#include "constants.h"
#include "globalOutput.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Security::Credentials;

using namespace std::chrono_literals;

void OnConnectionRequested(WiFiDirectConnectionListener const &sender, WiFiDirectConnectionRequestedEventArgs const &connectionEventArgs)
{
    auto connectionRequest = connectionEventArgs.GetConnectionRequest();

    GlobalOutput::WriteLocked([&connectionRequest]() {
        std::wcout << L"Connection requested from: " << connectionRequest.DeviceInformation().Name().c_str() << std::endl;
    });

    DeviceInformation info = connectionRequest.DeviceInformation();
    
    DeviceInformationCustomPairing customPairingInfo = info.Pairing().Custom();

    customPairingInfo.PairingRequested([](DeviceInformationCustomPairing const& sender, DevicePairingRequestedEventArgs const& args) {
        PasswordCredential credential;
        credential.UserName(L"testUser");
        credential.Password(L"testPass1234");
        args.AcceptWithPasswordCredential(credential);
        });

    WiFiDirectDevice wfdDevice = WiFiDirectDevice::FromIdAsync(connectionRequest.DeviceInformation().Id()).get();

    if (wfdDevice == nullptr)
    {
        return;
    }

    wfdDevice.ConnectionStatusChanged([](auto const& sender, auto const& args) {
        GlobalOutput.WriteLocked("Connection status changed: " + sender->ConnectionStatus.ToString() + "\n");
    });

    StreamSocketListener listener;
    bool connectionReceived = false;
    listener.ConnectionReceived([&connectionReceived](StreamSocketListener const& listener, StreamSocketListenerConnectionReceivedEventArgs const& args)
        {
            GlobalOutput::WriteLocked(L"Connection received!\n");
            connectionReceived = true;
            // Handle the incoming connection

            SocketReaderWriter sockReadWrite(args.Socket());
            sockReadWrite.WriteMessage(L"Client says hello.");
            sockReadWrite.ReadMessage();

            GlobalOutput::WriteLocked("Messaging done! Disconnecting.\n");

            sockReadWrite.Close();
        });

    GlobalOutput::WriteLocked(wfdDevice.GetConnectionEndpointPairs().GetAt(0).LocalHostName().ToString().c_str());
    auto task = listener.BindEndpointAsync(wfdDevice.GetConnectionEndpointPairs().GetAt(0).LocalHostName(), serverPort);
    GlobalOutput::WriteLocked([]() { std::wcout << L"Listening for incoming connections on port " << serverPort.c_str() << " ..." << std::endl; });

    for (int i = 0; i < 40 && !connectionReceived; ++i)
    {
        std::this_thread::sleep_for(500ms);
        GlobalOutput::WriteLocked(static_cast<uint32_t>(task.Status()) + "");
    }
}

int main()
{
    init_apartment();

    WiFiDirectAdvertisementPublisher publisher;

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

    return 0;
}
