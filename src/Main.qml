import QtQuick
import QtQuick.Controls

// qmllint disable unqualified
// appConfig is a QVariantMap context property injected from C++ (see
// src/main.cpp), backed by configs/app.yaml.
ApplicationWindow {
    width: appConfig && appConfig.width ? appConfig.width : 1200
    height: appConfig && appConfig.height ? appConfig.height : 800
    visible: true
    title: appConfig && appConfig.title ? appConfig.title : qsTr("Chameleon")
}
