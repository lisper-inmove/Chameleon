# Chameleon 界面设计与布局 — 设计文档

- 日期:2026-10-05
- 状态:设计已确认,待实施
- 关联 TODO:docs/TODO_List.md「界面设计与布局」章

## 背景与目标

Chameleon 是基于 Qt Quick + OpenCV 的 Windows 图片处理软件,当前 UI 为空白 ApplicationWindow。本轮目标:搭出完整的应用 UI 骨架,其中打开/显示/缩放/平移/标签页/主题等核心交互真实可用(QML 原生 Image 实现),依赖后续图像管线(C++/OpenCV)的功能(滤镜、直方图、格式转换、批处理)以占位面板呈现。

## 需求(已与用户确认)

1. 范围:完整 UI 骨架 + 可用核心(打开/保存/缩放/平移/标签页/主题用 QML 原生能力实现)
2. 主题:跟随系统(默认)+ 手动切换浅色/深色,Material 风格
3. 文案:直接写中文,暂不引入 i18n 框架
4. 标签页由左侧缩略图栏承担,不另做标签条
5. 状态栏本轮显示文件大小,内存占用待 C++ 管线接入后补

## 方案选择

选型结论:**方案 B —— 多文件 QML 组件 + QML 单例状态**。

- 弃用单文件方案(后期必拆,迁移成本高)
- 弃用 C++ 后端模型方案(图像管线未建,现在设计接口属返工风险)
- 状态集中在 AppState 单例,将来 OpenCV 管线接入时逐项下沉到 C++,QML 侧接口不变

## 文件结构

QML(位于 src/,QML 模块资源):

```
src/
├── Main.qml                 # 主窗口 shell:菜单栏、工具栏、布局组装
├── app/
│   └── AppState.qml         # pragma Singleton:文件列表、当前索引、主题、缩放状态
├── components/
│   ├── ImageViewer.qml      # 显示区:Image 显示、缩放/平移、空状态、加载状态
│   ├── ThumbnailBar.qml     # 左侧缩略图栏(与标签页联动,可折叠)
│   ├── FilterPanel.qml      # 右侧滤镜参数面板(控件骨架,禁用态)
│   ├── HistogramPanel.qml   # 直方图面板(Canvas 占位)
│   └── StatusBar.qml        # 底部状态栏(真实图片信息)
└── dialogs/
    └── AboutDialog.qml      # 关于对话框
```

C++(头文件进 include/ 并按二级目录组织,实现文件放 src/ 对应二级目录):

```
include/
├── config/
│   └── config_manager.h     # 从 src/ 迁入(现有代码随本轮一并调整)
└── utils/
    └── file_utils.h         # FileUtils::fileSizeOf,状态栏文件大小用

src/
├── main.cpp                 # 入口(保持不变)
├── config/
│   └── config_manager.cc    # 从 src/ 迁入
└── utils/
    └── file_utils.cc
```

- `include/` 作为 chameleon_lib 的 PUBLIC include 根,内部以 `#include "config/config_manager.h"`、`"utils/file_utils.h"` 引用;tests 经 chameleon_lib 传递获得 include 路径
- `FileUtils` 经 main.cpp 以 context property `fileUtils` 注入 QML

## 状态管理(AppState 单例)

| 状态 | 类型 | 说明 |
| --- | --- | --- |
| `images` | list<url> | 已打开文件列表(标签页 = 列表项) |
| `currentIndex` | int | 当前标签页索引 |
| `zoomMode` | enum | 适应窗口 / 实际大小 / 自定义 |
| `zoomFactor` | real | 自定义缩放倍率(滚轮/快捷键调整) |
| `theme` | enum | 跟随系统 / 浅色 / 深色 |
| `zoomByImage` | 每图独立 | 切换标签页不丢失缩放位置 |

持久化:
- `QtCore.Settings`(Qt 6.5+ 内置)保存 theme、最近打开文件(最多 8 条)
- 窗口几何由 configs/app.yaml 注入(现有机制),不重复持久化

职责边界:AppState 只存状态与枚举,不含 UI 代码。

## 布局与区域职责

```
┌────────────────────────────────────────────────────┐
│ 菜单栏:文件 编辑 视图 帮助                            │
│ 工具栏:打开 保存 撤销 重做 │ 缩放:适应/1:1 缩放下拉 │ ─ │
├──────────┬─────────────────────────┬───────────────┤
│ 缩略图栏  │  ImageViewer(显示区)    │ 滤镜面板        │
│ (可折叠)  │  空状态/加载状态         │ 直方图面板      │
│          │                         │ (右侧栏可折叠)  │
├──────────┴─────────────────────────┴───────────────┤
│ 状态栏:文件名 · 尺寸 · 格式 · 缩放比例 · 文件大小     │
└────────────────────────────────────────────────────┘
```

- 菜单栏:文件(打开/打开最近/保存/另存为/退出)、视图(缩略图栏/侧边栏开关、主题子菜单、缩放)、帮助(关于)
- 工具栏:打开/保存/撤销/重做(撤销重做本轮禁用)+ 缩放控件(适应窗口、实际大小、比例显示)
- 缩略图栏:已打开图片缩略图,点击切换,右上角 × 关闭,可折叠(Ctrl+B)
- 显示区:QML Image 异步加载;空状态提示;加载中指示;加载失败错误占位
- 右侧面板:滤镜参数控件骨架(滑杆+数值,禁用)、直方图 Canvas 占位;整栏可折叠(Ctrl+L)
- 状态栏:文件名、像素尺寸、文件格式、当前缩放比例、文件大小(文件大小由 C++ 辅助函数提供,QML 无文件 stat API,见下)

## 交互行为

**打开/保存**
- 打开:FileDialog 多选,过滤器 png/jpg/jpeg/bmp/webp/gif/tiff;追加进 AppState.images,跳到新打开的第一张
- 保存:本轮"保存"与"另存为"行为一致,均弹出另存为对话框,仅支持 PNG(QML Image.saveToFile 限制),界面给出提示;完整格式支持待 C++ 管线接入;无图时保存/另存为菜单与按钮禁用
- 打开最近:菜单列出最近文件(QtCore.Settings,最多 8 条)

**缩放/平移**
- 模式:适应窗口(默认)/ 实际大小 / 自定义倍率
- 自定义倍率范围 10%–800%
- 滚轮以鼠标位置为中心缩放;双击切换适应 ↔ 实际大小
- 自定义倍率下可拖拽平移;回到适应窗口时平移复位
- 工具栏与状态栏同步显示当前比例

**标签页**
- 点击缩略图切换;每图独立保存缩放状态;关闭当前(Ctrl+W)
- 全部关闭后显示区回到空状态

**主题**
- 视图 → 主题:跟随系统 / 浅色 / 深色,立即生效并持久化
- Material 风格;显示区底色随主题变化

**快捷键**
- Ctrl+O 打开、Ctrl+S 另存为 PNG、Ctrl+W 关闭当前、Ctrl+1 适应窗口、Ctrl+2 实际大小、Ctrl+0 重置缩放、Ctrl+B 折叠左栏、Ctrl+L 折叠右栏

## CMake 变更

- [cmakes/Targets.cmake](cmakes/Targets.cmake):`qt_add_qml_module(chameleon_lib ...)` 的 QML_FILES 补全新增文件
- 目录调整:chameleon_lib 源码改为 src/config/config_manager.cc 与 src/utils/file_utils.cc;`target_include_directories(chameleon_lib PUBLIC ...)` 由 src/ 改为 `${CMAKE_SOURCE_DIR}/include`;main.cpp 与 tests 的 include 引用改为 `"config/config_manager.h"` 等二级目录形式
- 新增最小 C++ 辅助:`FileUtils` QObject(`Q_INVOKABLE qint64 fileSizeOf(const QUrl &)`)——QML 无文件 stat API,状态栏显示文件大小需要它
- 无新增第三方依赖:QtCore.Settings、QtQuick.Dialogs 均在现有 Qt Quick 模块内

## 验证计划

1. msvc-debug / msvc-release 双预设构建通过,新增 QML 过 qmllint 无错误
2. 现有 gtest 测试双预设通过(纯 QML 改动,预期不受影响)
3. offscreen 启动确认 QML 无运行时错误;视觉确认由用户在真机 F5 完成
4. 同步 TODO_List.md:勾选本期实际交付条目,保留未完成项(滤镜预览控件、转换/批处理对话框等)
