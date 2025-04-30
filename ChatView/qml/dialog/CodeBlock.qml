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
import QtQuick.Controls
import ChatView

Rectangle {
    id: root

    property string code: ""
    property string language: ""

    property real currentContentY: 0
    property real blockStart: 0

    property alias codeFontFamily: codeText.font.family
    property alias codeFontSize: codeText.font.pointSize

    readonly property real buttonTopMargin: 5
    readonly property real blockEnd: blockStart + root.height
    readonly property real maxButtonOffset: Math.max(0, root.height - copyButton.height - buttonTopMargin)

    readonly property real buttonPosition: {
        if (currentContentY > blockEnd) {
            return buttonTopMargin;
        }
        else if (currentContentY > blockStart) {
            let offset = currentContentY - blockStart;
            return Math.min(offset, maxButtonOffset);
        }
        return buttonTopMargin;
    }

    color: palette.alternateBase
    border.color: root.color.hslLightness > 0.5 ? Qt.darker(root.color, 1.3)
                                                : Qt.lighter(root.color, 1.3)
    border.width: 2
    radius: 4
    implicitWidth: parent.width
    implicitHeight: codeText.implicitHeight + 20

    ChatUtils {
        id: utils
    }

    TextEdit {
        id: codeText
        anchors.fill: parent
        anchors.margins: 10
        text: root.code
        readOnly: true
        selectByMouse: true
        color: parent.color.hslLightness > 0.5 ? "black" : "white"
        wrapMode: Text.WordWrap
        selectionColor: palette.highlight
    }

    TextEdit {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 5
        readOnly: true
        selectByMouse: true
        text: root.language
        color: root.color.hslLightness > 0.5 ? Qt.darker(root.color, 1.1)
                                             : Qt.lighter(root.color, 1.1)
        font.pointSize: codeText.font.pointSize - 4
    }

    QoAButton {
        id: copyButton

        anchors {
            top: parent.top
            topMargin: root.buttonPosition
            right: parent.right
            rightMargin: root.buttonTopMargin
        }

        text: qsTr("Copy")
        onClicked: {
            utils.copyToClipboard(root.code)
            text = qsTr("Copied")
            copyTimer.start()
        }

        Timer {
            id: copyTimer
            interval: 2000
            onTriggered: parent.text = qsTr("Copy")
        }
    }
}
