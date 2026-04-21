// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Rectangle {
    id: root

    property alias toastTextItem: textItem
    property alias toastTextColor: textItem.color

    property string errorText: ""
    property int displayDuration: 7000

    width: Math.min(parent.width - 40, textItem.implicitWidth + radius)
    height: visible ? (textItem.implicitHeight + 12) : 0
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.top: parent.top
    anchors.topMargin: 10

    color: "#d32f2f"
    radius: height / 2
    border.color: "#b71c1c"
    border.width: 1
    visible: false
    opacity: 0

    TextEdit {
        id: textItem

        anchors.centerIn: parent
        anchors.margins: 6
        text: root.errorText
        color: palette.text
        font.pixelSize: 13
        wrapMode: TextEdit.Wrap
        width: Math.min(implicitWidth, root.parent.width - 60)
        horizontalAlignment: TextEdit.AlignHCenter
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        selectionColor: "#b71c1c"
    }

    function show(message) {
        errorText = message
        visible = true
        showAnimation.start()
        hideTimer.restart()
    }

    function hide() {
        hideAnimation.start()
    }

    NumberAnimation {
        id: showAnimation

        target: root
        property: "opacity"
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutQuad
    }

    NumberAnimation {
        id: hideAnimation

        target: root
        property: "opacity"
        from: 1
        to: 0
        duration: 200
        easing.type: Easing.InQuad
        onFinished: root.visible = false
    }

    Timer {
        id: hideTimer

        interval: root.displayDuration
        running: false
        repeat: false
        onTriggered: root.hide()
    }
}
