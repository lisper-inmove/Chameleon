import QtQuick
import QtQuick.Controls

import Chameleon

// qmllint disable unqualified
Item {
    id: root
    height: 26

    function fileNameOf(url) {
        return url !== "" ? String(url).split("/").pop() : ""
    }

    function formatOf(url) {
        var name = fileNameOf(url)
        var dot = name.lastIndexOf(".")
        return dot > 0 ? name.substring(dot + 1).toUpperCase() : ""
    }

    function sizeText() {
        if (AppState.currentImage === "")
            return ""
        var bytes = fileUtils.fileSizeOf(AppState.currentImage)
        if (bytes < 0)
            return ""
        if (bytes < 1024)
            return bytes + " B"
        if (bytes < 1024 * 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1024 / 1024).toFixed(1) + " MB"
    }

    function zoomText() {
        if (AppState.currentImage === "")
            return ""
        if (AppState.zoomMode === AppState.ZoomMode.Fit)
            return "适应窗口"
        if (AppState.zoomMode === AppState.ZoomMode.Actual)
            return "100%"
        return Math.round(AppState.zoomFactorForCurrent() * 100) + "%"
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 16

        Label {
            text: AppState.currentImage !== ""
                ? root.fileNameOf(AppState.currentImage) : "未打开图片"
            elide: Text.ElideMiddle
            width: 220
        }

        Label {
            visible: AppState.imageInfo.width !== undefined
            text: AppState.imageInfo.width + " × " + AppState.imageInfo.height
        }

        Label {
            visible: root.formatOf(AppState.currentImage) !== ""
            text: root.formatOf(AppState.currentImage)
        }

        Label {
            text: root.zoomText()
        }

        Label {
            text: root.sizeText()
        }
    }
}
