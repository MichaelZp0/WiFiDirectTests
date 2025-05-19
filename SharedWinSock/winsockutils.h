#include "pch.h"

#include <array>
#include <string>
#include <optional>
#include <vector>
#include <WinSock2.h>

class winsockutils
{
public:

    struct MyAdapterInfo
    {
        std::string Name;
        std::string IPAddress;
        std::string IPMask;
    };

    struct WinSockUtilsError
    {
		int errorCode;
		std::string errorMessage;
		WinSockUtilsError(int errorCode, std::string errorMessage)
			: errorCode(errorCode), errorMessage(errorMessage) {
		}
    };

    struct ConnectionResult
    {
        bool success;
        std::optional<SOCKET> socket;
		WinSockUtilsError error;

        static ConnectionResult CreateError(int errorCode, std::string errorMessage) {
            return ConnectionResult(false, std::nullopt, errorCode, errorMessage);
        };

        static ConnectionResult CreateSuccess(SOCKET socket)
        {
            return ConnectionResult(true, socket, 0, "");
        };

    private:
        ConnectionResult(bool success, std::optional<SOCKET> socket, int errorCode, std::string errorMessage)
            : success(success), socket(socket), error(WinSockUtilsError(errorCode, errorMessage)) {
        }
    };

    static std::array<uint8_t, 4> IpFromString(std::string ip);
    static std::array<uint8_t, 4> IpFromString(std::wstring ip);

    static std::optional<std::vector<winsockutils::MyAdapterInfo>> GetAdapterInfos();
    static std::optional<std::string> GetOwnIpInMatchingAdapter(std::vector<MyAdapterInfo>& adapterInfos, std::array<uint8_t, 4> serverIp);

    static std::optional<WinSockUtilsError> InitializeWinSock();

    static ConnectionResult OpenServer(std::string serverIp, uint32_t port);
    static ConnectionResult OpenClient(std::string serverIp, std::string localIp, uint32_t port);

    static void CloseSocketAndCleanUp(SOCKET socket);

	static std::optional<WinSockUtilsError> Send(SOCKET socket, std::string message);
    static std::optional<WinSockUtilsError> Recv(SOCKET socket);
};