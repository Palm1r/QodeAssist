// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import ChatView

Item {
    id: nav

    property var chatModel
    property var userIndices: []
    property int hoveredDotIndex: -1
    property color dotColor: "#92BD6C"

    signal messageClicked(int messageIndex)

    implicitWidth: 16

    function rebuild() {
        if (chatModel)
            userIndices = chatModel.userMessageIndices()
        else
            userIndices = []
    }

    onChatModelChanged: rebuild()
    Component.onCompleted: rebuild()

    Connections {
        target: nav.chatModel
        ignoreUnknownSignals: true
        function onRowsInserted() { nav.rebuild() }
        function onRowsRemoved() { nav.rebuild() }
        function onModelReset() { nav.rebuild() }
        function onModelReseted() { nav.rebuild() }
        function onLayoutChanged() { nav.rebuild() }
    }

    Rectangle {
        id: spine

        visible: nav.userIndices.length > 1
        x: nav.width / 2 - width / 2
        y: 14
        width: 1
        height: Math.max(0, nav.height - 28)
        color: palette.mid
        opacity: 0.4
    }

    Repeater {
        model: nav.userIndices

        delegate: Item {
            id: dotItem

            required property var modelData
            required property int index

            width: nav.width
            height: 14
            x: 0
            y: {
                const count = nav.userIndices.length
                const dotH = height
                if (count <= 1)
                    return (nav.height - dotH) / 2
                const top = 7
                const bottom = nav.height - dotH - 7
                return top + (bottom - top) * index / (count - 1)
            }

            Rectangle {
                id: dot
                anchors.centerIn: parent
                width: dotArea.containsMouse ? 10 : 7
                height: width
                radius: width / 2
                color: dotArea.containsMouse ? Qt.lighter(nav.dotColor, 1.15) : nav.dotColor
                border.color: Qt.darker(nav.dotColor, 1.4)
                border.width: 1
                opacity: dotArea.containsMouse ? 1.0 : 0.9

                Behavior on width { NumberAnimation { duration: 120 } }
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            MouseArea {
                id: dotArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: nav.messageClicked(dotItem.modelData)

                ToolTip.visible: containsMouse
                ToolTip.delay: 400
                ToolTip.text: qsTr("Jump to message #%1").arg(dotItem.index + 1)
            }
        }
    }
}
