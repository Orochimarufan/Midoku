import QtQml 2.3
import QtQuick 2.11
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

import org.kde.kirigami 2.4 as Kirigami

Label {
    Layout.alignment: Qt.AlignRight

    property int time
    property int total

    function pad_2(x) {
        return x < 10 ? "0" + x : x
    }

    function format_time(t) {
        var h = Math.floor(t / 3600)
        t %= 3600
        var m = Math.floor(t / 60)
        t %= 60
        return (h > 0 ? pad_2(h) + ":" : "") + pad_2(m) + ":" + pad_2(t)
    }

    text: format_time(time) + "/" + format_time(total)
}
