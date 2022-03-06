import QtQuick 2.15
import QtQuick.Controls 2.15

ToolButton {
    property alias alignRight: popup.alignRight
    property alias label: popup.text
    default property alias data: popup.data

    onClicked: popup.open()

    RowPopup {
        id: popup
    }
}
