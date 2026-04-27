// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property alias text: label.text
    property bool active: false

    visible: active
    color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.75)

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Column {
        anchors.centerIn: parent
        spacing: 10

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.active
            implicitWidth: 36
            implicitHeight: 36
        }

        Text {
            id: label

            anchors.horizontalCenter: parent.horizontalCenter
            color: palette.text
            font.pixelSize: 13
        }
    }
}
