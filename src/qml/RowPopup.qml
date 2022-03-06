import QtQuick 2.15
import QtQuick.Controls 2.15

Popup {
    property bool alignRight: false
    property alias text: label.text
    default property alias data: layout.data

    id: root

    x: alignRight ? parent.width - implicitWidth : 0
    spacing: 3
    modal: true
    focus: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

    contentItem: Row {
        id: layout
        spacing: root.spacing

        Label {
            id: label
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
