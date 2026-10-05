pragma Singleton

import QtQuick
import QtCore

QtObject {
    id: root

    enum ZoomMode {
        Fit,     // 适应窗口
        Actual,  // 实际大小
        Custom   // 自定义倍率
    }

    // 已打开的文件列表(标签页 = 列表项)
    property var images: []
    property int currentIndex: -1

    // 缩放状态:全局模式 + 每图独立倍率(键为 url 字符串)
    property int zoomMode: AppState.ZoomMode.Fit
    property real zoomFactor: 1.0
    property var zoomByImage: ({})

    // 当前图片信息(由 ImageViewer 在加载完成后写入)
    property var imageInfo: ({})

    // 主题:"system" | "light" | "dark"
    property string theme: "system"

    // 最近打开的文件(最多 8 条)
    property var recentFiles: []

    readonly property url currentImage:
        currentIndex >= 0 && currentIndex < images.length ? images[currentIndex] : ""

    function currentZoomKey() {
        return currentImage !== "" ? String(currentImage) : ""
    }

    function zoomFactorForCurrent() {
        var key = currentZoomKey()
        if (key !== "" && zoomByImage[key] !== undefined)
            return zoomByImage[key]
        return 1.0
    }

    function setZoomMode(mode) {
        zoomMode = mode
        if (mode === AppState.ZoomMode.Fit) {
            var key = currentZoomKey()
            if (key !== "")
                delete zoomByImage[key]
        } else if (mode === AppState.ZoomMode.Custom) {
            zoomFactor = zoomFactorForCurrent()
        } else if (mode === AppState.ZoomMode.Actual) {
            var key2 = currentZoomKey()
            if (key2 !== "")
                zoomByImage[key2] = 1.0
        }
    }

    function setZoomFactor(factor) {
        zoomFactor = Math.min(8.0, Math.max(0.1, factor))
        zoomMode = AppState.ZoomMode.Custom
        var key = currentZoomKey()
        if (key !== "")
            zoomByImage[key] = zoomFactor
    }

    function openImages(urls) {
        if (!urls || urls.length === 0)
            return
        var firstNew = images.length
        for (var i = 0; i < urls.length; ++i)
            images.push(urls[i])
        currentIndex = firstNew
        for (var j = 0; j < urls.length; ++j) {
            var entry = String(urls[j])
            var idx = recentFiles.indexOf(entry)
            if (idx >= 0)
                recentFiles.splice(idx, 1)
            recentFiles.unshift(entry)
        }
        while (recentFiles.length > 8)
            recentFiles.pop()
    }

    function closeImage(index) {
        if (index < 0 || index >= images.length)
            return
        images.splice(index, 1)
        if (images.length === 0) {
            currentIndex = -1
            imageInfo = ({})
            return
        }
        if (currentIndex > index)
            currentIndex -= 1
        else if (currentIndex >= images.length)
            currentIndex = images.length - 1
    }

    // QtObject has no default property, so Settings cannot be a child
    // object; bind it to a property instead.
    property Settings settings: Settings {
        category: "ui"
        property alias theme: root.theme
        property alias recentFiles: root.recentFiles
    }
}
