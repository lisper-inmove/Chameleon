import QtQuick
import QtQuick.Controls

Dialog {
    id: dialog
    title: "关于 Chameleon"
    modal: true
    standardButtons: Dialog.Ok

    Column {
        spacing: 8
        width: 280

        Label {
            text: "Chameleon"
            font.pixelSize: 20
            font.bold: true
        }
        Label { text: "基于 Qt Quick + OpenCV 的图片处理软件" }
        Label {
            text: "版本 0.1.0"
            color: Material.foreground
            opacity: 0.6
        }
    }
}
