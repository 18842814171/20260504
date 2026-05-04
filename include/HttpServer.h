#pragma once
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
using SOCKET = int;
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET (-1)
#  endif
#  ifndef SOCKET_ERROR
#    define SOCKET_ERROR (-1)
#  endif
#  ifndef closesocket
#    define closesocket ::close
#  endif
#endif
#include <string>
#include <unordered_map>
#include <optional>
#ifdef _WIN32
#  include "mysql.h"
#else
#  include <mysql/mysql.h>
#endif

class HttpServer {
public:
    HttpServer(int port);
    ~HttpServer();
    bool run();

private:
    struct SessionInfo {
        std::string role;
        std::string user_id;
    };

    int port_;
    SOCKET listen_socket_;
    MYSQL* con_;
    std::unordered_map<std::string, SessionInfo> sessions_;

    bool initWinsock();
    bool initDb();
    void closeAll();

    void handleClient(SOCKET client);
    std::string handleRequest(const std::string& method, const std::string& path, const std::string& body, const std::unordered_map<std::string, std::string>& headers);
    std::string htmlPage(const std::string& title, const std::string& body);
    std::string redirect(const std::string& location, const std::string& extraHeaders = "");

    std::unordered_map<std::string, std::string> parseForm(const std::string& body);
    std::string urlDecode(const std::string& s);
    std::string htmlEscape(const std::string& s);
    std::unordered_map<std::string, std::string> parseCookies(const std::string& cookieHeader);
    std::string makeSessionId();
    std::optional<SessionInfo> getSession(const std::unordered_map<std::string, std::string>& headers);

    // Split long handlers into separate translation units.
    std::string sqlEscape(std::string s);
    std::string urlEncode(const std::string& s);
    bool hasColumn(const char* tableName, const char* columnName);

    std::optional<std::string> tryHandleStaticGet(const std::string& method, const std::string& route);
    std::string handleLoginGet();
    std::string handleLoginPost(const std::string& body);
    std::string handleRegisterGet(const std::unordered_map<std::string, std::string>& query);
    std::string handleRegisterPost(const std::string& body);
    std::string handleItemsGet(const std::optional<SessionInfo>& session, const std::unordered_map<std::string, std::string>& query);
    std::string handleItemGet(const std::unordered_map<std::string, std::string>& query, const std::optional<SessionInfo>& session);
};
