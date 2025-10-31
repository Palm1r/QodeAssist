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
import QtQuick.Controls.Basic

Item {
    id: root

    property alias leftText: leftLabel.text
    property alias rightText: rightLabel.text

    property bool checked: false
    property bool enabled: true
    property bool hovered: mouseArea.containsMouse
    property int padding: 8

    signal toggled()

    readonly property real maxTextWidth: Math.max(leftLabel.implicitWidth, rightLabel.implicitWidth) + padding * 2
    readonly property real maxTextHeight: Math.max(leftLabel.implicitHeight, rightLabel.implicitHeight) + padding

    implicitWidth: maxTextWidth * 2
    implicitHeight: maxTextHeight

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.enabled ? palette.button : palette.mid
        border.color: root.enabled ? palette.mid : palette.midlight
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    Rectangle {
        id: slider

        x: root.checked ? parent.width / 2 : 0
        width: parent.width / 2
        height: parent.height
        opacity: 0.6
        radius: height / 2

        color: root.enabled
               ? (mouseArea.pressed ? palette.dark : palette.highlight)
               : palette.midlight

        border.color: root.enabled ? palette.highlight : palette.mid
        border.width: 1

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.alpha(palette.highlight, 0.4) }
                GradientStop { position: 1.0; color: Qt.alpha(palette.highlight, 0.2) }
            }
            opacity: root.hovered ? 0.3 : 0.01
            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        Behavior on x {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }

    Item {
        id: leftContainer

        width: parent.width / 2
        height: parent.height

        Text {
            id: leftLabel

            anchors.centerIn: parent
            text: root.leftText
            font.pixelSize: 14
            color: root.enabled
                   ? (!root.checked ? palette.highlightedText : palette.windowText)
                   : Qt.alpha(palette.windowText, 0.3)

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }
    }

    Item {
        id: rightContainer

        x: parent.width / 2
        width: parent.width / 2
        height: parent.height

        Text {
            id: rightLabel

            anchors.centerIn: parent
            text: root.rightText
            font.pixelSize: 14
            color: root.enabled
                   ? (root.checked ? palette.highlightedText : palette.windowText)
                   : Qt.alpha(palette.windowText, 0.3)

            Behavior on color {
                ColorAnimation { duration: 150 }
            }
        }
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled

        onClicked: {
            if (root.enabled) {
                root.checked = !root.checked
                root.toggled()
            }
        }
    }
}
