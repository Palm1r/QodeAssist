// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var skillProvider: null
    property var searchResults: []
    property int currentIndex: 0

    signal selectionRequested()

    visible: searchResults.length > 0
    height: Math.min(searchResults.length * 40, 40 * 6) + 2

    color: palette.window
    border.color: palette.mid
    border.width: 1
    radius: 4

    function updateSearch(query) {
        searchResults = skillProvider ? skillProvider.searchSlashCommands(query) : []
        currentIndex = 0
    }

    function dismiss() {
        searchResults = []
        currentIndex = 0
    }

    function moveUp() {
        if (currentIndex > 0)
            currentIndex--
    }

    function moveDown() {
        if (currentIndex < searchResults.length - 1)
            currentIndex++
    }

    function currentName() {
        if (currentIndex >= 0 && currentIndex < searchResults.length)
            return searchResults[currentIndex].name
        return ""
    }

    onCurrentIndexChanged: listView.positionViewAtIndex(currentIndex, ListView.Contain)

    ListView {
        id: listView

        anchors.fill: parent
        anchors.margins: 1
        model: root.searchResults
        currentIndex: root.currentIndex
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Rectangle {
            id: delegateItem

            required property int index
            required property var modelData

            width: listView.width
            height: 40
            color: index === root.currentIndex
                   ? palette.highlight
                   : (hoverArea.containsMouse
                      ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.25)
                      : "transparent")

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: "/" + delegateItem.modelData.name
                        textFormat: Text.PlainText
                        color: delegateItem.index === root.currentIndex
                               ? palette.highlightedText
                               : palette.text
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        visible: sourceBadge.text.length > 0
                        implicitWidth: Math.min(sourceBadge.implicitWidth + 10, 140)
                        implicitHeight: sourceBadge.implicitHeight + 2
                        radius: height / 2
                        color: delegateItem.index === root.currentIndex
                               ? Qt.rgba(palette.highlightedText.r,
                                         palette.highlightedText.g,
                                         palette.highlightedText.b, 0.2)
                               : palette.alternateBase

                        Text {
                            id: sourceBadge

                            anchors.fill: parent
                            anchors.leftMargin: 5
                            anchors.rightMargin: 5
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: delegateItem.modelData.source || ""
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: delegateItem.index === root.currentIndex
                                   ? palette.highlightedText
                                   : palette.mid
                            font.pixelSize: 10
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: delegateItem.modelData.description
                    textFormat: Text.PlainText
                    color: delegateItem.index === root.currentIndex
                           ? Qt.rgba(palette.highlightedText.r,
                                     palette.highlightedText.g,
                                     palette.highlightedText.b, 0.7)
                           : palette.mid
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: hoverArea

                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.currentIndex = delegateItem.index
                    root.selectionRequested()
                }
                onEntered: root.currentIndex = delegateItem.index
            }
        }
    }
}
