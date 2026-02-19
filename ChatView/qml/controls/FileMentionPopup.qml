/*
 * Copyright (C) 2026 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var searchResults: []
    property int currentIndex: 0

    signal fileSelected(string absolutePath, string relativePath, string projectName)
    signal projectSelected(string projectName)
    signal dismissed()

    visible: searchResults.length > 0
    height: Math.min(searchResults.length * 36, 36 * 6) + 2
    color: palette.window
    border.color: palette.mid
    border.width: 1
    radius: 4

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

            readonly property bool isProject: modelData.isProject === true
            readonly property bool isOpen: modelData.isOpen === true
            readonly property string fileName: {
                if (isProject)
                    return modelData.projectName
                const parts = modelData.relativePath.split('/')
                return parts[parts.length - 1]
            }

            width: listView.width
            height: 36
            color: index === root.currentIndex
                   ? palette.highlight
                   : (hoverArea.containsMouse
                      ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.25)
                      : "transparent")

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Item {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18

                    Rectangle {
                        anchors.fill: parent
                        radius: 3
                        visible: delegateItem.isProject || delegateItem.isOpen

                        color: {
                            if (delegateItem.index === root.currentIndex)
                                return Qt.rgba(palette.highlightedText.r,
                                               palette.highlightedText.g,
                                               palette.highlightedText.b, 0.2)
                            if (delegateItem.isProject)
                                return Qt.rgba(palette.highlight.r,
                                               palette.highlight.g,
                                               palette.highlight.b, 0.3)
                            return Qt.rgba(0.2, 0.7, 0.4, 0.3)
                        }

                        Text {
                            anchors.centerIn: parent
                            text: delegateItem.isProject ? "P" : "O"
                            font.bold: true
                            font.pixelSize: 10
                            color: {
                                if (delegateItem.index === root.currentIndex)
                                    return palette.highlightedText
                                if (delegateItem.isProject)
                                    return palette.highlight
                                return Qt.rgba(0.1, 0.6, 0.3, 1.0)
                            }
                        }
                    }
                }

                Text {
                    Layout.preferredWidth: 160
                    text: delegateItem.fileName
                    color: delegateItem.index === root.currentIndex
                           ? palette.highlightedText
                           : (delegateItem.isProject ? palette.highlight : palette.text)
                    font.bold: true
                    font.italic: delegateItem.isProject
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: delegateItem.isProject
                          ? "→"
                          : (delegateItem.modelData.projectName + " / " + delegateItem.modelData.relativePath)
                    color: delegateItem.index === root.currentIndex
                           ? (delegateItem.isProject
                              ? palette.highlightedText
                              : Qt.rgba(palette.highlightedText.r,
                                        palette.highlightedText.g,
                                        palette.highlightedText.b, 0.7))
                           : palette.mid
                    font.pixelSize: delegateItem.isProject ? 12 : 11
                    elide: Text.ElideLeft
                    horizontalAlignment: delegateItem.isProject ? Text.AlignLeft : Text.AlignRight
                }
            }

            MouseArea {
                id: hoverArea

                anchors.fill: parent
                hoverEnabled: true
                onClicked: handleSelection(delegateItem.modelData)
                onEntered: root.currentIndex = delegateItem.index
            }
        }
    }

    function handleSelection(item) {
        if (item.isProject === true) {
            root.projectSelected(item.projectName)
        } else {
            root.fileSelected(item.absolutePath, item.relativePath, item.projectName)
        }
    }

    function selectCurrent() {
        if (currentIndex >= 0 && currentIndex < searchResults.length)
            handleSelection(searchResults[currentIndex])
    }

    function moveDown() {
        if (currentIndex < searchResults.length - 1)
            currentIndex++
        listView.positionViewAtIndex(currentIndex, ListView.Contain)
    }

    function moveUp() {
        if (currentIndex > 0)
            currentIndex--
        listView.positionViewAtIndex(currentIndex, ListView.Contain)
    }
}
