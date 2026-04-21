// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Qt.labs.platform as Platform

TextEdit {
    id: root

    readOnly: true
    selectByMouse: true
    wrapMode: Text.WordWrap
    selectionColor: palette.highlight
    color: palette.text

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: contextMenu.open()
        cursorShape: root.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor
    }

    Platform.Menu {
        id: contextMenu

        Platform.MenuItem {
            text: qsTr("Copy")
            enabled: root.selectedText.length > 0
            onTriggered: root.copy()
        }

        Platform.MenuItem {
            text: qsTr("Select All")
            enabled: root.text.length > 0
            onTriggered: root.selectAll()
        }
    }
}
