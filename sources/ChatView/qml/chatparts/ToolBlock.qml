// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import Qt.labs.platform as Platform

Rectangle {
    id: root

    property string toolContent: ""
    property string toolKind: ""
    property string toolStatus: ""
    property var toolDetails: ({})
    property bool expanded: false

    property alias headerOpacity: headerRow.opacity

    readonly property int firstNewline: toolContent.indexOf('\n')
    readonly property string toolName: firstNewline > 0 ? toolContent.substring(0, firstNewline) : toolContent
    readonly property string toolResult: firstNewline > 0 ? toolContent.substring(firstNewline + 1) : ""

    readonly property var locations: (toolDetails && toolDetails.locations) ? toolDetails.locations : []
    readonly property var diffs: (toolDetails && toolDetails.diffs) ? toolDetails.diffs : []

    readonly property bool isRunning: toolStatus === "pending" || toolStatus === "in_progress"
    readonly property bool hasFailed: toolStatus === "failed"

    readonly property color statusColor: {
        if (hasFailed)
            return Qt.rgba(0.8, 0.2, 0.2, 0.9);
        if (isRunning)
            return palette.highlight;
        if (toolStatus === "completed")
            return Qt.rgba(0.2, 0.8, 0.2, 0.9);
        return palette.mid;
    }

    readonly property string statusMarker: {
        if (hasFailed)
            return "✗";
        if (toolStatus === "completed")
            return "✓";
        if (isRunning)
            return "…";
        return "";
    }

    radius: 6
    color: palette.base
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    MouseArea {
        id: header

        width: parent.width
        height: headerRow.height + 10
        cursorShape: Qt.PointingHandCursor
        onClicked: root.expanded = !root.expanded

        Row {
            id: headerRow

            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
                leftMargin: 10
            }
            width: parent.width
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.statusMarker
                textFormat: Text.PlainText
                visible: text.length > 0
                font.pixelSize: 12
                color: root.statusColor
            }

            Text {
                id: headerTitle

                width: headerRow.width - x - 30
                text: root.toolKind.length > 0
                          ? root.toolKind + ": " + root.toolName
                          : qsTr("Tool: %1").arg(root.toolName)
                textFormat: Text.PlainText
                font.pixelSize: 13
                font.bold: true
                color: root.hasFailed ? root.statusColor : palette.text
                elide: Text.ElideRight
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.expanded ? "▼" : "▶"
                font.pixelSize: 10
                color: palette.mid
            }
        }
    }

    Loader {
        id: contentLoader

        anchors {
            left: parent.left
            right: parent.right
            top: header.bottom
            margins: 10
        }
        active: root.expanded
        sourceComponent: contentComponent
    }

    Component {
        id: contentComponent

        Column {
            property alias resultEditor: resultText

            spacing: 8

            Repeater {
                model: root.locations

                delegate: Text {
                    id: locationEntry

                    required property var modelData

                    width: contentLoader.width
                    text: locationEntry.modelData.line !== undefined
                              ? "→ " + locationEntry.modelData.path + ":" + locationEntry.modelData.line
                              : "→ " + locationEntry.modelData.path
                    textFormat: Text.PlainText
                    color: palette.placeholderText
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
            }

            Repeater {
                model: root.diffs

                delegate: Column {
                    id: diffEntry

                    required property var modelData

                    width: contentLoader.width
                    spacing: 2

                    Text {
                        width: parent.width
                        text: qsTr("Diff") + ": " + (diffEntry.modelData.path || "")
                        textFormat: Text.PlainText
                        color: palette.text
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideMiddle
                    }

                    TextEdit {
                        width: parent.width
                        text: (diffEntry.modelData.oldText || "") + "\n" + (diffEntry.modelData.newText || "")
                        textFormat: TextEdit.PlainText
                        readOnly: true
                        selectByMouse: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                        font.family: "monospace"
                        font.pixelSize: 11
                        selectionColor: palette.highlight
                    }
                }
            }

            TextEdit {
                id: resultText

                width: parent.width
                visible: text.length > 0
                text: root.toolResult
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                color: palette.text
                wrapMode: Text.WordWrap
                font.family: "monospace"
                font.pixelSize: 11
                selectionColor: palette.highlight
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: contextMenu.open()
        propagateComposedEvents: true
    }

    Platform.Menu {
        id: contextMenu

        Platform.MenuItem {
            text: qsTr("Copy")
            enabled: contentLoader.item && contentLoader.item.resultEditor.selectedText.length > 0
            onTriggered: contentLoader.item.resultEditor.copy()
        }

        Platform.MenuItem {
            text: qsTr("Select All")
            enabled: contentLoader.item && contentLoader.item.resultEditor.text.length > 0
            onTriggered: contentLoader.item.resultEditor.selectAll()
        }

        Platform.MenuSeparator {}

        Platform.MenuItem {
            text: root.expanded ? qsTr("Collapse") : qsTr("Expand")
            onTriggered: root.expanded = !root.expanded
        }
    }

    Rectangle {
        id: messageMarker

        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: root.height - root.radius
        color: root.color.hslLightness > 0.5 ? Qt.darker(palette.alternateBase, 1.3)
                                             : Qt.lighter(palette.alternateBase, 1.3)
        radius: root.radius
    }


    states: [
        State {
            when: !root.expanded
            PropertyChanges {
                target: root
                implicitHeight: header.height
            }
        },
        State {
            when: root.expanded
            PropertyChanges {
                target: root
                implicitHeight: header.height + contentLoader.height + 20
            }
        }
    ]
}
