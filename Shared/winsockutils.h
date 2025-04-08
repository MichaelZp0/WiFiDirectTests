#include "pch.h"

#include <array>
#include <string>
#include <vector>
#include <optional>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x)) 
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#include "constants.h"
#include "globalOutput.h"

namespace winsockutils
{
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

	std::optional<std::string> GetOwnIpInMatchingAdapter(std::vector<MyAdapterInfo>& adapterInfos, std::array<uint8_t, 4> serverIp)
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
}