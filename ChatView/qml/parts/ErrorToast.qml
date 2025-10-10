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

Rectangle {
    id: errorToast

    property string errorText: ""
    property int displayDuration: 5000

    width: Math.min(parent.width - 40, errorTextItem.implicitWidth + radius)
    height: visible ? (errorTextItem.implicitHeight + 12) : 0
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
        id: errorTextItem

        anchors.centerIn: parent
        anchors.margins: 6
        text: errorToast.errorText
        color: "#ffffff"
        font.pixelSize: 13
        wrapMode: TextEdit.Wrap
        width: Math.min(implicitWidth, errorToast.parent.width - 60)
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

        target: errorToast
        property: "opacity"
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutQuad
    }

    NumberAnimation {
        id: hideAnimation

        target: errorToast
        property: "opacity"
        from: 1
        to: 0
        duration: 200
        easing.type: Easing.InQuad
        onFinished: errorToast.visible = false
    }

    Timer {
        id: hideTimer

        interval: errorToast.displayDuration
        running: false
        repeat: false
        onTriggered: errorToast.hide()
    }
}
