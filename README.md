# AntiSeewo

一个用于监控和终止某软件相关进程的Windows系统工具。

## 功能特性

- ⏰ **时间范围监控**：在指定时间段内自动终止目标进程
- 📁 **文件触发机制**：检测特定文件存在时激活进程终止功能
- 🎯 **进程管理**：终止指定进程及其进程树
- 👁️ **实时监控**：持续监控系统进程并在条件满足时采取行动
- 🔧 **配置化管理**：通过JSON配置文件灵活调整行为参数
- 🛡️ **管理员权限**：自动获取管理员权限以确保功能正常
- 📅 **计划任务**：自动注册为系统启动项
- 🖥️ **系统托盘**：提供系统托盘界面方便用户操作

## 系统要求

- Windows 7/8/10/11
- 管理员权限（程序会自动请求）
- Visual C++ Redistributable

## 构建说明

### 依赖项

- C++17 兼容编译器
- nlohmann/json 库（包含在 ThirdParty 目录中）
- Windows SDK

### 构建步骤

1. 克隆项目到本地
2. 使用支持C++17的IDE或编译器（如Visual Studio 2019+）打开项目
3. 确保包含必要的头文件和库文件
4. 编译生成可执行文件

## 配置说明

程序首次运行会在同目录下创建 `config.json` 配置文件，包含以下可配置项：

```json
{
    "timeRanges": [
        {
            "start": "12:10",
            "end": "13:40"
        },
        {
            "start": "17:20",
            "end": "18:30"
        }
    ],
    "appsToKill": [
        "SeewoCore.exe",
        "SeewoAbility.exe",
        "SeewoServiceAssistant.exe",
        "media_capture.exe",
        "screenCapture.exe",
        "rtcRemoteDesktop.exe"
    ],
    "fileNameToCheck": "antiseewo.txt",
    "monitorProcesses": [
        "screen_capture.exe",
        "rtcRemoteDesktop.exe",
        "notepad.exe"
    ],
    "targetProcesses": [
        "explorer.exe"
    ]
}
```

### 配置参数说明

- `timeRanges`: 触发进程终止的时间段列表
- `appsToKill`: 需要终止的进程名称列表
- `fileNameToCheck`: 触发文件检查的文件名
- `monitorProcesses`: 监控进程列表（当这些进程运行时触发终止操作）
- `targetProcesses`: 目标进程列表（被终止的进程）

## 使用方法

1. 首次运行程序会自动请求管理员权限
2. 程序会创建默认配置文件（如不存在）
3. 程序会注册为系统启动计划任务
4. 程序会最小化到系统托盘
5. 右键点击系统托盘图标可：
  - 编辑配置文件
  - 退出程序

## 注意事项

- 程序需要管理员权限才能正常终止系统进程(建议关闭UAC)
- 修改配置文件后无需重启程序
- 程序会自动在系统启动时运行
- 请谨慎修改目标进程列表，误操作可能导致系统不稳定

## 许可证

本项目采用开源许可证，具体信息请查看LICENSE文件。

## 免责声明

本工具仅供学习和研究使用，请勿用于非法用途。使用者应对自己的行为负责，作者不承担任何责任。
