/*
 * Copyright (C) 2025 Petr Mironychev
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
import Qt.labs.platform as Platform

Rectangle {
    id: root

    property string thinkingContent: ""
    property bool isRedacted: false

    enum DisplayMode { Collapsed, Compact, Expanded }
    property int displayMode: ThinkingBlock.DisplayMode.Compact
    property int compactHeight: 120

    property alias headerOpacity: headerRow.opacity

    radius: 6
    color: palette.base
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    implicitHeight: {
        if (displayMode === ThinkingBlock.DisplayMode.Collapsed) {
            return header.height
        } else if (displayMode === ThinkingBlock.DisplayMode.Compact) {
            let fullHeight = header.height + contentColumn.height + 20
            return Math.min(fullHeight, header.height + compactHeight)
        } else {
            return header.height + contentColumn.height + 20
        }
    }

    MouseArea {
        id: header

        width: parent.width
        height: headerRow.height + 10
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (root.displayMode === ThinkingBlock.DisplayMode.Collapsed) {
                root.displayMode = ThinkingBlock.DisplayMode.Compact
            } else if (root.displayMode === ThinkingBlock.DisplayMode.Compact) {
                root.displayMode = ThinkingBlock.DisplayMode.Collapsed
            } else {
                root.displayMode = ThinkingBlock.DisplayMode.Compact
            }
        }

        Row {
            id: headerRow

            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 10
            }
            width: parent.width
            spacing: 8

            Text {
                text: root.isRedacted ? qsTr("Thinking (Redacted)")
                                      : qsTr("Thinking")
                font.pixelSize: 13
                font.bold: true
                color: palette.text
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.displayMode === ThinkingBlock.DisplayMode.Collapsed ? "▶" : "▼"
                font.pixelSize: 10
                color: palette.mid
            }
        }
    }

    Item {
        id: contentWrapper

        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            margins: 10
            bottomMargin: expandButton.visible ? expandButton.height + 15 : 10
        }
        clip: true
        visible: root.displayMode !== ThinkingBlock.DisplayMode.Collapsed

        Column {
            id: contentColumn

            width: parent.width
            spacing: 8

            Text {
                visible: root.isRedacted
                width: parent.width
                text: qsTr("Thinking content was redacted by safety systems")
                font.pixelSize: 11
                font.italic: true
                color: Qt.rgba(0.8, 0.4, 0.4, 1.0)
                wrapMode: Text.WordWrap
            }

            TextEdit {
                id: thinkingText

                visible: !root.isRedacted
                width: parent.width
                text: root.thinkingContent
                readOnly: true
                selectByMouse: true
                color: palette.text
                wrapMode: Text.WordWrap
                font.family: "monospace"
                font.pixelSize: 11
                selectionColor: palette.highlight
            }
        }
    }

    Rectangle {
        id: expandButton

        property bool needsExpand: contentColumn.height > compactHeight - 20

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            bottomMargin: 5
            leftMargin: 10
            rightMargin: 10
        }
        height: 24
        radius: 4
        color: palette.button
        visible: needsExpand && root.displayMode !== ThinkingBlock.DisplayMode.Collapsed

        Text {
            anchors.centerIn: parent
            text: root.displayMode === ThinkingBlock.DisplayMode.Expanded ? qsTr("▲ Show less") : qsTr("▼ Show more")
            font.pixelSize: 11
            color: palette.buttonText
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.displayMode === ThinkingBlock.DisplayMode.Expanded) {
                    root.displayMode = ThinkingBlock.DisplayMode.Compact
                } else {
                    root.displayMode = ThinkingBlock.DisplayMode.Expanded
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: contextMenu.open()
        propagateComposedEvents: true
    }

    Platform.Menu {
        id: contextMenu

        Platform.MenuItem {
            text: root.displayMode === ThinkingBlock.DisplayMode.Collapsed ? qsTr("Expand") : qsTr("Collapse")
            onTriggered: {
                if (root.displayMode === ThinkingBlock.DisplayMode.Collapsed) {
                    root.displayMode = ThinkingBlock.DisplayMode.Compact
                } else {
                    root.displayMode = ThinkingBlock.DisplayMode.Collapsed
                }
            }
        }

        Platform.MenuItem {
            text: root.displayMode === ThinkingBlock.DisplayMode.Expanded ? qsTr("Compact view") : qsTr("Full view")
            enabled: root.displayMode !== ThinkingBlock.DisplayMode.Collapsed
            onTriggered: {
                if (root.displayMode === ThinkingBlock.DisplayMode.Expanded) {
                    root.displayMode = ThinkingBlock.DisplayMode.Compact
                } else {
                    root.displayMode = ThinkingBlock.DisplayMode.Expanded
                }
            }
        }
    }

    Rectangle {
        id: thinkingMarker

        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: root.height - root.radius
        color: root.isRedacted ? Qt.rgba(0.8, 0.3, 0.3, 0.9)
                               : (root.color.hslLightness > 0.5 ? Qt.darker(palette.alternateBase, 1.3)
                                                                : Qt.lighter(palette.alternateBase, 1.3))
        radius: root.radius
    }
}
