import QtQuick 2.15
import QtQuick.Controls 2.15

ToolButton {
    // --- Properties ---
    // May be set externally
    property bool active: false // Sleep timer is enabled
    property bool paused: true // should be bound to player pause state
    property int duration: 30 // timer duration in minutes

    // Should only be read
    readonly property int durationSec: duration * 60
    property int countdown: durationSec

    // Sleep timeout triggered
    signal timeout() // Should pause playback

    // --- Component Code ---
    id: sleep

    // Label text
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

    icon.name: "clock"
    text: active ? format_time(countdown) : "Sleep"

    // Main button function
    onClicked: {
        // Allow resetting timer by single click on button after timer was running over 2 min.
        // Open config popup if inactive or recently reset
        if (active && countdown < durationSec - 120)
            reset()
        else
            popup.open()
    }

    // Timer logic.
    Timer {
        id: timer

        interval: 1000
        repeat: true

        onTriggered: {
            sleep.countdown -= 1
            if (sleep.countdown < 1) {
                // Don't stop timer here, should happen through the paused feedback
                sleep.reset()
                sleep.timeout()
            }
        }
    }

    function reset() {
        countdown = durationSec
        if (active && !paused)
            timer.start()
        else
            timer.stop()
    }

    onActiveChanged: reset()
    onPausedChanged: reset()
    onDurationSecChanged: reset() // must be durationSec, not duration!

    // Config popup
    onDurationChanged: spinBox.value = duration

    RowPopup {
        id: popup
        text: "Sleep Timer"

        SpinBox {
            id: spinBox
            from: 1
            value: sleep.duration
            stepSize: 10

            onValueModified: sleep.duration = value
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: "min"
        }

        Button {
            text: sleep.active? "Stop" : "Start"
            checkable: true
            checked: sleep.active
            onToggled: {
                sleep.active = !sleep.active
                popup.close()
            }
        }
    }
}
