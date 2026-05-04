#include "HttpServer.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cctype>
#include <random>

HttpServer::HttpServer(int port) : port_(port), listen_socket_(INVALID_SOCKET), con_(nullptr) {}

HttpServer::~HttpServer() {
    closeAll();
}

bool HttpServer::initWinsock() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

bool HttpServer::initDb() {
    con_ = mysql_init(nullptr);
    if (!con_) {
        std::cerr << "[MySQL] mysql_init failed" << std::endl;
        return false;
    }
    mysql_options(con_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    const char* host = std::getenv("CAMPUS_DB_HOST");
    const char* user = std::getenv("CAMPUS_DB_USER");
    const char* pw = std::getenv("CAMPUS_DB_PASS");
    const char* db = std::getenv("CAMPUS_DB_NAME");
    const char* port_s = std::getenv("CAMPUS_DB_PORT");

    const char* host_v = (host && *host) ? host : "localhost";
    const char* user_v = (user && *user) ? user : "root";
    const char* pw_v = (pw && *pw) ? pw : "123456";
    const char* db_v = (db && *db) ? db : "campus_trade";
    unsigned int port_v = 3306;
    if (port_s && *port_s) {
        long p = std::strtol(port_s, nullptr, 10);
        if (p > 0 && p <= 65535) port_v = static_cast<unsigned int>(p);
    }

    if (!mysql_real_connect(con_, host_v, user_v, pw_v, db_v, port_v, nullptr, 0)) {
        std::cerr << "[MySQL] connect failed: " << mysql_error(con_) << std::endl;
        return false;
    }
    mysql_set_character_set(con_, "utf8mb4");
    mysql_query(con_, "SET NAMES utf8mb4");
    // Ensure legacy databases also have timestamps for insert/update ordering.
    mysql_query(con_, "ALTER TABLE item ADD COLUMN IF NOT EXISTS created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP");
    mysql_query(con_, "ALTER TABLE item ADD COLUMN IF NOT EXISTS updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP");
    mysql_query(con_, "ALTER TABLE item MODIFY COLUMN updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
    mysql_query(con_, "CREATE OR REPLACE VIEW v_user_info AS SELECT user_id, user_name, phone FROM user");
    return true;
}

void HttpServer::closeAll() {
    if (con_) {
        mysql_close(con_);
        con_ = nullptr;
    }
    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string HttpServer::htmlPage(const std::string& title, const std::string& body) {
    std::ostringstream out;
    out << "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
        << "<title>" << title << "</title>"
        << "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.8/dist/css/bootstrap.min.css' rel='stylesheet' integrity='sha384-sRIl4kxILFvY47J16cr9ZwB07vP4J8+LH7qKQnuqkuIAvNWLzeN8tE5YBujZqJLB' crossorigin='anonymous'>"
        << "<link href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css' rel='stylesheet'>"
        << "<style>"
        << "body.app-shell{background:#fff!important;min-height:100vh;}"
        << ".app-card,.card{background-color:#fffbeb!important;border-color:#fde68a!important;}"
        << ".table thead th{background:#fef9c3!important;border-color:#fde68a!important;color:#713f12;}"
        << ".table-bordered{border-color:#fde68a!important;}"
        << ".table-striped>tbody>tr:nth-of-type(odd)>*{--bs-table-bg-type:#fffef5;}"
        << ".nav-pills .nav-link{color:#0369a1;border-radius:999px;}"
        << ".nav-pills .nav-link:hover{background:#e0f2fe;}"
        << ".nav-pills .nav-link.active{background:#0ea5e9!important;color:#fff!important;}"
        << ".page-heading{color:#0c4a6e;}"
        << "</style>"
        << "</head><body class='app-shell'>"
        << "<div class='container py-4'>"
        << "<h2 class='mb-3 page-heading'><i class='bi bi-bag-heart-fill text-warning me-2' aria-hidden='true'></i>" << title << "</h2>"
        << "<nav class='nav nav-pills flex-wrap gap-2 mb-4' aria-label='主导航'>"
        << "<a class='nav-link' href='/'><i class='bi bi-house-door me-1'></i>首页</a>"
        << "<a class='nav-link' href='/items'><i class='bi bi-shop-window me-1'></i>商品</a>"
        << "<a class='nav-link' href='/users'><i class='bi bi-people me-1'></i>用户</a>"
        << "<a class='nav-link' href='/orders'><i class='bi bi-receipt-cutoff me-1'></i>订单</a>"
        << "<a class='nav-link' href='/queries'><i class='bi bi-search me-1'></i>查询</a>"
        << "</nav>"
        << body
        << "</div>"
        << "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.8/dist/js/bootstrap.bundle.min.js' integrity='sha384-FKyoEForCGlyvwx9Hj09JcYn3nv7wiPVlz7YYwJrWVcXK/BmnVDxM+D2scQbITxI' crossorigin='anonymous'></script>"
        << "</body></html>";
    return out.str();
}

std::string HttpServer::redirect(const std::string& location, const std::string& extraHeaders) {
    std::ostringstream out;
    out << "HTTP/1.1 302 Found\r\nLocation: " << location << "\r\n";
    if (!extraHeaders.empty()) out << extraHeaders;
    out << "Content-Length: 0\r\nConnection: close\r\n\r\n";
    return out.str();
}

std::string HttpServer::htmlEscape(const std::string& s) {
    std::string r = s;
    size_t pos = 0;
    while ((pos = r.find("&", pos)) != std::string::npos) { r.replace(pos, 1, "&amp;"); pos += 5; }
    pos = 0;
    while ((pos = r.find("<", pos)) != std::string::npos) { r.replace(pos, 1, "&lt;"); pos += 4; }
    pos = 0;
    while ((pos = r.find(">", pos)) != std::string::npos) { r.replace(pos, 1, "&gt;"); pos += 4; }
    return r;
}

std::string HttpServer::urlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out.push_back(' ');
        else if (s[i] == '%' && i + 2 < s.size()) {
            int v = 0;
            std::istringstream is(s.substr(i + 1, 2));
            is >> std::hex >> v;
            out.push_back(static_cast<char>(v));
            i += 2;
        } else out.push_back(s[i]);
    }
    return out;
}

std::unordered_map<std::string, std::string> HttpServer::parseForm(const std::string& body) {
    std::unordered_map<std::string, std::string> m;
    std::stringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t p = pair.find('=');
        if (p == std::string::npos) continue;
        m[urlDecode(pair.substr(0, p))] = urlDecode(pair.substr(p + 1));
    }
    return m;
}

std::unordered_map<std::string, std::string> HttpServer::parseCookies(const std::string& cookieHeader) {
    std::unordered_map<std::string, std::string> out;
    std::stringstream ss(cookieHeader);
    std::string pair;
    while (std::getline(ss, pair, ';')) {
        size_t begin = pair.find_first_not_of(' ');
        if (begin == std::string::npos) continue;
        pair = pair.substr(begin);
        size_t p = pair.find('=');
        if (p == std::string::npos) continue;
        out[pair.substr(0, p)] = pair.substr(p + 1);
    }
    return out;
}

std::string HttpServer::makeSessionId() {
    static std::mt19937_64 rng(std::random_device{}());
    std::ostringstream oss;
    oss << std::hex << rng() << rng();
    return oss.str();
}

std::optional<HttpServer::SessionInfo> HttpServer::getSession(const std::unordered_map<std::string, std::string>& headers) {
    auto it = headers.find("cookie");
    if (it == headers.end()) return std::nullopt;
    auto cookies = parseCookies(it->second);
    auto sit = cookies.find("session_id");
    if (sit == cookies.end()) return std::nullopt;
    auto sess = sessions_.find(sit->second);
    if (sess == sessions_.end()) return std::nullopt;
    return sess->second;
}

std::string HttpServer::handleRequest(const std::string& method, const std::string& path, const std::string& body, const std::unordered_map<std::string, std::string>& headers) {
    auto fieldDisplayName = [](const std::string& field) {
        static const std::unordered_map<std::string, std::string> mapping = {
            {"user_id", "用户ID"},
            {"user_name", "用户名"},
            {"phone", "手机号"},
            {"item_id", "商品ID"},
            {"item_name", "商品名称"},
            {"category", "类别"},
            {"price", "价格"},
            {"seller_id", "卖家ID"},
            {"status", "状态"},
            {"order_id", "订单ID"},
            {"buyer_id", "买家ID"},
            {"order_date", "下单时间"},
            {"created_at", "创建时间"},
            {"updated_at", "更新时间"}
        };
        auto it = mapping.find(field);
        return it == mapping.end() ? field : it->second;
    };
    auto displayCellValue = [](const std::string& field, const std::string& value) {
        if (field == "category") {
            static const std::unordered_map<std::string, std::string> catMap = {
                {"life", "生活"},
                {"study", "学习"},
                {"sport", "运动"},
                {"digital", "数码"},
                {"other", "其他"}
            };
            auto it = catMap.find(value);
            return it == catMap.end() ? value : it->second;
        }
        if (field == "status") {
            if (value == "0") return std::string("未售出");
            if (value == "1") return std::string("已售出");
        }
        return value;
    };

    auto categoryOptions = [&](const std::string& selected) {
        const std::vector<std::string> categories = {"life", "study", "sport", "digital", "other"};
        std::ostringstream out;
        out << "<option value=''>全部</option>";
        for (const auto& c : categories) {
            out << "<option value='" << c << "' " << (selected == c ? "selected" : "") << ">" << c << "</option>";
        }
        return out.str();
    };

    std::string route = path;
    std::unordered_map<std::string, std::string> query;
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) {
        route = path.substr(0, qpos);
        query = parseForm(path.substr(qpos + 1));
    }

    if (method == "GET" && path == "/favicon.ico") {
        return "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }
    if (auto staticResp = tryHandleStaticGet(method, route)) return *staticResp;

    auto session = getSession(headers);
    auto isAdmin = [&]() { return session && session->role == "admin"; };
    auto isUser = [&]() { return session && session->role == "user"; };
    auto requireLogin = [&]() { return !session.has_value(); };
    auto forbidden = [&]() {
        std::string html = htmlPage("无权限", "<div class='alert alert-danger'>当前角色无此操作权限。</div>");
        std::ostringstream out;
        out << "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
            << "\r\nConnection: close\r\n\r\n" << html;
        return out.str();
    };

    if (method == "GET" && route == "/login") return handleLoginGet();
    if (method == "POST" && route == "/login") return handleLoginPost(body);
    if (method == "GET" && route == "/register") return handleRegisterGet(query);
    if (method == "POST" && route == "/register") return handleRegisterPost(body);

    if (method == "GET" && route == "/logout") {
        auto it = headers.find("cookie");
        if (it != headers.end()) {
            auto cookies = parseCookies(it->second);
            auto sit = cookies.find("session_id");
            if (sit != cookies.end()) sessions_.erase(sit->second);
        }
        return redirect("/login", "Set-Cookie: session_id=deleted; Path=/; Max-Age=0\r\n");
    }

    if (requireLogin()) return redirect("/login");

    if (method == "GET" && route == "/") {
        std::string roleName = isAdmin() ? "管理员" : "用户";
        std::string loginUser = session ? session->user_id : "";
        std::string b = "<div class='card'><div class='card-body'>"
                        "<p class='mb-2'>校园二手交易数据库系统（C++ 版本）。</p>"
                        "<p class='mb-2'>当前登录角色：" + roleName + "，账号：" + htmlEscape(loginUser) + "</p>"
                        "<a class='btn btn-outline-secondary btn-sm' href='/logout'>退出登录</a>"
                        "</div></div>";
        std::string html = htmlPage("首页", b);
        std::ostringstream res;
        res << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size() << "\r\nConnection: close\r\n\r\n" << html;
        return res.str();
    }

    if (method == "POST" && route == "/add_item") {
        if (!isUser()) return forbidden();
        auto f = parseForm(body);
        // Generate a new item_id and ensure uniqueness.
        std::string itemId;
        for (int attempt = 0; attempt < 30; ++attempt) {
            std::string sid = makeSessionId();
            itemId = "i" + sid.substr(0, 8);
            bool exists = false;
            std::ostringstream check;
            check << "SELECT COUNT(*) FROM item WHERE item_id='" << sqlEscape(itemId) << "'";
            if (mysql_query(con_, check.str().c_str()) == 0) {
                MYSQL_RES* r = mysql_store_result(con_);
                MYSQL_ROW row = r ? mysql_fetch_row(r) : nullptr;
                exists = row && row[0] && atoi(row[0]) > 0;
                if (r) mysql_free_result(r);
            }
            if (!exists) break;
            itemId.clear();
        }
        if (itemId.empty()) return redirect("/items");
        std::ostringstream sql;
        sql << "INSERT INTO item(item_id,item_name,category,price,seller_id,status) VALUES('"
            << sqlEscape(itemId) << "','" << sqlEscape(f["item_name"]) << "','" << sqlEscape(f["category"]) << "',"
            << f["price"] << ",'" << sqlEscape(session->user_id) << "',0)";
        mysql_query(con_, sql.str().c_str());
        return redirect("/items?new_item_id=" + urlEncode(itemId));
    }
    if (method == "POST" && route == "/update_item") {
        if (!isUser()) return forbidden();
        auto f = parseForm(body);
        std::ostringstream sql;
        sql << "UPDATE item SET item_name='" << sqlEscape(f["item_name"])
            << "', category='" << sqlEscape(f["category"])
            << "', price=" << f["price"]
            << " WHERE item_id='" << sqlEscape(f["item_id"])
            << "' AND seller_id='" << sqlEscape(session->user_id) << "'";
        mysql_query(con_, sql.str().c_str());
        return redirect("/items");
    }
    if (method == "POST" && route == "/delete_unsold") {
        if (!isUser()) return forbidden();
        auto f = parseForm(body);
        std::ostringstream sql;
        sql << "DELETE FROM item WHERE item_id='" << sqlEscape(f["item_id"])
            << "' AND status=0 AND seller_id='" << sqlEscape(session->user_id) << "'";
        mysql_query(con_, sql.str().c_str());
        return redirect("/items");
    }
    if (method == "POST" && route == "/buy") {
        if (!isUser()) return forbidden();
        auto f = parseForm(body);
        mysql_query(con_, "START TRANSACTION");
        std::ostringstream s1;
        s1 << "SELECT status FROM item WHERE item_id='" << f["item_id"] << "' FOR UPDATE";
        if (mysql_query(con_, s1.str().c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(con_);
            MYSQL_ROW row = res ? mysql_fetch_row(res) : nullptr;
            bool ok = row && atoi(row[0]) == 0;
            if (res) mysql_free_result(res);
            if (ok) {
                std::ostringstream s2, s3;
                s2 << "INSERT INTO orders(order_id,item_id,buyer_id,order_date) VALUES('" << f["order_id"] << "','" << f["item_id"] << "','" << sqlEscape(session->user_id) << "',NOW())";
                s3 << "UPDATE item SET status=1 WHERE item_id='" << f["item_id"] << "'";
                if (mysql_query(con_, s2.str().c_str()) == 0 && mysql_query(con_, s3.str().c_str()) == 0) mysql_query(con_, "COMMIT");
                else mysql_query(con_, "ROLLBACK");
            } else mysql_query(con_, "ROLLBACK");
        } else mysql_query(con_, "ROLLBACK");
        return redirect("/orders");
    }

    auto makeTablePage = [&](const std::string& title, const std::string& sql, const std::string& extraForms) {
        mysql_query(con_, sql.c_str());
        MYSQL_RES* res = mysql_store_result(con_);
        std::ostringstream b;
        b << extraForms << "<div class='table-responsive'><table class='table table-striped table-bordered align-middle'>";
        if (res) {
            MYSQL_FIELD* fields = mysql_fetch_fields(res);
            unsigned int n = mysql_num_fields(res);
            b << "<tr>";
            for (unsigned int i = 0; i < n; ++i) b << "<th>" << fieldDisplayName(fields[i].name) << "</th>";
            b << "</tr>";
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                b << "<tr>";
                for (unsigned int i = 0; i < n; ++i) {
                    std::string raw = row[i] ? row[i] : "";
                    b << "<td>" << htmlEscape(displayCellValue(fields[i].name, raw)) << "</td>";
                }
                b << "</tr>";
            }
            mysql_free_result(res);
        }
        b << "</table></div>";
        std::string html = htmlPage(title, b.str());
        std::ostringstream out;
        out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size() << "\r\nConnection: close\r\n\r\n" << html;
        return out.str();
    };

    if (method == "GET" && route == "/users") {
        if (!isAdmin()) return forbidden();
        return makeTablePage("用户信息库", "SELECT * FROM v_user_info ORDER BY user_id", "");
    }

    if (method == "GET" && route == "/items") return handleItemsGet(session, query);
    if (method == "GET" && route == "/item") return handleItemGet(query, session);

    if (method == "GET" && route == "/orders") {
        std::string forms =
            "<div class='card mb-3'><div class='card-body'>"
            "<h5>购买商品</h5><form class='row g-2' method='post' action='/buy'>"
            "<div class='col-md-3'><input class='form-control' name='order_id' placeholder='order_id'></div>"
            "<div class='col-md-3'><input class='form-control' name='item_id' placeholder='item_id'></div>"
            "<div class='col-md-3'><button class='btn btn-success w-100'>购买</button></div>"
            "</form></div></div>";
        if (isAdmin()) {
            return makeTablePage("订单", "SELECT * FROM orders ORDER BY order_id DESC", "<div class='alert alert-info'>管理员仅可查看订单信息。</div>");
        }
        std::string sql = "SELECT * FROM orders WHERE buyer_id='" + sqlEscape(session->user_id) + "' ORDER BY order_id DESC";
        return makeTablePage("订单（我的）", sql, forms);
    }

    if (method == "GET" && route == "/queries") {
        auto parseDoubleSafe = [](const std::string& s, double& v) {
            if (s.empty()) return false;
            try {
                size_t idx = 0;
                v = std::stod(s, &idx);
                return idx == s.size();
            } catch (...) {
                return false;
            }
        };
        auto parseIntSafe = [](const std::string& s, int& v) {
            if (s.empty()) return false;
            try {
                size_t idx = 0;
                v = std::stoi(s, &idx);
                return idx == s.size();
            } catch (...) {
                return false;
            }
        };

        std::string minPrice = query.count("min_price") ? query["min_price"] : "";
        std::string maxPrice = query.count("max_price") ? query["max_price"] : "";
        std::string category = query.count("category") ? query["category"] : "";
        std::string status = query.count("status") ? query["status"] : "";
        std::string sellerId = query.count("seller_id") ? query["seller_id"] : "";
        std::string keyword = query.count("keyword") ? query["keyword"] : "";

        std::vector<std::string> conditions;
        double minPriceNum = 0.0, maxPriceNum = 0.0;
        if (parseDoubleSafe(minPrice, minPriceNum)) {
            std::ostringstream p;
            p << std::fixed << std::setprecision(2) << minPriceNum;
            conditions.push_back("price >= " + p.str());
        }
        if (parseDoubleSafe(maxPrice, maxPriceNum)) {
            std::ostringstream p;
            p << std::fixed << std::setprecision(2) << maxPriceNum;
            conditions.push_back("price <= " + p.str());
        }
        if (!category.empty()) conditions.push_back("category = '" + sqlEscape(category) + "'");
        if (status == "0" || status == "1") conditions.push_back("status = " + status);
        if (!sellerId.empty()) conditions.push_back("seller_id = '" + sqlEscape(sellerId) + "'");
        if (!keyword.empty()) conditions.push_back("item_name LIKE '%" + sqlEscape(keyword) + "%'");

        std::string whereSql;
        if (!conditions.empty()) {
            whereSql = " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i) whereSql += " AND ";
                whereSql += conditions[i];
            }
        }

        std::string sortField = "item_id";
        if (hasColumn("item", "updated_at")) sortField = "updated_at";
        else if (hasColumn("item", "created_at")) sortField = "created_at";

        int page = 1;
        if (query.count("page")) parseIntSafe(query["page"], page);
        if (page < 1) page = 1;
        const int pageSize = 10;

        std::string countSql = "SELECT COUNT(*) FROM item" + whereSql;
        int total = 0;
        if (mysql_query(con_, countSql.c_str()) == 0) {
            MYSQL_RES* cntRes = mysql_store_result(con_);
            if (cntRes) {
                MYSQL_ROW row = mysql_fetch_row(cntRes);
                if (row && row[0]) total = atoi(row[0]);
                mysql_free_result(cntRes);
            }
        }
        int totalPages = (total + pageSize - 1) / pageSize;
        if (totalPages == 0) totalPages = 1;
        if (page > totalPages) page = totalPages;
        int offset = (page - 1) * pageSize;

        std::ostringstream listSql;
        listSql << "SELECT * FROM item" << whereSql << " ORDER BY " << sortField << " DESC LIMIT " << pageSize << " OFFSET " << offset;
        MYSQL_RES* res = nullptr;
        if (mysql_query(con_, listSql.str().c_str()) == 0) {
            res = mysql_store_result(con_);
        } else {
            // Fallback when time columns do not exist in legacy schema.
            std::ostringstream fallbackSql;
            fallbackSql << "SELECT * FROM item" << whereSql << " ORDER BY item_id DESC LIMIT " << pageSize << " OFFSET " << offset;
            if (mysql_query(con_, fallbackSql.str().c_str()) == 0) {
                res = mysql_store_result(con_);
            }
        }

        auto buildQueryUrl = [&](int toPage) {
            std::ostringstream u;
            u << "/queries?page=" << toPage
              << "&min_price=" << minPrice
              << "&max_price=" << maxPrice
              << "&category=" << category
              << "&status=" << status
              << "&seller_id=" << sellerId
              << "&keyword=" << keyword;
            return u.str();
        };

        std::ostringstream b;
        b << "<div class='card mb-3'><div class='card-body'>"
          << "<form class='row g-2' method='get' action='/queries'>"
          << "<div class='col-md-12'>"
          << "<button class='btn btn-outline-primary' type='button' data-bs-toggle='collapse' data-bs-target='#filterPanel'>过滤条件</button>"
          << "<a class='btn btn-outline-secondary ms-2' href='/queries'>清空条件</a>"
          << "</div>"
          << "<div class='collapse show mt-2' id='filterPanel'>"
          << "<div class='card card-body'>"
          << "<div class='row g-2'>"
          << "<div class='col-md-2'><label class='form-label mb-0'>价格下限</label><input class='form-control' name='min_price' value='" << htmlEscape(minPrice) << "'></div>"
          << "<div class='col-md-2'><label class='form-label mb-0'>价格上限</label><input class='form-control' name='max_price' value='" << htmlEscape(maxPrice) << "'></div>"
          << "<div class='col-md-2'><label class='form-label mb-0'>类别条件</label><select class='form-select' name='category'>" << categoryOptions(category) << "</select></div>"
          << "<div class='col-md-2'><label class='form-label mb-0'>售出状态条件</label>"
          << "<select class='form-select' name='status'>"
          << "<option value='' " << (status.empty() ? "selected" : "") << ">全部</option>"
          << "<option value='0' " << (status == "0" ? "selected" : "") << ">未售出</option>"
          << "<option value='1' " << (status == "1" ? "selected" : "") << ">已售出</option>"
          << "</select></div>"
          << "<div class='col-md-2'><label class='form-label mb-0'>卖家条件</label><input class='form-control' name='seller_id' value='" << htmlEscape(sellerId) << "' placeholder='如 u001'></div>"
          << "<div class='col-md-2'><label class='form-label mb-0'>名称关键词</label><input class='form-control' name='keyword' value='" << htmlEscape(keyword) << "' placeholder='模糊匹配'></div>"
          << "<div class='col-md-2'><input type='hidden' name='page' value='1'><button class='btn btn-primary w-100 mt-4'>查询</button></div>"
          << "</div></div></div></form></div></div>";

        b << "<div class='d-flex justify-content-between align-items-center mb-2'>"
          << "<div class='text-secondary'>共 " << total << " 条，当前第 " << page << "/" << totalPages
          << " 页；默认按最近更新时间倒序展示</div>"
          << "<div>";
        if (page > 1) b << "<a class='btn btn-sm btn-outline-secondary me-2' href='" << buildQueryUrl(page - 1) << "'>上一页</a>";
        if (page < totalPages) b << "<a class='btn btn-sm btn-outline-secondary' href='" << buildQueryUrl(page + 1) << "'>下一页</a>";
        b << "</div></div>";

        b << "<div class='table-responsive'><table class='table table-striped table-bordered align-middle'>";
        if (res) {
            MYSQL_FIELD* fields = mysql_fetch_fields(res);
            unsigned int n = mysql_num_fields(res);
            b << "<tr>";
            for (unsigned int i = 0; i < n; ++i) b << "<th>" << fieldDisplayName(fields[i].name) << "</th>";
            b << "</tr>";
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                b << "<tr>";
                for (unsigned int i = 0; i < n; ++i) {
                    std::string raw = row[i] ? row[i] : "";
                    std::string cell = htmlEscape(displayCellValue(fields[i].name, raw));
                    if (std::string(fields[i].name) == "item_id" && !raw.empty()) {
                        cell = "<a href='/item?item_id=" + htmlEscape(raw) + "'>" + cell + "</a>";
                    }
                    b << "<td>" << cell << "</td>";
                }
                b << "</tr>";
            }
            mysql_free_result(res);
        }
        b << "</table></div>";

        std::string html = htmlPage("查询", b.str());
        std::ostringstream out;
        out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size() << "\r\nConnection: close\r\n\r\n" << html;
        return out.str();
    }

    std::string html = htmlPage("404", "<p>页面不存在</p>");
    std::ostringstream out;
    out << "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size() << "\r\nConnection: close\r\n\r\n" << html;
    return out.str();
}

void HttpServer::handleClient(SOCKET client) {
    std::string req;
    char buf[8192];
    int n = recv(client, buf, sizeof(buf), 0);
    if (n <= 0) return;
    req.assign(buf, n);

    size_t line_end = req.find("\r\n");
    if (line_end == std::string::npos) return;
    std::string first = req.substr(0, line_end);
    std::istringstream fs(first);
    std::string method, path, version;
    fs >> method >> path >> version;

    size_t header_end = req.find("\r\n\r\n");
    std::unordered_map<std::string, std::string> headers;
    if (header_end != std::string::npos) {
        size_t pos = line_end + 2;
        while (pos < header_end) {
            size_t next = req.find("\r\n", pos);
            if (next == std::string::npos || next > header_end) break;
            std::string line = req.substr(pos, next - pos);
            size_t sep = line.find(':');
            if (sep != std::string::npos) {
                std::string key = line.substr(0, sep);
                for (char& c : key) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                size_t vbegin = line.find_first_not_of(' ', sep + 1);
                headers[key] = (vbegin == std::string::npos) ? "" : line.substr(vbegin);
            }
            pos = next + 2;
        }
    }
    std::string body;
    if (header_end != std::string::npos) body = req.substr(header_end + 4);

    std::string resp = handleRequest(method, path, body, headers);
    send(client, resp.c_str(), static_cast<int>(resp.size()), 0);
}

bool HttpServer::run() {
    if (!initWinsock()) {
        std::cerr << "[Net] initWinsock failed" << std::endl;
        return false;
    }
    if (!initDb()) return false;

    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        std::cerr << "[Net] socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Net] bind(0.0.0.0:" << port_ << ") failed: " << std::strerror(errno) << std::endl;
        return false;
    }
    if (listen(listen_socket_, 16) == SOCKET_ERROR) {
        std::cerr << "[Net] listen() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    std::cout << "HTTP server started: http://127.0.0.1:" << port_ << std::endl;
    while (true) {
        SOCKET client = accept(listen_socket_, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        handleClient(client);
        closesocket(client);
    }
}
