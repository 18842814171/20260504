CREATE DATABASE IF NOT EXISTS campus_trade CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE campus_trade;

DROP VIEW IF EXISTS v_unsold_items;
DROP VIEW IF EXISTS v_sold_items_with_buyer;
DROP VIEW IF EXISTS v_user_info;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS item;
DROP TABLE IF EXISTS user;

CREATE TABLE user (
    user_id VARCHAR(20) PRIMARY KEY,
    user_name VARCHAR(50) NOT NULL,
    phone VARCHAR(30) NOT NULL
);

CREATE TABLE item (
    item_id VARCHAR(20) PRIMARY KEY,
    item_name VARCHAR(100) NOT NULL,
    category VARCHAR(50) NOT NULL,
    price DECIMAL(10,2) NOT NULL CHECK (price >= 0),
    seller_id VARCHAR(20) NOT NULL,
    status TINYINT NOT NULL DEFAULT 0 CHECK (status IN (0, 1)),
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    CONSTRAINT fk_item_seller FOREIGN KEY (seller_id) REFERENCES user(user_id)
);

CREATE TABLE orders (
    order_id VARCHAR(20) PRIMARY KEY,
    item_id VARCHAR(20) NOT NULL UNIQUE,
    buyer_id VARCHAR(20) NOT NULL,
    order_date DATETIME NOT NULL,
    CONSTRAINT fk_order_item FOREIGN KEY (item_id) REFERENCES item(item_id),
    CONSTRAINT fk_order_buyer FOREIGN KEY (buyer_id) REFERENCES user(user_id)
);

INSERT INTO user (user_id, user_name, phone) VALUES
('u001', 'zhangsan', '13800000001'),
('u002', 'lisi', '13800000002'),
('u003', 'wangwu', '13800000003'),
('u004', 'zhaoliu', '13800000004');

INSERT INTO item (item_id, item_name, category, price, seller_id, status) VALUES
('i001', 'desk_lamp', 'life', 35.00, 'u001', 0),
('i002', 'db_book', 'study', 45.00, 'u001', 1),
('i003', 'badminton_racket', 'sport', 80.00, 'u002', 0),
('i004', 'thermos', 'life', 28.00, 'u003', 1),
('i005', 'calculator', 'study', 22.00, 'u004', 0);

INSERT INTO orders (order_id, item_id, buyer_id, order_date) VALUES
('o001', 'i002', 'u003', '2026-04-20 10:00:00'),
('o002', 'i004', 'u001', '2026-04-21 14:30:00');

CREATE OR REPLACE VIEW v_sold_items_with_buyer AS
SELECT i.item_name, o.buyer_id
FROM item i
JOIN orders o ON i.item_id = o.item_id
WHERE i.status = 1;

CREATE OR REPLACE VIEW v_user_info AS
SELECT user_id, user_name, phone
FROM user;

CREATE OR REPLACE VIEW v_unsold_items AS
SELECT item_id, item_name, category, price, seller_id
FROM item
WHERE status = 0;
