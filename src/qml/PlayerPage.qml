import QtQml 2.3
import QtQuick 2.11
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

import Qt.labs.settings 1.0

import org.kde.kirigami 2.4 as Kirigami


Kirigami.Page {

    property var player: app.player
    property var mpv: player.mpv
    property bool locked: false

    id: root

    // ------ Misc ------
    function playBook(book) {
        app.playBook(book)
    }

    Shortcut {
        autoRepeat: false
        sequence: "Space"

        onActivated: mpv.pause = !mpv.pause
    }

    // ------ ToolBar ------
    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            SleepButton {
                id: sleep
                paused: mpv.pause
                onTimeout: mpv.pause = true
            }

            Item {
                Layout.fillWidth: true
            }

            PopupButton {
                icon.name: {
                    var v = mpv.volume
                    if (v === 0)
                        return "audio-volume-muted"
                    else if (v <= 33)
                        return "audio-volume-low"
                    else if (v <= 66)
                        return "audio-volume-medium"
                    else
                        return "audio-volume-high"
                }
                text: mpv.volume.toFixed() + "%"
                alignRight: true
                label: "Volume"
                Slider {
                    from: 0
                    to: 100
                    value: mpv.volume
                    onMoved: mpv.volume = value
                }
            }

            PopupButton {
                icon.name: "speedometer"
                text: mpv.speed !== 1 ? (mpv.speed * 100).toFixed() + "%" : null
                alignRight: true
                label: "Playback Speed"
                RealSpinBox {
                    decimals: 2
                    realFrom: 0.2
                    realTo: 5
                    realStep: 0.1
                    realValue: mpv.speed
                    onValueModified: mpv.speed = realValue
                }
            }

            ToolButton {
                icon.name: locked ? "object-locked" : "object-unlocked"
                checkable: true
                onCheckedChanged: locked = checked
            }
        }
    }

    // ------ Main Container -------
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

        // Art/Button area
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

                icon.name: mpv.pause ? "media-playback-start" : "media-playback-pause"
                text: mpv.pause ? "Play" : "Pause"
                onClicked: mpv.pause = !mpv.pause
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right

                icon.name: "media-skip-forward"
                text: "Next"
                onClicked: app.nextChapter()
                enabled: !locked
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left

                icon.name: "media-skip-backward"
                text: "Prev"
                onClicked: app.previousChapter()
                enabled: !locked
            }

            Button {
                anchors.bottom: parent.bottom
                anchors.left: parent.left

                icon.name: "media-seek-backward"
                text: "1m"
                onClicked: app.seekRelative(-60)
                enabled: !locked
            }

            Button {
                anchors.bottom: parent.bottom
                anchors.right: parent.right

                icon.name: "media-seek-forward"
                text: "1m"
                onClicked: app.seekRelative(60)
                enabled: !locked
            }
        }
    }

    // ------ Persistent Settings ------
    Settings {
        category: "Sleep"
        property alias active: sleep.active
        property alias duration: sleep.duration
    }

    Settings {
        id: playerSettings
        category: "Player"
        property alias uiLocked: root.locked

        // Cannot use property aliases with mpv properties
        Component.onCompleted: {
            var speed = value("speed", null)
            if (speed !== null)
                mpv.speed = speed
            mpv.speedChanged.connect(function() {
                playerSettings.setValue("speed", mpv.speed)
            })
            var volume = value("volume", null)
            if (volume !== null)
                mpv.volume = volume
            mpv.volumeChanged.connect(function() {
                playerSettings.setValue("volume", mpv.volume)
            })
        }
    }
}
