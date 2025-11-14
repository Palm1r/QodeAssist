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
    // property string signature: ""
    property bool isRedacted: false
    property bool expanded: false
    
    property alias headerOpacity: headerRow.opacity

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
                text: root.isRedacted ? qsTr("Thinking (Redacted)")
                                      : qsTr("Thinking")
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

        // Rectangle {
        //     visible: root.signature.length > 0 && root.expanded
        //     width: parent.width
        //     height: signatureText.height + 10
        //     color: palette.alternateBase
        //     radius: 4

        //     Text {
        //         id: signatureText

        //         anchors {
        //             left: parent.left
        //             right: parent.right
        //             verticalCenter: parent.verticalCenter
        //             margins: 5
        //         }
        //         text: qsTr("Signature: %1").arg(root.signature.substring(0, Math.min(40, root.signature.length)) + "...")
        //         font.pixelSize: 9
        //         font.family: "monospace"
        //         color: palette.mid
        //         elide: Text.ElideRight
        //     }
        // }
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
            text: root.expanded ? qsTr("Collapse") : qsTr("Expand")
            onTriggered: root.expanded = !root.expanded
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

