import QtQuick
import QtQuick.Controls

import Chameleon

// qmllint disable unqualified
Item {
    id: root

    readonly property real minScale: 0.1
    readonly property real maxScale: 8.0

    function effectiveScale() {
        switch (AppState.zoomMode) {
        case AppState.ZoomMode.Actual:
            return 1.0
        case AppState.ZoomMode.Custom:
            return AppState.zoomFactorForCurrent()
        default: // Fit
            if (image.sourceSize.width <= 0 || image.sourceSize.height <= 0)
                return 1.0
            return Math.min(flick.width / image.sourceSize.width,
                            flick.height / image.sourceSize.height)
        }
    }

    // 供外部(保存流程)调用
    function saveAs(url) {
        if (image.status !== Image.Ready)
            return false
        return image.saveToFile(fileUtils.toLocalPath(url))
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: disp.width
        contentHeight: disp.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: disp.width > width || disp.height > height

        Item {
            id: disp
            width: image.sourceSize.width * root.effectiveScale()
            height: image.sourceSize.height * root.effectiveScale()
            x: Math.max(0, (flick.width - width) / 2)
            y: Math.max(0, (flick.height - height) / 2)

            Image {
                id: image
                anchors.fill: parent
                source: AppState.currentImage
                asynchronous: true
                fillMode: Image.Stretch
                smooth: root.effectiveScale() < 2.0
                visible: status === Image.Ready

                onStatusChanged: {
                    if (status === Image.Ready) {
                        AppState.imageInfo = {
                            width: sourceSize.width,
                            height: sourceSize.height
                        }
                    } else if (status === Image.Error || status === Image.Null) {
                        AppState.imageInfo = ({})
                    }
                }
            }
        }
    }

    // 滚轮缩放(以鼠标位置为中心)+ 双击切换适应/实际大小
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: function (wheel) {
            if (AppState.currentImage === "" || image.status !== Image.Ready)
                return
            var factor = wheel.angleDelta.y > 0 ? 1.15 : 1 / 1.15
            var newScale = Math.min(root.maxScale,
                Math.max(root.minScale, root.effectiveScale() * factor))
            var mx = wheel.x
            var my = wheel.y
            AppState.setZoomFactor(newScale)
            flick.contentX = mx - (mx - flick.contentX) * (newScale / root.effectiveScale())
            flick.contentY = my - (my - flick.contentY) * (newScale / root.effectiveScale())
        }
        onDoubleClicked: {
            if (AppState.currentImage === "")
                return
            AppState.zoomMode === AppState.ZoomMode.Fit
                ? AppState.setZoomMode(AppState.ZoomMode.Actual)
                : AppState.setZoomMode(AppState.ZoomMode.Fit)
        }
    }

    // 空状态
    Label {
        anchors.centerIn: parent
        visible: AppState.currentImage === ""
        text: "按 Ctrl+O 打开图片"
        color: Material.foreground
        opacity: 0.6
    }

    // 加载状态
    BusyIndicator {
        anchors.centerIn: parent
        visible: image.status === Image.Loading
    }

    // 错误状态(Review Focus 1)
    Label {
        anchors.centerIn: parent
        visible: image.status === Image.Error
        text: "无法加载图片:" + String(AppState.currentImage).split("/").pop()
        color: Material.color(Material.Red)
    }
}
