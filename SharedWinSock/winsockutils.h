#include "pch.h"

#include <array>
#include <string>
#include <optional>
#include <vector>

class winsockutils
{
public:

    struct MyAdapterInfo
    {
        std::string Name;
        std::string IPAddress;
        std::string IPMask;
    };

    struct Error
    {
        int code;
        std::string message;

        Error(int c, std::string m) : code(c), message(m) {}
    };

    static std::array<uint8_t, 4> IpFromString(std::string ip);
    static std::array<uint8_t, 4> IpFromString(std::wstring ip);

    static std::optional<std::vector<winsockutils::MyAdapterInfo>> GetAdapterInfos();
    static std::optional<std::string> GetOwnIpInMatchingAdapter(std::vector<MyAdapterInfo>& adapterInfos, std::array<uint8_t, 4> serverIp);

    static std::optional<Error> InitializeWinSock();

    static std::optional<Error> OpenServer(std::string clientIp, uint32_t port);
    static std::optional<Error> OpenClient(std::string serverIp, uint32_t port);
};