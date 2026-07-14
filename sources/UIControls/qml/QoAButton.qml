// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property color accentColor: "transparent"

    focusPolicy: Qt.NoFocus
    padding: 4

    icon.width: 16
    icon.height: 16

    contentItem.height: 20

    background: Rectangle {
        id: bg

        readonly property bool hasAccent: control.accentColor.a > 0

        implicitHeight: 20

        color: !control.enabled || !control.down ? control.palette.button : control.palette.dark
        border.color: bg.hasAccent
                      ? control.accentColor
                      : (!control.enabled || (!control.hovered && !control.visualFocus) ? control.palette.mid : control.palette.highlight)
        border.width: bg.hasAccent ? 2 : 1
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

        Rectangle {
            anchors.fill: bg
            radius: bg.radius
            color: control.accentColor
            visible: bg.hasAccent
            opacity: 0.15
        }
    }
}
