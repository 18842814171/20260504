#pragma once
#include "mysql.h"
#include <string>
#include <vector>

struct UserRow {
    std::string user_id;
    std::string user_name;
    std::string phone;
};

struct ItemRow {
    std::string item_id;
    std::string item_name;
    std::string category;
    double price;
    std::string seller_id;
    int status;
};

struct OrderRow {
    std::string order_id;
    std::string item_id;
    std::string buyer_id;
    std::string order_date;
};

class SecondHandDB {
public:
    SecondHandDB();
    ~SecondHandDB();

    bool isConnected() const;
    bool initSchema();

    std::vector<UserRow> listUsers();
    std::vector<ItemRow> listItems();
    std::vector<OrderRow> listOrders();

    bool addItem(const ItemRow& item);
    bool updateItemPrice(const std::string& item_id, double new_price);
    bool deleteUnsoldItem(const std::string& item_id);
    bool buyItem(const std::string& order_id, const std::string& item_id, const std::string& buyer_id);

private:
    bool exec(const std::string& sql);
    MYSQL* con;
    const char* host = "localhost";
    const char* user = "root";
    const char* pw = "123456";
    const char* database_name = "campus_trade";
    const int port = 3306;
};
