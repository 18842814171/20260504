#include "SecondHandDB.h"
#include <iostream>
#include <sstream>

SecondHandDB::SecondHandDB() {
    con = mysql_init(nullptr);
    if (!con) {
        return;
    }
    mysql_options(con, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (!mysql_real_connect(con, host, user, pw, database_name, port, nullptr, 0)) {
        con = nullptr;
    } else {
        mysql_set_character_set(con, "utf8mb4");
        mysql_query(con, "SET NAMES utf8mb4");
    }
}

SecondHandDB::~SecondHandDB() {
    if (con) {
        mysql_close(con);
    }
}

bool SecondHandDB::isConnected() const {
    return con != nullptr;
}

bool SecondHandDB::exec(const std::string& sql) {
    if (!con) {
        return false;
    }
    if (mysql_query(con, sql.c_str()) != 0) {
        std::cerr << "[MySQL] " << mysql_error(con) << std::endl;
        return false;
    }
    return true;
}

bool SecondHandDB::initSchema() {
    return true;
}

std::vector<UserRow> SecondHandDB::listUsers() {
    std::vector<UserRow> out;
    if (!exec("SELECT user_id, user_name, phone FROM user ORDER BY user_id")) {
        return out;
    }
    MYSQL_RES* res = mysql_store_result(con);
    if (!res) return out;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        out.push_back({row[0] ? row[0] : "", row[1] ? row[1] : "", row[2] ? row[2] : ""});
    }
    mysql_free_result(res);
    return out;
}

std::vector<ItemRow> SecondHandDB::listItems() {
    std::vector<ItemRow> out;
    if (!exec("SELECT item_id, item_name, category, price, seller_id, status FROM item ORDER BY item_id")) {
        return out;
    }
    MYSQL_RES* res = mysql_store_result(con);
    if (!res) return out;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        ItemRow t;
        t.item_id = row[0] ? row[0] : "";
        t.item_name = row[1] ? row[1] : "";
        t.category = row[2] ? row[2] : "";
        t.price = row[3] ? atof(row[3]) : 0.0;
        t.seller_id = row[4] ? row[4] : "";
        t.status = row[5] ? atoi(row[5]) : 0;
        out.push_back(t);
    }
    mysql_free_result(res);
    return out;
}

std::vector<OrderRow> SecondHandDB::listOrders() {
    std::vector<OrderRow> out;
    if (!exec("SELECT order_id, item_id, buyer_id, order_date FROM orders ORDER BY order_id")) {
        return out;
    }
    MYSQL_RES* res = mysql_store_result(con);
    if (!res) return out;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        out.push_back({row[0] ? row[0] : "", row[1] ? row[1] : "", row[2] ? row[2] : "", row[3] ? row[3] : ""});
    }
    mysql_free_result(res);
    return out;
}

bool SecondHandDB::addItem(const ItemRow& item) {
    std::ostringstream oss;
    oss << "INSERT INTO item(item_id,item_name,category,price,seller_id,status) VALUES('"
        << item.item_id << "','" << item.item_name << "','" << item.category << "',"
        << item.price << ",'" << item.seller_id << "',0)";
    return exec(oss.str());
}

bool SecondHandDB::updateItemPrice(const std::string& item_id, double new_price) {
    std::ostringstream oss;
    oss << "UPDATE item SET price=" << new_price << " WHERE item_id='" << item_id << "'";
    return exec(oss.str());
}

bool SecondHandDB::deleteUnsoldItem(const std::string& item_id) {
    std::ostringstream oss;
    oss << "DELETE FROM item WHERE item_id='" << item_id << "' AND status=0";
    return exec(oss.str());
}

bool SecondHandDB::buyItem(const std::string& order_id, const std::string& item_id, const std::string& buyer_id) {
    if (!con) return false;
    if (!exec("START TRANSACTION")) return false;

    std::ostringstream check_sql;
    check_sql << "SELECT status FROM item WHERE item_id='" << item_id << "' FOR UPDATE";
    if (mysql_query(con, check_sql.str().c_str()) != 0) {
        exec("ROLLBACK");
        return false;
    }
    MYSQL_RES* res = mysql_store_result(con);
    if (!res) {
        exec("ROLLBACK");
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row || atoi(row[0]) != 0) {
        mysql_free_result(res);
        exec("ROLLBACK");
        return false;
    }
    mysql_free_result(res);

    std::ostringstream insert_order;
    insert_order << "INSERT INTO orders(order_id,item_id,buyer_id,order_date) VALUES('"
                 << order_id << "','" << item_id << "','" << buyer_id << "',NOW())";
    if (!exec(insert_order.str())) {
        exec("ROLLBACK");
        return false;
    }

    std::ostringstream update_item;
    update_item << "UPDATE item SET status=1 WHERE item_id='" << item_id << "'";
    if (!exec(update_item.str())) {
        exec("ROLLBACK");
        return false;
    }

    return exec("COMMIT");
}
