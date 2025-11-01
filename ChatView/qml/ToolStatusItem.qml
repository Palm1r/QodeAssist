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
    property bool expanded: false

    readonly property int firstNewline: toolContent.indexOf('\n')
    readonly property string toolName: firstNewline > 0 ? toolContent.substring(0, firstNewline) : toolContent
    readonly property string toolResult: firstNewline > 0 ? toolContent.substring(firstNewline + 1) : ""

    radius: 6
    color: palette.base
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    MouseArea {
        id: header

        width: parent.width
        height: headerRow.height + 10
        cursorShape: Qt.PointingHandCursor
        onClicked: root.expanded = !root.expanded

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
                text: root.expanded ? "▼" : "▶"
                font.pixelSize: 10
                color: palette.mid
            }
        }
    }

    Column {
        id: contentColumn

        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            margins: 10
        }
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
            text: root.expanded ? qsTr("Collapse") : qsTr("Expand")
            onTriggered: root.expanded = !root.expanded
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


    states: [
        State {
            when: !root.expanded
            PropertyChanges {
                target: root
                implicitHeight: header.height
            }
        },
        State {
            when: root.expanded
            PropertyChanges {
                target: root
                implicitHeight: header.height + contentColumn.height + 20
            }
        }
    ]
}
