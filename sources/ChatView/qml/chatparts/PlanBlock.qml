// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Layouts
import "BlockPayload.js" as BlockPayload

Rectangle {
    id: root

    property string planContent: ""
    property bool expanded: true

    readonly property var planData: parsePlanData(planContent)
    readonly property var entries: planData === null ? [] : (planData.entries || [])
    readonly property int completedCount: countCompleted()
    readonly property bool isComplete: entries.length > 0 && completedCount === entries.length

    readonly property int borderRadius: 6
    readonly property int contentMargin: 10
    readonly property int rowSpacing: 6

    readonly property color completedColor: Qt.rgba(0.2, 0.8, 0.2, 0.9)
    readonly property color activeColor: palette.highlight
    readonly property color pendingColor: palette.placeholderText

    implicitHeight: layout.implicitHeight + 2 * contentMargin
    radius: borderRadius
    color: palette.base
    border.width: 1
    border.color: isComplete ? completedColor : palette.mid

    function parsePlanData(content) {
        return BlockPayload.parsePlan(content);
    }

    function countCompleted() {
        let done = 0;
        for (let i = 0; i < root.entries.length; ++i) {
            if (root.entries[i].status === "completed")
                ++done;
        }
        return done;
    }

    function entryColor(status) {
        if (status === "completed")
            return root.completedColor;
        if (status === "in_progress")
            return root.activeColor;
        return root.pendingColor;
    }

    function entryMarker(status) {
        if (status === "completed")
            return "✓";
        if (status === "in_progress")
            return "▶";
        return "○";
    }

    ColumnLayout {
        id: layout

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: root.contentMargin
        }
        spacing: root.rowSpacing

        MouseArea {
            Layout.fillWidth: true
            Layout.preferredHeight: header.implicitHeight
            cursorShape: Qt.PointingHandCursor
            onClicked: root.expanded = !root.expanded

            RowLayout {
                id: header

                anchors.fill: parent
                spacing: root.rowSpacing

                Text {
                    text: qsTr("Plan")
                    textFormat: Text.PlainText
                    font.pixelSize: 13
                    font.bold: true
                    color: palette.text
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 of %2 done").arg(root.completedCount).arg(root.entries.length)
                    textFormat: Text.PlainText
                    font.pixelSize: 11
                    color: palette.placeholderText
                }

                Text {
                    text: root.expanded ? "▼" : "▶"
                    font.pixelSize: 10
                    color: palette.mid
                }
            }
        }

        Repeater {
            model: root.expanded ? root.entries : []

            delegate: RowLayout {
                id: entryRow

                required property var modelData

                Layout.fillWidth: true
                spacing: root.rowSpacing

                Text {
                    Layout.alignment: Qt.AlignTop
                    text: root.entryMarker(entryRow.modelData.status)
                    textFormat: Text.PlainText
                    font.pixelSize: 12
                    color: root.entryColor(entryRow.modelData.status)
                }

                Text {
                    Layout.fillWidth: true
                    text: entryRow.modelData.content || ""
                    textFormat: Text.PlainText
                    font.pixelSize: 12
                    font.strikeout: entryRow.modelData.status === "completed"
                    color: entryRow.modelData.status === "completed" ? palette.placeholderText
                                                                    : palette.text
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.alignment: Qt.AlignTop
                    visible: entryRow.modelData.priority === "high"
                    text: qsTr("high")
                    textFormat: Text.PlainText
                    font.pixelSize: 10
                    color: palette.highlight
                }
            }
        }
    }
}
