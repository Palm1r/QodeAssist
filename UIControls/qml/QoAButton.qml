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
import QtQuick.Controls.Basic

Button {
    id: control

    padding: 4

    icon.width: 16
    icon.height: 16

    contentItem.height: 20

    background: Rectangle {
        id: bg

        implicitHeight: 20

        color: !control.enabled || !control.down ? control.palette.button : control.palette.dark
        border.color: !control.enabled || (!control.hovered && !control.visualFocus) ? control.palette.mid : control.palette.highlight
        border.width: 1
        radius: 4

        Rectangle {
            anchors.fill: bg
            radius: bg.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.alpha(control.palette.highlight, 0.4) }
                GradientStop { position: 1.0; color: Qt.alpha(control.palette.highlight, 0.2) }
            }
            opacity: control.hovered ? 0.3 : 0.01
            Behavior on opacity {NumberAnimation{duration: 250}}
        }
    }
}
