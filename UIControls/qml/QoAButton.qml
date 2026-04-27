// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
