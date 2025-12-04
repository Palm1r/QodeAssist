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

    property string toolContent: ""

    enum DisplayMode { Collapsed, Compact, Expanded }
    property int displayMode: ToolBlock.DisplayMode.Compact
    property int compactHeight: 120

    property alias headerOpacity: headerRow.opacity

    readonly property int firstNewline: toolContent.indexOf('\n')
    readonly property string toolName: firstNewline > 0 ? toolContent.substring(0, firstNewline) : toolContent
    readonly property string toolResult: firstNewline > 0 ? toolContent.substring(firstNewline + 1) : ""

    radius: 6
    color: palette.base
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    implicitHeight: {
        if (displayMode === ToolBlock.DisplayMode.Collapsed) {
            return header.height
        } else if (displayMode === ToolBlock.DisplayMode.Compact) {
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
            if (root.displayMode === ToolBlock.DisplayMode.Collapsed) {
                root.displayMode = ToolBlock.DisplayMode.Compact
            } else if (root.displayMode === ToolBlock.DisplayMode.Compact) {
                root.displayMode = ToolBlock.DisplayMode.Collapsed
            } else {
                root.displayMode = ToolBlock.DisplayMode.Compact
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
                text: qsTr("Tool: %1").arg(root.toolName)
                font.pixelSize: 13
                font.bold: true
                color: palette.text
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.displayMode === ToolBlock.DisplayMode.Collapsed ? "▶" : "▼"
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
        visible: root.displayMode !== ToolBlock.DisplayMode.Collapsed

        Column {
            id: contentColumn

            width: parent.width
            spacing: 8

            TextEdit {
                id: resultText

                width: parent.width
                text: root.toolResult
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
        visible: needsExpand && root.displayMode !== ToolBlock.DisplayMode.Collapsed

        Text {
            anchors.centerIn: parent
            text: root.displayMode === ToolBlock.DisplayMode.Expanded ? qsTr("▲ Show less") : qsTr("▼ Show more")
            font.pixelSize: 11
            color: palette.buttonText
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.displayMode === ToolBlock.DisplayMode.Expanded) {
                    root.displayMode = ToolBlock.DisplayMode.Compact
                } else {
                    root.displayMode = ToolBlock.DisplayMode.Expanded
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
            text: qsTr("Copy")
            enabled: resultText.selectedText.length > 0
            onTriggered: resultText.copy()
        }

        Platform.MenuItem {
            text: qsTr("Select All")
            enabled: resultText.text.length > 0
            onTriggered: resultText.selectAll()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: root.displayMode === ToolBlock.DisplayMode.Collapsed ? qsTr("Expand") : qsTr("Collapse")
            onTriggered: {
                if (root.displayMode === ToolBlock.DisplayMode.Collapsed) {
                    root.displayMode = ToolBlock.DisplayMode.Compact
                } else {
                    root.displayMode = ToolBlock.DisplayMode.Collapsed
                }
            }
        }

        Platform.MenuItem {
            text: root.displayMode === ToolBlock.DisplayMode.Expanded ? qsTr("Compact view") : qsTr("Full view")
            enabled: root.displayMode !== ToolBlock.DisplayMode.Collapsed
            onTriggered: {
                if (root.displayMode === ToolBlock.DisplayMode.Expanded) {
                    root.displayMode = ToolBlock.DisplayMode.Compact
                } else {
                    root.displayMode = ToolBlock.DisplayMode.Expanded
                }
            }
        }
    }

    Rectangle {
        id: messageMarker

        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: root.height - root.radius
        color: root.color.hslLightness > 0.5 ? Qt.darker(palette.alternateBase, 1.3)
                                             : Qt.lighter(palette.alternateBase, 1.3)
        radius: root.radius
    }
}
