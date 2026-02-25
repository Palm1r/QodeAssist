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

ToolTip {
    id: root

    padding: 8

    contentItem: Text {
        text: root.text
        font: root.font
        color: palette.toolTipText
        wrapMode: Text.Wrap
    }

    background: Item {
        implicitWidth: bg.implicitWidth
        implicitHeight: bg.implicitHeight

        Rectangle {
            anchors.fill: bg
            anchors.margins: -2
            color: Qt.rgba(palette.shadow.r, palette.shadow.g, palette.shadow.b, 0.12)
            radius: 8
            z: -2
        }

        Rectangle {
            anchors.fill: bg
            anchors.margins: -1
            color: Qt.rgba(palette.shadow.r, palette.shadow.g, palette.shadow.b, 0.08)
            radius: 7
            z: -1
        }

        Rectangle {
            id: bg
            anchors.fill: parent
            color: palette.toolTipBase
            border.color: Qt.darker(palette.toolTipBase, 1.2)
            border.width: 1
            radius: 6
        }
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 150
            easing.type: Easing.OutQuad
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 100
            easing.type: Easing.InQuad
        }
    }
}
