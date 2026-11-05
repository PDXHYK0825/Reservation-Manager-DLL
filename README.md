# 校园自习室预约管理系统

C++17、Qt 6.11.2 Widgets、Visual Studio 2022 / v143、Windows x64。保留登录、座位预约、本人签到和取消、违约登记与撤销、查询统计、JSON 保存和重启恢复功能。业务规则沿用当前实现：12 个固定座位，到期未签到自动补记违约。

## 功能概览

| 功能 | 说明 |
| --- | --- |
| 登录与会话 | 管理员和学生使用不同账号登录，支持退出后切换角色 |
| 座位查询 | 以 3x4 网格显示 12 个固定座位，并区分当前状态和所选时段是否可用 |
| 预约管理 | 创建、取消和查询预约，阻止座位冲突及同一学生的时间冲突 |
| 签到与违约 | 预约时段内签到；管理员登记或撤销违约；到期未签到自动登记违约 |
| 查询与统计 | 管理员可按日期、账号查询预约和违约，并查看四项汇总数据 |
| 数据持久化 | 使用 JSON 保存数据，程序重启后可恢复；写入失败不会留下半完成记录 |

## 快速开始

### 运行环境

- Windows 10/11 x64
- Visual Studio 2022，安装“使用 C++ 的桌面开发”
- Qt 6.11.2 `msvc2022_64`
- Qt VS Tools
- C++17、MSVC v143

### 1. 获取源码

```powershell
git clone https://github.com/PDXHYK0825/Reservation-Manager-DLL.git
cd Reservation-Manager-DLL
```

### 2. 配置 Qt 路径

仓库中的 `QtPaths.props` 保存 Qt SDK 的相对路径。若它与你的 Qt 安装位置不同，请编辑该文件，使 `QtSdkRoot` 指向实际的 `msvc2022_64` 目录。

也可以使用构建脚本传入 Qt 路径。脚本要求 Qt SDK 和项目位于同一盘符：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Build.ps1 `
    -QtRoot '..\..\Qt\QT6.11\6.11.2\msvc2022_64'
```

把示例路径替换成你电脑上 Qt 的实际相对路径。

### 3. 编译和运行

1. 用 Visual Studio 打开根目录的 `StudyRoomReservation.sln`。
2. 选择 `Debug | x64` 或 `Release | x64`。
3. 将 `StudyRoomReservation` 设为启动项目。
4. 按 `F5` 编译并启动。

根解决方案会先构建 `DataStatistics`、`Reservation`、`AttendanceViolation` 三个 DLL，再构建主程序和集成测试。

Release 程序输出位置：

```text
bin\StudyRoomReservation\x64\Release\StudyRoomReservation.exe
```

Debug 程序位于：

```text
bin\StudyRoomReservation\x64\Debug\StudyRoomReservation.exe
```

程序依赖的三个业务 DLL 为：

```text
DataStatistics.dll
Reservation.dll
AttendanceViolation.dll
```

发布或复制程序时，需要同时携带 EXE、上述三个 DLL、Qt 运行库和 `platforms` 插件目录。不要只复制 `StudyRoomReservation.exe`。

如果已经构建完成，也可以在 PowerShell 中为当前进程补充 Qt 和业务 DLL 的搜索路径后启动：

```powershell
$qtBin = (Resolve-Path '..\..\Qt\QT6.11\6.11.2\msvc2022_64\bin').Path
$appDir = (Resolve-Path '.\bin\StudyRoomReservation\x64\Release').Path
$env:PATH = "$qtBin;$appDir;$env:PATH"
& "$appDir\StudyRoomReservation.exe"
```

其中 Qt 路径需要替换为你电脑上的实际位置。

### 4. 一次构建并运行全部测试

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Build.ps1 `
    -QtRoot '..\..\Qt\QT6.11\6.11.2\msvc2022_64'
```

脚本默认构建 Debug 和 Release，并依次运行数据、预约、签到违约和集成测试。构建日志和测试报告写入 `verification` 目录。

只测试某一个模块时：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Build.ps1 `
    -QtRoot '..\..\Qt\QT6.11\6.11.2\msvc2022_64' `
    -Module Reservation `
    -Configuration Release
```

`-Module` 可选值为：

```text
DataStatistics
Reservation
AttendanceViolation
Application
```

## 登录账号

| 账号 | 密码 | 身份 |
| --- | --- | --- |
| `admin` | `123456` | 管理员 |
| `student01` | `123456` | 学生一 |
| `student02` | `123456` | 学生二 |

## 使用说明

### 学生端

1. 使用学生账号登录。
2. 在“座位预约”页面选择当天的开始时间和结束时间。
3. 查看座位网格中的“当前”和“所选时段”两个状态。
4. 选择空闲座位，点击“预约”。系统会再次校验时间、座位和本人预约冲突。
5. 到达预约开始时间后，可在预约操作区域点击“签到”。
6. 在“预约记录”页面查看本人预约、签到时间、预约状态和违约记录。
7. 开始前或使用中可取消预约；进入预约时段后也可以签到。

座位状态含义：

| 状态 | 含义 |
| --- | --- |
| 空闲 | 当前时间没有被有效预约占用 |
| 已预约 | 当前时间已有预约，但学生尚未签到 |
| 使用中 | 学生已在预约时段内完成签到 |
| 可预约 | 所选时段没有与该座位重叠的有效预约 |
| 已有预约 | 所选时段与已有预约重叠 |

### 管理员端

管理员登录后可以使用四个页面：

- “座位查询”：查看座位当前状态，并查询指定时段是否存在预约。
- “预约与签到查询”：按预约日期、学生账号查询预约和签到记录。
- “违约管理”：选择已经开始的预约登记违约，或选中违约记录执行撤销。
- “统计”：选择起止日期，查看预约总数、取消数、签到数和有效违约数。

### 预约规则

- 只允许预约当天 08:00-23:00 的时间。
- 开始和结束时间必须落在整点或半点。
- 单次预约至少 30 分钟，结束时间必须晚于开始时间。
- 开始时间不能早于当前时间。
- 不允许同一座位的时间重叠。
- 不允许同一学生同时拥有两个重叠预约。
- 前一个预约结束时间等于后一个预约开始时间时，允许首尾相接。
- 已取消和已终止的预约不会占用座位。

### 签到与违约规则

- 签到仅允许在 `[预约开始时间, 预约结束时间)` 内进行。
- 每次预约只能签到一次。
- 登记违约会终止仍然有效的预约。
- 撤销违约会保留违约记录，但不会恢复已经终止的预约。
- 预约到期仍未签到时，系统会自动登记违约。
- 学生只能查看本人的预约和违约，管理员可以查看全部记录。

## 数据保存与恢复

数据文件保存在程序 EXE 同目录：

```text
study_room_data.json
```

首次运行没有该文件属于正常情况，程序会使用空数据启动。关闭程序后重新运行，已有预约和违约记录会从该文件恢复。

如果需要从空数据重新测试，建议先备份而不是直接删除：

```powershell
$data = ".\bin\StudyRoomReservation\x64\Debug\study_room_data.json"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
if (Test-Path -LiteralPath $data) {
    Move-Item -LiteralPath $data -Destination "$data.$stamp.bak"
}
```

## 四人联合开发结构

每个工作包对应一名成员的职责，文件夹和输出库均按功能命名。源码中保留原有分工注释，原仓库 Git 历史继续保留。

| 工作包 | 源码目录 | 职责 | 交付 |
| --- | --- | --- | --- |
| 界面与座位 | `modules/Application` | 登录与会话、座位状态、主窗口及模块集成 | `StudyRoomReservation.exe` |
| 预约管理 | `modules/Reservation` | 创建/取消/查询预约、冲突校验、时间规则及预约页面 | `ReservationManager.h` 等公开头文件、`Reservation.lib/.dll` |
| 签到与违约 | `modules/AttendanceViolation` | 本人签到、违约登记/撤销/查询及页面 | 公开头文件、`AttendanceViolation.lib/.dll` |
| 数据与统计 | `modules/DataStatistics` | JSON 原子保存、数据校验、统计及页面、发布公共模型 | 公开头文件、`DataStatistics.lib/.dll` |

公共模型在 `modules/DataStatistics/shared/include/Models.h`；系统常量在同目录 `SystemConfig.h`，由界面与座位工作包维护。

依赖顺序为 **DataStatistics → Reservation → AttendanceViolation → Application**。数据统计模块只依赖 Qt；预约依赖数据统计；签到违约依赖预约的公共规则和数据统计；界面组装三个 DLL。

联合开发边界可以直接在工程中检查：

- 三个业务模块各有自己的 `.sln`、DLL 工程、独立测试工程和导出宏。
- 根解决方案汇总主程序、集成测试和三个独立 DLL，并通过解决方案依赖控制生成顺序；所有工程都只编译本模块的实现文件。
- 调用方通过 `commonlibdll/Use*.props` 使用公开头文件和导入库，运行时加载对应 DLL。
- 生成业务 DLL 后，将公开头文件发布到 `commonlibdll/<功能名>/include`，导入库和 DLL 分别输出到 `lib/x64/<配置>`、`bin/x64/<配置>`。
- 管理器共用一个 `JsonStore`。修改在副本上完成，保存成功后更新内存并通过 `dataChanged()` 刷新界面。

## 模块独立验收与交接

| 工作包 | 解决方案 | 独立测试 |
| --- | --- | --- |
| 数据与统计 | `modules/DataStatistics/DataStatistics.sln` | `DataStatisticsTests`：54 项 |
| 预约管理 | `modules/Reservation/Reservation.sln` | `ReservationTests`：52 项 |
| 签到与违约 | `modules/AttendanceViolation/AttendanceViolation.sln` | `AttendanceViolationTests`：42 项 |
| 界面与座位 | 根目录 `StudyRoomReservation.sln` | `IntegrationTests`：13 项 |

首次使用按上述依赖顺序生成。已有依赖接口包后，可单独验证一个工作包：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Build.ps1 -QtRoot '..\..\Qt\QT6.11\6.11.2\msvc2022_64' -Module Reservation -Configuration Release
```

交接时提供本模块的公开头文件、同一配置的 `.lib/.dll` 和测试结果；调用方无需取得实现 `.cpp`。公共接口或模型变更时，先重建提供方，再依次重建受影响模块，避免头文件与 DLL 不匹配。各模块统一 Qt/MSVC 版本、x64 和 Debug/Release，调用方不定义提供方的导出宏。

## 常见问题

### 提示缺少 `DataStatistics.dll`、`Reservation.dll` 或 `AttendanceViolation.dll`

重新生成根解决方案，不要只生成主程序。主程序和三个业务 DLL 必须位于同一运行目录。`DataStatistics`、`Reservation` 和 `AttendanceViolation` 都是动态库，不能直接作为启动项目运行。

### 提示缺少 `Qt6Widgets.dll` 或找不到 `windows` 平台插件

确认 Qt 路径配置正确，优先从 Visual Studio 启动程序或运行 `tools/Build.ps1`。直接双击 EXE 时，需要保证 Qt 的 `bin` 和 `plugins` 已正确部署。

### 无法创建预约

请检查：

- 是否选择了当天时间。
- 开始和结束时间是否为整点或半点。
- 是否选择了过去的开始时间。
- 是否跨过 23:00。
- 座位或本人是否已有重叠预约。

### 无法签到

签到时间范围是 `[预约开始时间, 预约结束时间)`。尚未开始、已经结束、已取消、已终止或已经签到的预约都不能重复签到。

### 数据文件损坏

程序在 JSON 格式错误时会拒绝进入业务界面，并且不会覆盖原文件。请先备份 `study_room_data.json`，再检查或恢复数据。


