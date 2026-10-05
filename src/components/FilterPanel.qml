import QtQuick
import QtQuick.Controls

Item {
    implicitWidth: 240

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Label {
            text: "滤镜"
            font.bold: true
        }

        Label {
            text: "滤镜功能待实现"
            color: Material.foreground
            opacity: 0.6
            wrapMode: Text.WordWrap
            width: parent.width
        }

        // 参数控件骨架(禁用态,后续滤镜功能接入时启用)
        Column {
            width: parent.width
            spacing: 4

            Row {
                width: parent.width
                Label { text: "亮度"; width: 56 }
                Slider { enabled: false; from: -100; to: 100; value: 0; width: parent.width - 56 }
            }
            Row {
                width: parent.width
                Label { text: "对比度"; width: 56 }
                Slider { enabled: false; from: -100; to: 100; value: 0; width: parent.width - 56 }
            }
            Row {
                width: parent.width
                Label { text: "饱和度"; width: 56 }
                Slider { enabled: false; from: -100; to: 100; value: 0; width: parent.width - 56 }
            }
        }
    }
}
