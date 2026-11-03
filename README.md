# 校园自习室预约管理系统

C++17、Qt 6.11.2 Widgets、Visual Studio 2022 / v143、Windows x64。保留登录、座位预约、本人签到和取消、违约登记与撤销、查询统计、JSON 保存和重启恢复功能。业务规则沿用当前实现：12 个固定座位，到期未签到自动补记违约。

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

## 编译与运行

安装 VS2022 C++ 桌面开发工具及 Qt 6.11.2 `msvc2022_64`，确认 `QtPaths.props` 中的相对路径指向实际 Qt 安装目录。

直接用 VS 打开根目录 `StudyRoomReservation.sln`，选择 `Debug | x64`，将 `StudyRoomReservation` 设为启动项目，按 F5。根解决方案会先生成 DataStatistics、Reservation、AttendanceViolation 三个 DLL 并发布公共头文件，再生成主程序；首次使用也无需先手工生成依赖。

需要运行全部独立测试时，在本工程根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\Build.ps1 -QtRoot '..\..\Qt\QT6.11\6.11.2\msvc2022_64'
```

Qt 路径以当前命令所在目录为基准传入；脚本会换算为相对于 `QtPaths.props` 的路径，并使用 `$(MSBuildThisFileDirectory)` 定位。Qt SDK 与工程须在同一盘符，移动后重新传入实际相对路径。

脚本依次生成四个工作包，默认构建并测试 Debug、Release。构建会生成 `commonlibdll` 中的接口包和 `bin` 中的程序。属性表已为 VS 调试器配置 Qt DLL 搜索路径。

也可在工程根目录运行 Release 程序：

```powershell
$env:PATH = (Resolve-Path '..\..\Qt\QT6.11\6.11.2\msvc2022_64\bin').Path + ';' + $env:PATH
& .\bin\StudyRoomReservation\x64\Release\StudyRoomReservation.exe
```

账号：`admin`、`student01`、`student02`，密码均为 `123456`。程序目录下的 `study_room_data.json` 保存业务记录，首次运行没有该文件正常。

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


