#include "HttpServer.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

std::string HttpServer::sqlEscape(std::string s) {
    size_t pos = 0;
    while ((pos = s.find('\'', pos)) != std::string::npos) {
        s.replace(pos, 1, "''");
        pos += 2;
    }
    return s;
}

std::string HttpServer::urlEncode(const std::string& s) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out << static_cast<char>(c);
        } else if (c == ' ') {
            out << '+';
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return out.str();
}

bool HttpServer::hasColumn(const char* tableName, const char* columnName) {
    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM information_schema.COLUMNS "
        << "WHERE TABLE_SCHEMA = DATABASE() "
        << "AND TABLE_NAME = '" << tableName << "' "
        << "AND COLUMN_NAME = '" << columnName << "'";
    if (mysql_query(con_, sql.str().c_str()) != 0) return false;
    MYSQL_RES* cRes = mysql_store_result(con_);
    if (!cRes) return false;
    MYSQL_ROW row = mysql_fetch_row(cRes);
    bool ok = row && row[0] && atoi(row[0]) > 0;
    mysql_free_result(cRes);
    return ok;
}

std::optional<std::string> HttpServer::tryHandleStaticGet(const std::string& method, const std::string& route) {
    if (method != "GET") return std::nullopt;
    if (!(route.rfind("/static/", 0) == 0 || route.rfind("/uploads/", 0) == 0)) return std::nullopt;

    const bool isStatic = route.rfind("/static/", 0) == 0;
    const std::string prefix = isStatic ? "/static/" : "/uploads/";
    std::string rel = route.substr(prefix.size());
    if (rel.empty() || rel.find("..") != std::string::npos || rel.find('\\') != std::string::npos) {
        return std::string("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }
    while (!rel.empty() && rel[0] == '/') rel.erase(rel.begin());
    std::string filePath = (isStatic ? "static/" : "uploads/") + rel;

    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        return std::string("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    auto contentType = [&](const std::string& p) {
        auto dot = p.find_last_of('.');
        std::string ext = (dot == std::string::npos) ? "" : p.substr(dot + 1);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == "png") return std::string("image/png");
        if (ext == "jpg" || ext == "jpeg") return std::string("image/jpeg");
        if (ext == "gif") return std::string("image/gif");
        if (ext == "webp") return std::string("image/webp");
        if (ext == "svg") return std::string("image/svg+xml");
        if (ext == "pdf") return std::string("application/pdf");
        if (ext == "txt") return std::string("text/plain; charset=utf-8");
        return std::string("application/octet-stream");
    };

    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\nContent-Type: " << contentType(filePath)
        << "\r\nContent-Length: " << data.size()
        << "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
    return out.str() + data;
}

