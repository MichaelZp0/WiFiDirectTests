#include "pch.h"
#include "pairing.h"

#include "globalOutput.h"

#include <iostream>
#include <sstream>

#include <winrt/Windows.Devices.WiFiDirect.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Credentials.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Security::Credentials;
using namespace Windows::Devices::Enumeration;

namespace Pairing
{
    winrt::Windows::Foundation::IAsyncAction PairIfNeeded(
        winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo)
    {
        bool isPaired = (deviceInfo.Pairing().IsPaired()) ||
            co_await Pairing::IsAepPairedAsync(deviceInfo.Id());

        if (!isPaired)
        {
            co_await Pairing::RequestPairDeviceAsync(deviceInfo.Pairing());
        }
    }

    IAsyncOperation<bool> IsAepPairedAsync(winrt::hstring deviceId)
    {
        DeviceInformation devInfo = nullptr;
        try
        {
            devInfo = co_await DeviceInformation::CreateFromIdAsync(deviceId, single_threaded_vector<winrt::hstring>({ L"System.Devices.Aep.DeviceAddress" }));
        }
        catch (hresult_error const& ex)
        {
            GlobalOutput::WriteLocked([&ex]() { std::wcout << L"Failed to create device information: " << ex.message().c_str() << std::endl; });
            co_return false;
        }

        if (devInfo == nullptr)
        {
            GlobalOutput::WriteLocked(L"Failed to create device information: Device is null\n");
            co_return false;
        }

        std::optional<hstring> deviceAddress = devInfo.Properties().Lookup(L"System.Devices.Aep.DeviceAddress").try_as<hstring>();

        if (!deviceAddress.has_value())
        {
            GlobalOutput::WriteLocked(L"Failed to get device address", true);
            co_return false;
        }

        std::stringstream deviceSelectorStream;
        deviceSelectorStream << "System.Devices.Aep.DeviceAddress:=\"{devInfo.Properties[" << deviceAddress.value().c_str() << "\"";
        winrt::hstring deviceSelector = winrt::to_hstring(deviceSelectorStream.str());

        DeviceInformationCollection pairedDeviceCollection = co_await DeviceInformation::FindAllAsync(deviceSelector, nullptr, DeviceInformationKind::Device);

        co_return pairedDeviceCollection.Size() > 0;
    }

    IAsyncOperation<bool> RequestPairDeviceAsync(DeviceInformationPairing pairing)
    {
        WiFiDirectConnectionParameters connectionParams;

        //DevicePairingKinds devicePairingKinds = DevicePairingKinds::ConfirmOnly | DevicePairingKinds::DisplayPin | DevicePairingKinds::ProvidePin;
        DevicePairingKinds devicePairingKinds = DevicePairingKinds::ProvidePasswordCredential;
        //DevicePairingKinds devicePairingKinds = DevicePairingKinds::ProvideAddress;

        //connectionParams.PreferenceOrderedConfigurationMethods().Append(WiFiDirectConfigurationMethod::PushButton);
        //connectionParams.PreferenceOrderedConfigurationMethods().Append(WiFiDirectConfigurationMethod::ProvidePin);
        //connectionParams.PreferenceOrderedConfigurationMethods().Append(WiFiDirectConfigurationMethod::DisplayPin);

        connectionParams.PreferredPairingProcedure(WiFiDirectPairingProcedure::GroupOwnerNegotiation);

        DeviceInformationCustomPairing customPairing = pairing.Custom();

        customPairing.PairingRequested([](DeviceInformationCustomPairing const& sender, DevicePairingRequestedEventArgs const& args) {
            switch (args.PairingKind())
            {
            case DevicePairingKinds::ConfirmOnly:
                GlobalOutput::WriteLocked(L"Pairing kind: ConfirmOnly\n");
                args.Accept();
                break;
            case DevicePairingKinds::ProvidePin:
                GlobalOutput::WriteLocked(L"Pairing requested: Provide PIN\n");
                args.Accept(L"123456"); // Provide a PIN
                break;
            case DevicePairingKinds::DisplayPin:
                GlobalOutput::WriteLocked([&args]() { std::wcout << L"Pairing requested: Display PIN - " << args.Pin().c_str() << std::endl; });
                args.Accept();
                break;
            case DevicePairingKinds::ProvidePasswordCredential:
            {
                GlobalOutput::WriteLocked(L"Pairing kind: ProvidePasswordCredential\n");
                PasswordCredential credential;
                credential.UserName(L"testUser");
                credential.Password(L"testPass1234");
                args.AcceptWithPasswordCredential(credential);
                break;
            }
            default:
                GlobalOutput::WriteLocked(L"Pairing requested: Unknown method\n");
                args.Accept();
                break;
            }
            });

        DevicePairingResult result = co_await customPairing.PairAsync(devicePairingKinds, DevicePairingProtectionLevel::None, connectionParams);

        if (result.Status() == DevicePairingResultStatus::AlreadyPaired)
        {
            GlobalOutput::WriteLocked(L"Already paired\n");
        }
        else if (result.Status() == DevicePairingResultStatus::Paired)
        {
            GlobalOutput::WriteLocked(L"Pairing succeeded\n");
        }
        else
        {
            GlobalOutput::WriteLocked([&result]() { std::wcout << L"Pairing failed: " << static_cast<int>(result.Status()) << std::endl; });
            co_return false;
        }
        co_return true;
    }
}
