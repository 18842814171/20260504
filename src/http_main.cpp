#include "HttpServer.h"
#include <cstdlib>
#include <iostream>

int main() {
    const char* host_env = std::getenv("VM_HOST");
    if (!host_env || !*host_env) host_env = std::getenv("CAMPUS_HOST");
    const char* port_env = std::getenv("VM_PORT");
    if (!port_env || !*port_env) port_env = std::getenv("CAMPUS_PORT");

    const char* host = (host_env && *host_env) ? host_env : "0.0.0.0";
    int port = 8001;
    if (port_env && *port_env) {
        long p = std::strtol(port_env, nullptr, 10);
        if (p > 0 && p <= 65535) port = static_cast<int>(p);
    }

    std::cout << "Starting server on http://" << host << ":" << port << std::endl;
    std::cout << "Tip: export VM_HOST=0.0.0.0 VM_PORT=8001" << std::endl;

    HttpServer server(port);
    if (!server.run()) {
        std::cerr << "start server failed" << std::endl;
        return 1;
    }
    return 0;
}
