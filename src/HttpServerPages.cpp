#include "HttpServer.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

std::string HttpServer::handleLoginGet() {
    std::string b =
        "<div class='card'><div class='card-body'>"
        "<form method='post' action='/login' class='row g-3'>"
        "<div class='col-md-3'><label class='form-label'>角色</label><select class='form-select' name='role'>"
        "<option value='user'>用户</option><option value='admin'>管理员</option></select></div>"
        "<div class='col-md-4'><label class='form-label'>账号</label><input class='form-control' name='user_id' placeholder='用户ID或admin'></div>"
        "<div class='col-md-3'><label class='form-label'>密码</label><input type='password' class='form-control' name='password' placeholder='管理员必填'></div>"
        "<div class='col-md-2 d-flex align-items-end'><button class='btn btn-primary w-100'>登录</button></div>"
        "</form>"
        "<div class='d-flex justify-content-between align-items-center mt-3'>"
        "<p class='text-secondary mb-0'>演示规则：管理员账号 admin / admin123；普通用户用 user_id 登录。</p>"
        "<a class='btn btn-outline-secondary btn-sm' href='/register'>注册账号</a>"
        "</div>"
        "</div></div>";
    std::string html = htmlPage("登录", b);
    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
        << "\r\nConnection: close\r\n\r\n" << html;
    return out.str();
}

std::string HttpServer::handleLoginPost(const std::string& body) {
    auto f = parseForm(body);
    std::string role = f.count("role") ? f["role"] : "user";
    std::string userId = f.count("user_id") ? f["user_id"] : "";
    std::string password = f.count("password") ? f["password"] : "";
    bool ok = false;
    if (role == "admin") {
        ok = (userId == "admin" && password == "admin123");
    } else {
        std::ostringstream sql;
        sql << "SELECT COUNT(*) FROM user WHERE user_id='" << sqlEscape(userId) << "'";
        if (mysql_query(con_, sql.str().c_str()) == 0) {
            MYSQL_RES* r = mysql_store_result(con_);
            MYSQL_ROW row = r ? mysql_fetch_row(r) : nullptr;
            ok = row && row[0] && atoi(row[0]) > 0;
            if (r) mysql_free_result(r);
        }
    }
    if (!ok) return redirect("/login");
    std::string sid = makeSessionId();
    sessions_[sid] = SessionInfo{role, userId};
    return redirect("/", "Set-Cookie: session_id=" + sid + "; Path=/; HttpOnly\r\n");
}

std::string HttpServer::handleRegisterGet(const std::unordered_map<std::string, std::string>& query) {
    std::string err = query.count("err") ? query.at("err") : "";
    std::ostringstream b;
    if (!err.empty()) {
        b << "<div class='alert alert-danger'>" << htmlEscape(err) << "</div>";
    }
    b << "<div class='card'><div class='card-body'>"
      << "<form method='post' action='/register' class='row g-3'>"
      << "<div class='col-md-6'><label class='form-label'>用户名</label>"
      << "<input class='form-control' name='user_name' placeholder='如 张三' required></div>"
      << "<div class='col-md-6'><label class='form-label'>手机号</label>"
      << "<input class='form-control' name='phone' placeholder='11位手机号' required></div>"
      << "<div class='col-12 d-flex gap-2'>"
      << "<button class='btn btn-primary'>注册</button>"
      << "<a class='btn btn-outline-secondary' href='/login'>返回登录</a>"
      << "</div>"
      << "</form>"
      << "<p class='text-secondary mt-3 mb-0'>提示：注册成功后系统会自动生成 user_id（请记住它用于登录）。</p>"
      << "</div></div>";
    std::string html = htmlPage("注册账号", b.str());
    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
        << "\r\nConnection: close\r\n\r\n" << html;
    return out.str();
}

std::string HttpServer::handleRegisterPost(const std::string& body) {
    auto f = parseForm(body);
    std::string userName = f.count("user_name") ? f["user_name"] : "";
    std::string phone = f.count("phone") ? f["phone"] : "";

    auto isDigits = [](const std::string& s) {
        if (s.empty()) return false;
        for (unsigned char c : s) if (!std::isdigit(c)) return false;
        return true;
    };

    if (userName.empty() || userName.size() > 64) {
        return redirect("/register?err=" + urlEncode("用户名不能为空且不超过 64 个字符"));
    }
    if (!isDigits(phone) || phone.size() != 11) {
        return redirect("/register?err=" + urlEncode("手机号应为 11 位纯数字"));
    }

    // Generate a new user_id and ensure uniqueness.
    std::string userId;
    for (int attempt = 0; attempt < 30; ++attempt) {
        std::string sid = makeSessionId(); // hex random string
        userId = "u" + sid.substr(0, 8);
        bool exists = false;
        std::ostringstream sql;
        sql << "SELECT COUNT(*) FROM user WHERE user_id='" << sqlEscape(userId) << "'";
        if (mysql_query(con_, sql.str().c_str()) == 0) {
            MYSQL_RES* r = mysql_store_result(con_);
            MYSQL_ROW row = r ? mysql_fetch_row(r) : nullptr;
            exists = row && row[0] && atoi(row[0]) > 0;
            if (r) mysql_free_result(r);
        }
        if (!exists) break;
        userId.clear();
    }
    if (userId.empty()) return redirect("/register?err=" + urlEncode("生成用户ID失败，请重试"));

    std::ostringstream ins;
    ins << "INSERT INTO user(user_id,user_name,phone) VALUES('"
        << sqlEscape(userId) << "','" << sqlEscape(userName) << "','" << sqlEscape(phone) << "')";
    if (mysql_query(con_, ins.str().c_str()) != 0) {
        return redirect("/register?err=" + urlEncode("注册失败：请检查数据库连接与表结构"));
    }

    std::ostringstream b;
    b << "<div class='alert alert-success'>注册成功</div>"
      << "<div class='card'><div class='card-body'>"
      << "<p class='mb-2'>你的用户ID为：</p>"
      << "<div class='display-6'><code>" << htmlEscape(userId) << "</code></div>"
      << "<p class='text-secondary mt-3 mb-0'>请记住该 user_id，用于登录与发布商品。</p>"
      << "<div class='mt-3 d-flex gap-2'>"
      << "<a class='btn btn-primary' href='/login'>去登录</a>"
      << "<a class='btn btn-outline-secondary' href='/register'>继续注册</a>"
      << "</div>"
      << "</div></div>";
    std::string html = htmlPage("注册成功", b.str());
    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
        << "\r\nConnection: close\r\n\r\n" << html;
    return out.str();
}

std::string HttpServer::handleItemsGet(const std::optional<SessionInfo>& session, const std::unordered_map<std::string, std::string>& query) {
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
            {"updated_at", "更新时间"},
            {"item_desc", "商品说明"},
            {"image_path", "图片路径"}
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

    auto isAdmin = [&]() { return session && session->role == "admin"; };
    auto isUser = [&]() { return session && session->role == "user"; };

    auto makeItemListPage = [&](const std::string& title, const std::string& sql, const std::string& extraForms) {
        mysql_query(con_, sql.c_str());
        MYSQL_RES* res = mysql_store_result(con_);
        std::ostringstream b;
        b << extraForms;
        b << "<div class='table-responsive'><table class='table table-striped table-bordered align-middle'>";
        if (res) {
            MYSQL_FIELD* fields = mysql_fetch_fields(res);
            unsigned int n = mysql_num_fields(res);
            int itemIdIdx = -1;
            for (unsigned int i = 0; i < n; ++i) {
                if (std::string(fields[i].name) == "item_id") itemIdIdx = static_cast<int>(i);
            }
            b << "<tr>";
            for (unsigned int i = 0; i < n; ++i) b << "<th>" << fieldDisplayName(fields[i].name) << "</th>";
            if (isUser()) b << "<th>操作</th>";
            b << "</tr>";
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                std::string itemId = (itemIdIdx >= 0 && row[itemIdIdx]) ? row[itemIdIdx] : "";
                b << "<tr>";
                for (unsigned int i = 0; i < n; ++i) {
                    std::string raw = row[i] ? row[i] : "";
                    std::string cell = htmlEscape(displayCellValue(fields[i].name, raw));
                    if (std::string(fields[i].name) == "item_id" && !itemId.empty()) {
                        cell = "<a href='/item?item_id=" + htmlEscape(itemId) + "'>" + cell + "</a>";
                    }
                    b << "<td>" << cell << "</td>";
                }
                if (isUser()) {
                    b << "<td><a class='btn btn-sm btn-outline-primary' href='/item?item_id=" << htmlEscape(itemId) << "&edit=1'>修改信息</a></td>";
                }
                b << "</tr>";
            }
            mysql_free_result(res);
        }
        b << "</table></div>";
        std::string html = htmlPage(title, b.str());
        std::ostringstream out;
        out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
            << "\r\nConnection: close\r\n\r\n" << html;
        return out.str();
    };

    std::string forms;
    if (isUser()) {
        std::string newItemId = query.count("new_item_id") ? query.at("new_item_id") : "";

        std::ostringstream headerNotice;
        if (!newItemId.empty()) {
            headerNotice << "<div class='alert alert-success'>发布成功：系统生成商品ID = <code>"
                         << htmlEscape(newItemId) << "</code>（点击表格中的商品ID可查看详情）</div>";
        }

        forms =
            headerNotice.str() +
            "<div class='card mb-3'><div class='card-body'>"
            "<h5>新增商品</h5><form class='row g-2' method='post' action='/add_item'>"
            "<div class='col-md-3'><input class='form-control' name='item_name' placeholder='商品名称'></div>"
            "<div class='col-md-3'><select class='form-select' name='category'>" + categoryOptions("") + "</select></div>"
            "<div class='col-md-2'><input class='form-control' name='price' placeholder='价格'></div>"
            "<div class='col-md-2'><button class='btn btn-primary w-100'>发布（自动生成商品ID）</button></div>"
            "</form></div></div>"
            "<div class='card mb-3'><div class='card-body'>"
            "<h5>删除我的未售商品</h5><form class='row g-2' method='post' action='/delete_unsold'>"
            "<div class='col-md-10'><input class='form-control' name='item_id' placeholder='商品ID'></div>"
            "<div class='col-md-2'><button class='btn btn-danger w-100'>删除</button></div>"
            "</form></div></div>"
            "<div class='alert alert-secondary'>提示：点击列表中的 <b>商品ID</b> 可进入详情页查看卖家联系方式与图片/说明占位。</div>";
        std::string sql = "SELECT * FROM item WHERE seller_id='" + sqlEscape(session->user_id) + "' ORDER BY item_id DESC";
        return makeItemListPage("商品（我的）", sql, forms);
    }

    forms = isAdmin()
                ? "<div class='alert alert-info'>管理员仅可查看商品信息，不可修改。点击商品ID可看详情。</div>"
                : "<div class='alert alert-info'>点击商品ID可看详情。</div>";
    return makeItemListPage("商品（全部）", "SELECT * FROM item ORDER BY item_id DESC", forms);
}

std::string HttpServer::handleItemGet(const std::unordered_map<std::string, std::string>& query, const std::optional<SessionInfo>& session) {
    std::string itemId = query.count("item_id") ? query.at("item_id") : "";
    if (itemId.empty()) return redirect("/items");

    const bool hasDesc = hasColumn("item", "item_desc");
    const bool hasImg = hasColumn("item", "image_path");

    std::ostringstream sql;
    sql << "SELECT i.item_id,i.item_name,i.category,i.price,i.status,i.seller_id,"
        << "u.user_name,u.phone";
    if (hasDesc) sql << ",i.item_desc";
    if (hasImg) sql << ",i.image_path";
    sql << " FROM item i JOIN user u ON i.seller_id=u.user_id"
        << " WHERE i.item_id='" << sqlEscape(itemId) << "' LIMIT 1";

    if (mysql_query(con_, sql.str().c_str()) != 0) return redirect("/items");
    MYSQL_RES* res = mysql_store_result(con_);
    MYSQL_ROW row = res ? mysql_fetch_row(res) : nullptr;
    if (!row) {
        if (res) mysql_free_result(res);
        std::string html = htmlPage("商品详情", "<div class='alert alert-warning'>未找到该商品。</div><a class='btn btn-outline-secondary' href='/items'>返回列表</a>");
        std::ostringstream out;
        out << "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
            << "\r\nConnection: close\r\n\r\n" << html;
        return out.str();
    }

    int idx = 0;
    std::string v_item_id = row[idx++] ? row[idx - 1] : "";
    std::string v_item_name = row[idx++] ? row[idx - 1] : "";
    std::string v_category = row[idx++] ? row[idx - 1] : "";
    std::string v_price = row[idx++] ? row[idx - 1] : "";
    std::string v_status = row[idx++] ? row[idx - 1] : "";
    std::string v_seller_id = row[idx++] ? row[idx - 1] : "";
    std::string v_seller_name = row[idx++] ? row[idx - 1] : "";
    std::string v_phone = row[idx++] ? row[idx - 1] : "";
    std::string v_desc = hasDesc ? (row[idx++] ? row[idx - 1] : "") : "";
    std::string v_img = hasImg ? (row[idx++] ? row[idx - 1] : "") : "";
    mysql_free_result(res);

    // Optional: load details from file `item_details/<item_id>.txt`
    // Format example:
    //   desc=some text...
    //   image=/uploads/demo.png
    {
        std::ifstream in("item_details/" + itemId + ".txt");
        if (in) {
            std::string line;
            while (std::getline(in, line)) {
                if (line.rfind("desc=", 0) == 0 && v_desc.empty()) v_desc = line.substr(5);
                else if (line.rfind("image=", 0) == 0 && v_img.empty()) v_img = line.substr(6);
            }
        }
    }

    auto statusName = [&](const std::string& s) {
        if (s == "0") return std::string("未售出");
        if (s == "1") return std::string("已售出");
        return s;
    };
    auto categoryOptions = [&](const std::string& selected) {
        const std::vector<std::string> categories = {"生活", "学习", "运动", "数码", "其他"};
        std::ostringstream out;
        for (const auto& c : categories) {
            out << "<option value='" << c << "' " << (selected == c ? "selected" : "") << ">" << c << "</option>";
        }
        return out.str();
    };
    const bool canEdit = session && session->role == "user" && session->user_id == v_seller_id;
    const bool editMode = canEdit && query.count("edit") && query.at("edit") == "1";

    std::ostringstream b;
    b << "<div class='d-flex justify-content-between align-items-center mb-3'>"
      << "<a class='btn btn-outline-secondary btn-sm' href='/items'>返回列表</a>"
      << "<div class='text-secondary'>商品详情</div>";
    if (canEdit && !editMode) {
        b << "<a class='btn btn-warning btn-sm' href='/item?item_id=" << htmlEscape(v_item_id) << "&edit=1'>修改</a>";
    } else if (canEdit && editMode) {
        b << "<a class='btn btn-outline-secondary btn-sm' href='/item?item_id=" << htmlEscape(v_item_id) << "'>取消编辑</a>";
    }
    b << "</div>";

    b << "<div class='card mb-3'><div class='card-body'>"
      << "<h4 class='mb-2'>" << htmlEscape(v_item_name) << "</h4>"
      << "<div class='row g-2'>"
      << "<div class='col-md-3'><div class='text-secondary'>商品ID</div><div><code>" << htmlEscape(v_item_id) << "</code></div></div>"
      << "<div class='col-md-3'><div class='text-secondary'>类别</div><div>" << htmlEscape(v_category) << "</div></div>"
      << "<div class='col-md-3'><div class='text-secondary'>价格</div><div>" << htmlEscape(v_price) << "</div></div>"
      << "<div class='col-md-3'><div class='text-secondary'>状态</div><div>" << htmlEscape(statusName(v_status)) << "</div></div>"
      << "</div>"
      << "</div></div>";

    if (editMode) {
        b << "<div class='card mb-3 border-warning'><div class='card-body'>"
          << "<h5>修改商品信息</h5>"
          << "<form class='row g-2' method='post' action='/update_item'>"
          << "<input type='hidden' name='item_id' value='" << htmlEscape(v_item_id) << "'>"
          << "<div class='col-md-4'><input class='form-control' name='item_name' value='" << htmlEscape(v_item_name) << "' required></div>"
          << "<div class='col-md-3'><select class='form-select' name='category'>" << categoryOptions(v_category) << "</select></div>"
          << "<div class='col-md-3'><input class='form-control' name='price' value='" << htmlEscape(v_price) << "' required></div>"
          << "<div class='col-md-2'><button class='btn btn-warning w-100'>更新</button></div>"
          << "</form>"
          << "<div class='text-secondary mt-2'>仅商品发布者可修改，管理员也无修改权限。</div>"
          << "</div></div>";
    }

    b << "<div class='row g-3'>"
      << "<div class='col-lg-7'>"
      << "<div class='card h-100'><div class='card-body'>"
      << "<h5>商品说明</h5>";
    if (!v_desc.empty()) {
        b << "<div class='text-break'>" << htmlEscape(v_desc) << "</div>";
    } else {
        b << "<div class='alert alert-secondary mb-0'>暂无说明（当前先留空）。</div>"
          << "<div class='text-secondary mt-2'>如何补充：在 <code>item_details/" << htmlEscape(itemId) << ".txt</code> 写入 <code>desc=...</code>；或在数据库给 <code>item</code> 表新增 <code>item_desc</code> 字段并更新该商品。</div>";
    }
    b << "</div></div></div>";

    b << "<div class='col-lg-5'>"
      << "<div class='card mb-3'><div class='card-body'>"
      << "<h5>商品图片</h5>";
    if (!v_img.empty()) {
        b << "<img class='img-fluid rounded border' src='" << htmlEscape(v_img) << "' alt='item image'>";
    } else {
        b << "<div class='alert alert-secondary mb-0'>暂无图片（当前先留空）。</div>"
          << "<div class='text-secondary mt-2'>如何上传：把图片文件放到 <code>uploads/</code>（或 <code>static/</code>）里，然后在 <code>item_details/" << htmlEscape(itemId) << ".txt</code> 写入 <code>image=/uploads/文件名</code>（或 <code>/static/文件名</code>）。</div>";
    }
    b << "</div></div>";

    b << "<div class='card'><div class='card-body'>"
      << "<h5>商家信息</h5>"
      << "<div class='row g-2'>"
      << "<div class='col-6'><div class='text-secondary'>卖家ID</div><div>" << htmlEscape(v_seller_id) << "</div></div>"
      << "<div class='col-6'><div class='text-secondary'>卖家姓名</div><div>" << htmlEscape(v_seller_name) << "</div></div>"
      << "<div class='col-12'><div class='text-secondary'>联系方式</div><div><a href='tel:" << htmlEscape(v_phone) << "'>" << htmlEscape(v_phone) << "</a></div></div>"
      << "</div>"
      << "</div></div>"
      << "</div>"
      << "</div>";

    std::string html = htmlPage("商品详情", b.str());
    std::ostringstream out;
    out << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << html.size()
        << "\r\nConnection: close\r\n\r\n" << html;
    return out.str();
}

