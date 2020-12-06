import QtQml 2.3
import QtQuick 2.11
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

import org.kde.kirigami 2.4 as Kirigami

//import midoku.mpv 1.0
Kirigami.Page {

    property var player: app.player
    property var mpv: player.mpv
    property var locked: false

    function playBook(book) {
        app.playBook(book)
    }

    Shortcut {
        autoRepeat: false
        sequence: "Space"

        onActivated: mpv.pause = !mpv.pause
    }

    header: ToolBar {
        id: tools

        RowLayout {
            anchors.fill: parent

            ToolButton {
                text: "Sleep"
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: "Volume"
            }

            ToolButton {
                id: speedButton
                text: "Speed"
                onClicked: speedPopup.open()
            }

            ToolButton {
                text: "Lock"
                checkable: true
                onCheckedChanged: locked = checked
            }
        }
    }

    Popup {
        id: speedPopup
        modal: true
        focus: true
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        x: speedButton.x
        y: speedButton.y

        contentItem: SpinBox {
            from: 0
            value: 100
            to: 1000
            stepSize: 10
            anchors.centerIn: parent

            property int decimals: 2
            property real realValue: value / 100

            validator: DoubleValidator {
                bottom: 0
                top: 1000
            }

            textFromValue: function (valuelocale) {
                return Number(value / 100).toLocaleString(locale, 'f', decimals)
            }

            valueFromText: function (textlocale) {
                return Number.fromLocaleString(locale, text) * 100
            }

            onValueModified: mpv.speed = realValue
        }
    }

    ColumnLayout {
        anchors.fill: parent

        Label {
            id: bookLabel
            text: player.book.author + " - " + player.book.title
        }

        Slider {
            Layout.fillWidth: true

            from: 0
            to: player.duration_book
            value: player.time_book

            onMoved: app.seekBook(value)
            enabled: !locked
        }

        TimeLabel {
            id: totalTimeLabel

            time: player.time_book
            total: player.duration_book
        }

        Label {
            id: chapLabel
            text: player.chapter.chapter + ". " + player.chapter.title
        }

        Slider {
            Layout.fillWidth: true

            id: slider
            from: 0
            to: player.duration_chapter
            value: player.time_chapter
            onMoved: app.seekChapter(value)
            enabled: !locked
        }

        TimeLabel {
            id: timeLabel

            time: player.time_chapter
            total: player.duration_chapter
        }

        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            Image {
                source: "image://blob/" + player.book.cover_blob_id
                fillMode: Image.PreserveAspectFit
                anchors.fill: parent
            }

            Button {
                anchors.centerIn: parent

                text: mpv.pause ? "Play |>" : "Pause ||"
                onClicked: mpv.pause = !mpv.pause
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right

                text: "Next >>"
                onClicked: app.nextChapter()
                enabled: !locked
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left

                text: "<< Prev"
                onClicked: app.previousChapter()
                enabled: !locked
            }

            Button {
                anchors.bottom: parent.bottom
                anchors.left: parent.left

                text: "<< 1m"
                onClicked: app.seekRelative(-60)
            }

            Button {
                anchors.bottom: parent.bottom
                anchors.right: parent.right

                text: "1m >>"
                onClicked: app.seekRelative(60)
            }
        }
    }
}
