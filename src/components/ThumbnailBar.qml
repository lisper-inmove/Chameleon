import QtQuick
import QtQuick.Controls

import Chameleon

Item {
    id: root

    implicitWidth: 180

    Label {
        anchors.centerIn: parent
        visible: AppState.images.length === 0
        text: "未打开图片"
        color: Material.foreground
        opacity: 0.6
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        model: AppState.images
        clip: true
        spacing: 4

        delegate: Item {
            width: list.width
            height: 110

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: 6
                color: index === AppState.currentIndex
                    ? Material.accent : Material.background
                opacity: index === AppState.currentIndex ? 0.25 : 1.0
            }

            Image {
                anchors.fill: parent
                anchors.margins: 12
                anchors.rightMargin: 32
                source: modelData
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 120
                cache: false
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 10
                text: String(modelData).split("/").pop()
                elide: Text.ElideMiddle
                color: Material.foreground
            }

            RoundButton {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                width: 20
                height: 20
                text: "×"
                onClicked: AppState.closeImage(index)
            }

            MouseArea {
                anchors.fill: parent
                onClicked: AppState.currentIndex = index
            }
        }
    }
}
