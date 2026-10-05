import QtQuick
import QtQuick.Controls

Item {
    implicitWidth: 240

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Label {
            text: "直方图"
            font.bold: true
        }

        Rectangle {
            width: parent.width
            height: 140
            radius: 6
            color: Material.background
            border.color: Material.foreground
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: "直方图待实现"
                color: Material.foreground
                opacity: 0.6
            }
        }
    }
}
