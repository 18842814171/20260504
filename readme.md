## 编译说明

### Windows（VS2022）

```powershell
rmdir /s /q build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

改成用 VS2022 运行：
- 在 VS 里重新生成 `campus_trade_web`
- 运行 `campus_trade_web`（不是 `campus_trade_db_demo`）
- 打开 <http://127.0.0.1:8080>

### Linux（GCC / Clang）

```bash
# 纯 g++（不经过 CMake）：编译并运行 web
make
```

### Makefile（推荐）

```bash
# 编译并运行 web
make

# 清理
make clean
```
运行
cd /home/linda/4.13
make
export VM_HOST=0.0.0.0
export VM_PORT=8001
./build/campus_trade_web

### 使用说明（Web）

- **注册普通用户**：打开 `http://127.0.0.1:8001/register`，填写 `user_name / phone` 提交；系统会自动生成并展示 `user_id`（用于登录）。
- **登录**：打开 `http://127.0.0.1:8001/login`
  - 管理员：`admin / admin123`
  - 普通用户：选择“用户”，只需填写 `user_id`（无需密码，前提是该 `user_id` 已存在，可通过注册创建）
- **商品详情**：打开 `http://127.0.0.1:8001/items`，点击列表里的 **商品ID** 进入 `http://127.0.0.1:8001/item?item_id=...`，可看到卖家手机号与“说明/图片”占位。
- **修改商品（统一在详情页）**
  - 在“我的商品”列表点击 **修改信息**，会跳转到详情页编辑模式；
  - 在“查询”页点击商品ID进入详情页后，若该商品属于当前登录用户，会出现 **修改** 按钮；
  - 只有商品发布者可编辑，管理员和其他用户都不能修改该商品。
- **如何放商品图片/文档（先用本地目录模拟上传）**
  - 把图片/文档放到项目目录：`/home/linda/4.13/uploads/`（或 `static/`）
  - 浏览器访问：`http://127.0.0.1:8001/uploads/文件名`（或 `/static/文件名`）
  - **给某个商品补充说明/图片（文件方式）**：在 `item_details/商品ID.txt` 里写：
    - `desc=这里写商品说明`
    - `image=/uploads/xxx.png`（或 `/static/xxx.png`）
  - 也可以走数据库方式：给 `item` 表新增 `item_desc` / `image_path` 字段并写入；详情页会自动读取


=============任务：按优先级排序=================

1. 增加创建用户功能（或者注册账号）✅
2. 在商品概要列表，点击某个商品，展示详细信息，包括商家联系方式和商品说明/图片 ✅（说明/图片占位 + 静态文件访问）