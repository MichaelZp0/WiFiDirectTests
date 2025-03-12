#include "pch.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <iostream>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

void OnConnectionRequested(WiFiDirectConnectionListener const &sender, WiFiDirectConnectionRequestedEventArgs const &connectionEventArgs)
{
    auto connectionRequest = connectionEventArgs.GetConnectionRequest();
    std::wcout << L"Connection requested from: " << connectionRequest.DeviceInformation().Name().c_str() << std::endl;

    WiFiDirectDevice wfdDevice = WiFiDirectDevice::FromIdAsync(connectionRequest.DeviceInformation().Id()).get();
    StreamSocketListener listener;
    listener.ConnectionReceived([](StreamSocketListener const& listener, StreamSocketListenerConnectionReceivedEventArgs const& args)
        {
            std::wcout << L"Connection received!" << std::endl;
            // Handle the incoming connection
        });

    listener.BindServiceNameAsync(L"50001").get();
    std::wcout << L"Listening for incoming connections on port 50001..." << std::endl;
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
            std::wcout << L"Advertisement status: " << static_cast<int32_t>(args.Status()) << std::endl;
        });

    // Add publisher IE
    WiFiDirectInformationElement IE;

    param::hstring IE_String = L"IE_String";

    DataWriter dataWriter;
    dataWriter.UnicodeEncoding(UnicodeEncoding::Utf8);
    dataWriter.ByteOrder(ByteOrder::LittleEndian);
    dataWriter.WriteUInt32(dataWriter.MeasureString(IE_String));
    dataWriter.WriteString(IE_String);
    IE.Value(dataWriter.DetachBuffer());

    const char* customOuiStr = "CustomOui_Txt";
    std::array<uint8_t, 13> customOuiStdArr{};
    for (int i = 0; i < customOuiStdArr.size(); ++i)
    {
        customOuiStdArr[i] = static_cast<uint8_t>(customOuiStr[i]);
    }

    DataWriter dataWriterOUI;
    dataWriterOUI.WriteBytes(customOuiStdArr);
    IE.Oui(dataWriterOUI.DetachBuffer());

    IE.OuiType(0xDD);

    publisher.Advertisement().InformationElements().Append(IE);

    WiFiDirectConnectionListener listener;
    listener.ConnectionRequested(OnConnectionRequested);

    publisher.Start();
    std::wcout << L"Started WiFi Direct advertisement..." << std::endl;

    // Keep the application running to allow asynchronous operations to complete
    std::string input;
    std::getline(std::cin, input);

    publisher.Stop();
    std::wcout << L"Stopped WiFi Direct advertisement." << std::endl;

    return 0;
}
