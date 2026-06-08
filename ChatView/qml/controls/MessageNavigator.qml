// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Controls
import ChatView
import UIControls

Item {
    id: nav

    property var chatModel
    property var entries: []
    property color dotColor: "#92BD6C"
    property int currentMessageIndex: -1

    readonly property int dotCount: entries.length
    readonly property int verticalPadding: 8
    readonly property int minDotSpacing: 18
    readonly property real availableHeight: Math.max(0, height - 2 * verticalPadding)
    readonly property real naturalHeight: dotCount > 1 ? (dotCount - 1) * minDotSpacing : 0
    readonly property bool needsScrolling: naturalHeight > availableHeight
    readonly property real contentHeight: needsScrolling
        ? naturalHeight + 2 * verticalPadding
        : Math.max(height, 2 * verticalPadding)

    signal messageClicked(int messageIndex)

    implicitWidth: 16

    function rebuild() {
        entries = chatModel ? chatModel.userMessagePreviews(80) : []
        Qt.callLater(scrollCurrentIntoView)
    }

    function updateCurrentFromModelIndex(modelIdx) {
        if (modelIdx < 0) {
            currentMessageIndex = -1
            return
        }
        let best = -1
        for (let i = 0; i < entries.length; ++i) {
            const e = entries[i]
            if (!e)
                continue
            const mi = e.messageIndex
            if (mi <= modelIdx)
                best = mi
            else
                break
        }
        currentMessageIndex = best
    }

    function uiIndexOf(messageIndex) {
        for (let i = 0; i < entries.length; ++i) {
            const e = entries[i]
            if (e && e.messageIndex === messageIndex)
                return i
        }
        return -1
    }

    function dotCenterY(uiIndex) {
        const count = dotCount
        if (count <= 1)
            return contentHeight / 2
        const spacing = needsScrolling
            ? minDotSpacing
            : availableHeight / (count - 1)
        return verticalPadding + spacing * uiIndex
    }

    function scrollCurrentIntoView() {
        if (!needsScrolling || currentMessageIndex < 0)
            return
        const ui = uiIndexOf(currentMessageIndex)
        if (ui < 0)
            return
        const y = dotCenterY(ui)
        const margin = 24
        if (y < flick.contentY + margin)
            flick.contentY = Math.max(0, y - margin)
        else if (y > flick.contentY + flick.height - margin)
            flick.contentY = Math.min(
                Math.max(0, flick.contentHeight - flick.height),
                y - flick.height + margin)
    }

    onChatModelChanged: rebuild()
    onCurrentMessageIndexChanged: scrollCurrentIntoView()
    Component.onCompleted: rebuild()

    Connections {
        target: nav.chatModel
        ignoreUnknownSignals: true
        function onRowsInserted() { nav.rebuild() }
        function onRowsRemoved() { nav.rebuild() }
        function onModelReset() { nav.rebuild() }
        function onModelReseted() { nav.rebuild() }
        function onDataChanged() { nav.rebuild() }
    }

    Flickable {
        id: flick

        anchors.fill: parent
        contentWidth: width
        contentHeight: nav.contentHeight
        interactive: nav.needsScrolling
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Rectangle {
            id: spine

            visible: nav.dotCount > 1
            anchors.horizontalCenter: parent.horizontalCenter
            y: nav.verticalPadding
            width: 1
            height: Math.max(0, flick.contentHeight - 2 * nav.verticalPadding)
            color: palette.mid
            opacity: 0.4
        }

        Repeater {
            model: nav.entries

            delegate: Item {
                id: dotItem

                required property var modelData
                required property int index

                readonly property int msgIndex: modelData && modelData.messageIndex !== undefined
                                                ? modelData.messageIndex : -1
                readonly property string preview: modelData && modelData.preview !== undefined
                                                  ? modelData.preview : ""
                readonly property bool isCurrent: nav.currentMessageIndex === msgIndex

                width: 16
                height: 14
                anchors.horizontalCenter: parent.horizontalCenter
                y: nav.dotCenterY(index) - height / 2

                Rectangle {
                    id: dot

                    anchors.centerIn: parent
                    width: dotItem.isCurrent ? 11 : (dotArea.containsMouse ? 10 : 7)
                    height: width
                    radius: width / 2
                    color: dotArea.containsMouse
                           ? Qt.lighter(nav.dotColor, 1.2)
                           : nav.dotColor
                    border.color: dotItem.isCurrent
                                  ? Qt.darker(nav.dotColor, 1.7)
                                  : Qt.darker(nav.dotColor, 1.4)
                    border.width: dotItem.isCurrent ? 2 : 1
                    opacity: dotItem.isCurrent || dotArea.containsMouse ? 1.0 : 0.55

                    Behavior on width { NumberAnimation { duration: 120 } }
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    id: dotArea

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: nav.messageClicked(dotItem.msgIndex)

                    QoAToolTip {
                        visible: dotArea.containsMouse
                        delay: 350
                        text: dotItem.preview.length > 0
                              ? qsTr("#%1  ·  %2").arg(dotItem.index + 1).arg(dotItem.preview)
                              : qsTr("Jump to message #%1").arg(dotItem.index + 1)
                    }
                }
            }
        }
    }

}
