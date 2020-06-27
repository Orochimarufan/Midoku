import QtQml 2.3
import QtQuick 2.11
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

import org.kde.kirigami 2.4 as Kirigami

Kirigami.ApplicationWindow {
    id: root
    visible: true

    width: 500
    height: 800

    title: qsTr("Midoku")

    PlayerPage {
        id: mainPage
    }

    Component {
        id: libraryPageComponent
        LibraryPage {
        }
    }

    Component {
        id: settingsPageComponent
        SettingsPage {
        }
    }

    pageStack.initialPage: mainPage

    globalDrawer: Kirigami.GlobalDrawer {
        title: "味読 Midoku"
        titleIcon: "applications-music"
        bannerImageSource: "qrc:/img/banner.jpg"

        actions: [
            Kirigami.Action {
                text: "Library"
                onTriggered: pageStack.push(libraryPageComponent)
            }
        ]
    }
}
