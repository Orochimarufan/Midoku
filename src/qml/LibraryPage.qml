import QtQml 2.3
import QtQuick 2.11
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

import org.kde.kirigami 2.4 as Kirigami

Kirigami.Page {
    id: libraryPage

    TabBar {
        id: tabs

        anchors.top: parent.top
        width: parent.width

        currentIndex: libraryView.currentIndex

        onCurrentIndexChanged: libraryView.currentIndex = currentIndex

        TabButton {
            text: "All"
        }

        TabButton {
            text: "New"
        }

        TabButton {
            text: "Started"
        }

        TabButton {
            text: "Finished"
        }
    }

    SwipeView {
        id: libraryView

        anchors.top: tabs.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        Item {
            id: allPage

            ListView {
                anchors.fill: parent

                model: app.booksModel()

                delegate: Kirigami.BasicListItem {
                    id: listItem

                    reserveSpaceForIcon: false
                    label: model.title

                    onClicked: {
                        mainPage.playBook(model.id)
                        root.pageStack.pop()
                    }
                }
            }
        }

        Item {
            id: newPage
        }

        Item {
            id: startedPage
        }

        Item {
            id: finishedPage
        }
    }
}
