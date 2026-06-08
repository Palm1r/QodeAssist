// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property alias text: badgeText.text
    property alias hovered: mouse.hovered

    QoAToolTip {
        visible: root.hovered && root.ToolTip.text.length > 0
        text: root.ToolTip.text
        delay: root.ToolTip.delay
    }

    implicitWidth: badgeText.implicitWidth + root.radius
    implicitHeight: badgeText.implicitHeight + 6
    color: palette.button
    radius: root.height / 2
    border.color: palette.mid
    border.width: 1

    Text {
        id: badgeText

        anchors.centerIn: parent
        color: palette.buttonText
    }

    HoverHandler {
        id: mouse

        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    }
}
