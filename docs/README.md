# Chameleon

基于 **Qt Widgets + OpenCV** 的桌面图片处理软件。

## 技术栈

| 组件 | 用途 |
| --- | --- |
| Qt 6.12 Widgets (MSVC 2022 套件) | UI 框架 |
| OpenCV 5.1 | 图像处理核心 |
| spdlog | 日志 |
| yaml-cpp | 解析运行时配置(configs/app.yaml) |
| googletest | 单元测试(含 UI 与像素渲染测试) |
| CMake + VS 2026 | 构建系统 |

## 构建

需要:Visual Studio 2026、Qt 6.12.0 msvc2022_64、预编译第三方库(F:/codes/third_party 与 F:/codes/third_party_debug)。

```bash
cmake --preset msvc-debug     # Debug 配置
cmake --build --preset msvc-debug
cmake --preset msvc-release   # Release 配置
cmake --build --preset msvc-release
```

VSCode 中直接按 F5 即可构建并调试(预设已在 .vscode/settings.json 中配置)。

## 测试

```bash
ctest --preset msvc-debug
```

VSCode 中可在 Testing 面板(TestMate C++ 提供者)直接运行单个测试。

## 目录结构

```
cmakes/           # CMake 模块(依赖、编译选项、目标、测试)
configs/          # 运行时 YAML 配置(app.yaml,开发时直接生效)
include/          # C++ 头文件(按二级目录组织:app/ config/ ui/ utils/)
src/              # 源码(.cc,入口 src/main.cpp;ui/ 与 app/ 二级目录)
tests/            # 单元测试(.cc,入口 tests/main.cpp;含 UI 自动化测试)
docs/             # 项目文档
```

## 配置

运行时配置位于 [configs/app.yaml](../configs/app.yaml),开发时修改立即生效(见 [src/main.cpp](../src/main.cpp) 的加载逻辑)。

## 功能规划

见 [TODO_List.md](TODO_List.md)。
