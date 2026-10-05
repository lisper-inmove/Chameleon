import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

import Chameleon
import "components"
import "dialogs"

// qmllint disable unqualified
ApplicationWindow {
    id: window

    // appConfig 为 C++ 注入的 QVariantMap context property(configs/app.yaml)
    width: appConfig && appConfig.width ? appConfig.width : 1200
    height: appConfig && appConfig.height ? appConfig.height : 800
    visible: true
    title: appConfig && appConfig.title ? appConfig.title : "Chameleon"

    // 主题切换(运行时即时生效;风格本身已在 main.cpp 用 QQuickStyle 选定)
    Material.theme: AppState.theme === "light" ? Material.Light
                  : AppState.theme === "dark" ? Material.Dark
                  : Material.System
    Material.accent: Material.DeepOrange

    property bool leftPanelVisible: true
    property bool rightPanelVisible: true

    // 临时提示
    function showToast(msg) {
        toast.text = msg
        toast.visible = true
        toastTimer.restart()
    }

    menuBar: MenuBar {
        Menu {
            title: "文件(&F)"
            Action { text: "打开(&O)..."; shortcut: "Ctrl+O"; onTriggered: openDialog.open() }
            Menu {
                title: "打开最近"
                enabled: AppState.recentFiles.length > 0
                Instantiator {
                    model: AppState.recentFiles
                    delegate: MenuItem {
                        text: String(modelData).split("/").pop()
                        onTriggered: AppState.openImages([String(modelData)])
                    }
                }
            }
            MenuSeparator {}
            Action {
                text: "保存(&S)"
                shortcut: "Ctrl+S"
                enabled: AppState.currentImage !== ""
                onTriggered: saveAsDialog.open()
            }
            Action {
                text: "关闭当前(&W)"
                shortcut: "Ctrl+W"
                enabled: AppState.currentImage !== ""
                onTriggered: AppState.closeImage(AppState.currentIndex)
            }
            MenuSeparator {}
            Action { text: "退出(&X)"; onTriggered: Qt.quit() }
        }
        Menu {
            title: "视图(&V)"
            Action {
                text: "缩略图栏"
                shortcut: "Ctrl+B"
                checkable: true
                checked: window.leftPanelVisible
                onTriggered: window.leftPanelVisible = checked
            }
            Action {
                text: "侧边栏"
                shortcut: "Ctrl+L"
                checkable: true
                checked: window.rightPanelVisible
                onTriggered: window.rightPanelVisible = checked
            }
            MenuSeparator {}
            Menu {
                title: "主题"
                MenuItem {
                    text: "跟随系统"
                    checkable: true
                    checked: AppState.theme === "system"
                    onTriggered: AppState.theme = "system"
                }
                MenuItem {
                    text: "浅色"
                    checkable: true
                    checked: AppState.theme === "light"
                    onTriggered: AppState.theme = "light"
                }
                MenuItem {
                    text: "深色"
                    checkable: true
                    checked: AppState.theme === "dark"
                    onTriggered: AppState.theme = "dark"
                }
            }
            MenuSeparator {}
            Action {
                text: "适应窗口"
                shortcut: "Ctrl+1"
                enabled: AppState.currentImage !== ""
                onTriggered: AppState.setZoomMode(AppState.ZoomMode.Fit)
            }
            Action {
                text: "实际大小"
                shortcut: "Ctrl+2"
                enabled: AppState.currentImage !== ""
                onTriggered: AppState.setZoomMode(AppState.ZoomMode.Actual)
            }
            Action {
                text: "重置缩放"
                shortcut: "Ctrl+0"
                enabled: AppState.currentImage !== ""
                onTriggered: AppState.setZoomMode(AppState.ZoomMode.Fit)
            }
        }
        Menu {
            title: "帮助(&H)"
            Action { text: "关于 Chameleon(&A)..."; onTriggered: aboutDialog.open() }
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton {
                text: "打开"
                onClicked: openDialog.open()
            }
            ToolButton {
                text: "保存"
                enabled: AppState.currentImage !== ""
                onClicked: saveAsDialog.open()
            }
            ToolButton { text: "撤销"; enabled: false }
            ToolButton { text: "重做"; enabled: false }

            Item { Layout.fillWidth: true }

            ToolButton {
                text: "适应"
                checkable: true
                checked: AppState.zoomMode === AppState.ZoomMode.Fit
                enabled: AppState.currentImage !== ""
                onClicked: AppState.setZoomMode(AppState.ZoomMode.Fit)
            }
            ToolButton {
                text: "1:1"
                checkable: true
                checked: AppState.zoomMode === AppState.ZoomMode.Actual
                enabled: AppState.currentImage !== ""
                onClicked: AppState.setZoomMode(AppState.ZoomMode.Actual)
            }
            Label {
                text: AppState.currentImage !== ""
                    ? (AppState.zoomMode === AppState.ZoomMode.Fit ? "适应"
                       : Math.round((AppState.zoomMode === AppState.ZoomMode.Actual
                           ? 1.0 : AppState.zoomFactorForCurrent()) * 100) + "%")
                    : ""
                width: 48
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ThumbnailBar {
                visible: window.leftPanelVisible
                Layout.preferredWidth: 180
                Layout.fillHeight: true
            }

            ImageViewer {
                id: imageViewer
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            ColumnLayout {
                visible: window.rightPanelVisible
                Layout.preferredWidth: 240
                Layout.fillHeight: true
                spacing: 0

                FilterPanel { Layout.fillWidth: true; Layout.preferredHeight: 260 }
                HistogramPanel { Layout.fillWidth: true; Layout.fillHeight: true }
            }
        }

        StatusBar { Layout.fillWidth: true }
    }

    FileDialog {
        id: openDialog
        title: "打开图片"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tif *.tiff)",
                      "所有文件 (*.*)"]
        onAccepted: AppState.openImages(selectedFiles)
    }

    FileDialog {
        id: saveAsDialog
        title: "另存为 PNG"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: ["PNG 图片 (*.png)"]
        onAccepted: {
            if (imageViewer.saveAs(selectedFile))
                window.showToast("已保存:" + String(selectedFile).split("/").pop())
            else
                window.showToast("保存失败")
        }
    }

    AboutDialog {
        id: aboutDialog
    }

    // 临时提示(保存结果等)
    Label {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 48
        padding: 10
        visible: false
        background: Rectangle {
            radius: 6
            color: Material.background
        }
        text: ""
    }
    Timer {
        id: toastTimer
        interval: 2500
        onTriggered: toast.visible = false
    }
}
