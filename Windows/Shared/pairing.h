#include "pch.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>

namespace Pairing
{
    winrt::Windows::Foundation::IAsyncAction PairIfNeeded(
        winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo);

    winrt::Windows::Foundation::IAsyncOperation<bool> IsAepPairedAsync(winrt::hstring deviceId);

    winrt::Windows::Foundation::IAsyncOperation<bool> RequestPairDeviceAsync(
        winrt::Windows::Devices::Enumeration::DeviceInformationPairing pairing);
}