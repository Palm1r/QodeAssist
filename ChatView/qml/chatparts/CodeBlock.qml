/*
 * Copyright (C) 2024-2025 Petr Mironychev
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
import ChatView
import UIControls
import Qt.labs.platform as Platform

Rectangle {
    id: root

    property string code: ""
    property string language: ""

    enum DisplayMode { Collapsed, Compact, Expanded }
    property int displayMode: CodeBlock.DisplayMode.Compact
    property int compactHeight: 150

    property alias codeFontFamily: codeText.font.family
    property alias codeFontSize: codeText.font.pointSize
    readonly property real headerHeight: copyButton.height + 10

    color: palette.alternateBase
    border.color: root.color.hslLightness > 0.5 ? Qt.darker(root.color, 1.3)
                                                : Qt.lighter(root.color, 1.3)
    border.width: 2
    radius: 4
    implicitWidth: parent.width
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    implicitHeight: {
        if (displayMode === CodeBlock.DisplayMode.Collapsed) {
            return headerHeight
        } else if (displayMode === CodeBlock.DisplayMode.Compact) {
            let fullHeight = headerHeight + codeText.implicitHeight + 20
            return Math.min(fullHeight, headerHeight + compactHeight)
        } else {
            return headerHeight + codeText.implicitHeight + 20
        }
    }

    ChatUtils {
        id: utils
    }

    HoverHandler {
        id: hoverHandler
        enabled: true
    }

    MouseArea {
        id: header

        width: parent.width
        height: root.headerHeight
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (root.displayMode === CodeBlock.DisplayMode.Collapsed) {
                root.displayMode = CodeBlock.DisplayMode.Compact
            } else if (root.displayMode === CodeBlock.DisplayMode.Compact) {
                root.displayMode = CodeBlock.DisplayMode.Collapsed
            } else {
                root.displayMode = CodeBlock.DisplayMode.Compact
            }
        }

        Row {
            id: headerRow

            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 10
            }
            spacing: 6

            Text {
                text: root.language ? qsTr("Code (%1)").arg(root.language) :
                                      qsTr("Code")
                font.pixelSize: 12
                font.bold: true
                color: palette.text
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.displayMode === CodeBlock.DisplayMode.Collapsed ? "▶" : "▼"
                font.pixelSize: 10
                color: palette.mid
            }
        }
    }

    Item {
        id: codeWrapper

        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            margins: 10
            bottomMargin: expandButton.visible ? expandButton.height + 15 : 10
        }
        clip: true
        visible: root.displayMode !== CodeBlock.DisplayMode.Collapsed

        TextEdit {
            id: codeText

            width: parent.width
            text: root.code
            readOnly: true
            selectByMouse: true
            color: root.color.hslLightness > 0.5 ? "black" : "white"
            wrapMode: Text.WordWrap
            selectionColor: palette.highlight

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: contextMenu.open()
            }
        }
    }

    Rectangle {
        id: expandButton

        property bool needsExpand: codeText.implicitHeight > compactHeight - 20

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
        visible: needsExpand && root.displayMode !== CodeBlock.DisplayMode.Collapsed

        Text {
            anchors.centerIn: parent
            text: root.displayMode === CodeBlock.DisplayMode.Expanded ? qsTr("▲ Show less") : qsTr("▼ Show more")
            font.pixelSize: 11
            color: palette.buttonText
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.displayMode === CodeBlock.DisplayMode.Expanded) {
                    root.displayMode = CodeBlock.DisplayMode.Compact
                } else {
                    root.displayMode = CodeBlock.DisplayMode.Expanded
                }
            }
        }
    }

    Platform.Menu {
        id: contextMenu

        Platform.MenuItem {
            text: qsTr("Copy")
            onTriggered: {
                const textToCopy = codeText.selectedText || root.code
                utils.copyToClipboard(textToCopy)
            }
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: root.displayMode === CodeBlock.DisplayMode.Collapsed ? qsTr("Expand") : qsTr("Collapse")
            onTriggered: {
                if (root.displayMode === CodeBlock.DisplayMode.Collapsed) {
                    root.displayMode = CodeBlock.DisplayMode.Compact
                } else {
                    root.displayMode = CodeBlock.DisplayMode.Collapsed
                }
            }
        }

        Platform.MenuItem {
            text: root.displayMode === CodeBlock.DisplayMode.Expanded ? qsTr("Compact view") : qsTr("Full view")
            enabled: root.displayMode !== CodeBlock.DisplayMode.Collapsed
            onTriggered: {
                if (root.displayMode === CodeBlock.DisplayMode.Expanded) {
                    root.displayMode = CodeBlock.DisplayMode.Compact
                } else {
                    root.displayMode = CodeBlock.DisplayMode.Expanded
                }
            }
        }
    }

    QoAButton {
        id: copyButton

        anchors.right: parent.right
        anchors.rightMargin: 5

        y: 5
        text: qsTr("Copy")

        onClicked: {
            utils.copyToClipboard(root.code)
            text = qsTr("Copied")
            copyTimer.start()
        }

        Timer {
            id: copyTimer
            interval: 2000
            onTriggered: parent.text = qsTr("Copy")
        }
    }
}
