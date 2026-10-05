# Chameleon 界面设计与布局 — 设计文档(Qt Widgets 版)

- 日期:2026-10-05(重写,替代 QML 版)
- 状态:设计已确认,执行中
- 关联 TODO:docs/TODO_List.md「界面设计与布局」章

## 背景与目标

Chameleon 是基于 Qt + OpenCV 的 Windows 图片处理软件。UI 原计划用 QML(Qt Quick)实现,实施中遇到多个 qmlcachegen 编译期缺陷(单例注册、url 比较、布局附着属性),且 QML 无法用 gtest 做自动化 UI 验证。**经用户决定:放弃 QML,全部改用 Qt Widgets 实现。**

目标不变:完整的应用 UI 骨架,打开/显示/缩放/平移/标签页/主题等核心交互真实可用,滤镜/直方图/批处理等后续功能以占位面板呈现。

## 架构(已与用户确认)

| 原 QML | Widgets 实现 |
| --- | --- |
| ApplicationWindow | `MainWindow`(QMainWindow) |
| AppState.qml 单例 | `AppState` C++ QObject 单例(gtest 可直接测试) |
| ImageViewer.qml | `ImageViewerWidget`:paintEvent 绘制 QImage,滚轮/拖拽/双击缩放平移 |
| ThumbnailBar.qml | `QListWidget`(IconMode,itemWidget 带 × 按钮)在左侧 `QDockWidget` |
| FilterPanel / HistogramPanel | 右侧 `QDockWidget` 内两个占位 QWidget(禁用滑杆 + 边框占位) |
| StatusBar.qml | `QStatusBar` + QLabel(文件名/尺寸/格式/缩放/文件大小) |
| 菜单栏 / 工具栏 | `QMenuBar` / `QToolBar`(QAction + 快捷键) |
| FileDialog / AboutDialog | `QFileDialog` / `QMessageBox::about` |
| Material 主题 | Fusion 风格 + 深/浅调色板;跟随系统(读注册表 AppsUseLightTheme);QSettings 持久化 |
| context property 注入 | 删除——C++ 直接调用 ConfigManager / FileUtils |

## 文件结构

```
include/
├── app/
│   └── app_state.h           # AppState 单例(状态与逻辑)
├── config/config_manager.h   # 现有,不动
└── utils/file_utils.h        # 现有,不动
src/
├── main.cpp                  # QApplication + 主题 + MainWindow
├── app/app_state.cc
└── ui/
    ├── main_window.h/.cc
    ├── image_viewer_widget.h/.cc
    ├── filter_panel.h/.cc
    └── histogram_panel.h/.cc
```

删除:src/ 下全部 .qml、`qt_add_qml_module`、chameleon_libplugin 链接、`.qmlls.ini`、launch/test 预设中 QML2_IMPORT_PATH、QT_QML_SINGLETON_TYPE 等 QML 配置;`find_package` Quick/QuickControls2 → **Widgets**。

## 状态管理(AppState,C++ 单例)

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `images()` | QStringList | 已打开文件(标签页 = 列表项) |
| `currentIndex()` | int | -1 表示无 |
| `currentImage()` | QString | 空串表示无 |
| `hasImage()` | bool | 派生自索引有效性 |
| `zoomMode()` | enum Fit/Actual/Custom | |
| `zoomFactor()` | double | 自定义倍率,钳制 0.1–8.0 |
| `zoomFactorForCurrent()` | double | 每图独立倍率(zoomByImage map) |
| `imageInfo()` | QSize | 当前图片像素尺寸(ImageViewer 写入) |
| `theme()` | QString | "system"/"light"/"dark",QSettings 持久化 |
| `recentFiles()` | QStringList | 最多 8 条,去重,QSettings 持久化 |

信号:`imagesChanged` / `currentIndexChanged` / `zoomChanged` / `imageInfoChanged` / `themeChanged` / `recentFilesChanged`。

## 布局与区域职责

与 QML 版一致:菜单栏(文件/视图/帮助)、工具栏(打开/保存/撤销/重做禁用/缩放控件)、左侧缩略图 dock、中央 ImageViewer、右侧滤镜+直方图 dock、底部状态栏(文件名/尺寸/格式/缩放比例/文件大小)。标签页由缩略图栏承担。

## 交互行为(与 QML 版一致)

- 打开:QFileDialog 多选,过滤器 png/jpg/jpeg/bmp/webp/gif/tiff;追加并跳到第一张
- 保存/另存为:本轮行为一致,仅 PNG(viewer->saveAs),状态栏 showMessage 提示
- 打开最近:菜单列出最近文件(最多 8 条)
- 缩放:适应窗口(默认)/实际大小/自定义 10%–800%;滚轮以鼠标为中心;双击切换适应↔实际;放大后拖拽平移;回到适应时复位;工具栏与状态栏同步百分比
- 标签页:点击缩略图切换;每图独立缩放;× 关闭;Ctrl+W 关闭当前;全部关闭回空状态(保存/缩放禁用,状态栏复位)
- 主题:跟随系统/浅色/深色,即时生效并持久化;显示区底色随主题
- 快捷键:Ctrl+O 打开、Ctrl+S 保存、Ctrl+W 关闭当前、Ctrl+1 适应、Ctrl+2 实际大小、Ctrl+0 重置缩放、Ctrl+B 折叠左栏、Ctrl+L 折叠右栏
- 空状态:显示区绘制"按 Ctrl+O 打开图片";加载失败:绘制"无法加载图片:xxx",不崩溃

## 测试策略(核心升级)

全部用 gtest,无需人工 F5:
1. **AppStateTest**:openImages/closeImage 边界(关闭中间/末尾)、缩放钳制(42→8.0、0.01→0.1)、最近文件去重与上限 8
2. **MainWindowTest**:菜单结构(文件/视图/帮助)、空状态栏文本、打开图片后状态栏(文件名 + "640 × 400")、关闭全部回空态、动作使能状态
3. **像素渲染测试**:`QWidget::grab()` 抓取 ImageViewer,断言测试图橙色椭圆像素数 > 100(真实渲染证据)
4. **主题测试**:切深色后 `qApp->palette()` 窗口色亮度 < 100

## 验证计划

msvc-debug / msvc-release 双预设构建零错误;ctest 双预设全部通过;offscreen 启动冒烟;TODO_List.md 同步(勾选交付项,保留占位相关未完成项)。
