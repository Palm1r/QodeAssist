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
import QtQuick.Controls
import QtQuick.Layouts
import UIControls

Rectangle {
    id: root

    property string editContent: ""

    readonly property var editData: parseEditData(editContent)
    readonly property string filePath: editData.file || ""
    readonly property string fileName: getFileName(filePath)
    readonly property string editStatus: editData.status || "pending"
    readonly property string statusMessage: editData.status_message || ""
    readonly property string oldContent: editData.old_content || ""
    readonly property string newContent: editData.new_content || ""

    signal applyEdit(string editId)
    signal rejectEdit(string editId)
    signal undoEdit(string editId)
    signal openInEditor(string editId)

    readonly property int borderRadius: 4
    readonly property int contentMargin: 10
    readonly property int contentBottomPadding: 20
    readonly property int headerPadding: 8
    readonly property int statusIndicatorWidth: 4

    readonly property bool isPending: editStatus === "pending"
    readonly property bool isApplied: editStatus === "applied"
    readonly property bool isRejected: editStatus === "rejected"
    readonly property bool isArchived: editStatus === "archived"

    readonly property color appliedColor: Qt.rgba(0.2, 0.8, 0.2, 0.8)
    readonly property color revertedColor: Qt.rgba(0.8, 0.6, 0.2, 0.8)
    readonly property color rejectedColor: Qt.rgba(0.8, 0.2, 0.2, 0.8)
    readonly property color archivedColor: Qt.rgba(0.5, 0.5, 0.5, 0.8)
    readonly property color pendingColor: palette.highlight

    readonly property color appliedBgColor: Qt.rgba(0.2, 0.8, 0.2, 0.3)
    readonly property color revertedBgColor: Qt.rgba(0.8, 0.6, 0.2, 0.3)
    readonly property color rejectedBgColor: Qt.rgba(0.8, 0.2, 0.2, 0.3)
    readonly property color archivedBgColor: Qt.rgba(0.5, 0.5, 0.5, 0.3)

    readonly property string codeFontFamily: {
        switch (Qt.platform.os) {
        case "windows": return "Consolas"
        case "osx": return "Menlo"
        case "linux": return "DejaVu Sans Mono"
        default: return "monospace"
        }
    }
    readonly property int codeFontSize: Qt.application.font.pointSize

    readonly property color statusColor: {
        if (isArchived) return archivedColor
        if (isApplied) return appliedColor
        if (isRejected) return rejectedColor
        return pendingColor
    }

    readonly property color statusBgColor: {
        if (isArchived) return archivedBgColor
        if (isApplied) return appliedBgColor
        if (isRejected) return rejectedBgColor
        return palette.button
    }

    readonly property string statusText: {
        if (isArchived) return qsTr("ARCHIVED")
        if (isApplied) return qsTr("APPLIED")
        if (isRejected) return qsTr("REJECTED")
        return qsTr("PENDING")
    }

    readonly property int addedLines: countLines(newContent)
    readonly property int removedLines: countLines(oldContent)

    function parseEditData(content) {
        try {
            const marker = "QODEASSIST_FILE_EDIT:";
            let jsonStr = content;
            if (content.indexOf(marker) >= 0) {
                jsonStr = content.substring(content.indexOf(marker) + marker.length);
            }
            return JSON.parse(jsonStr);
        } catch (e) {
            return {
                edit_id: "",
                file: "",
                old_content: "",
                new_content: "",
                status: "error",
                status_message: ""
            };
        }
    }

    function getFileName(path) {
        if (!path) return "";
        const parts = path.split('/');
        return parts[parts.length - 1];
    }

    function countLines(text) {
        if (!text) return 0;
        return text.split('\n').length;
    }

    implicitHeight: fileEditView.implicitHeight

    Rectangle {
        id: fileEditView

        property bool expanded: false

        anchors.fill: parent
        implicitHeight: expanded ? headerArea.height + contentColumn.implicitHeight + root.contentBottomPadding + root.contentMargin * 2
                                 : headerArea.height
        radius: root.borderRadius

        color: palette.base

        border.width: 1
        border.color: root.isPending
            ? (color.hslLightness > 0.5 ? Qt.darker(color, 1.3) : Qt.lighter(color, 1.3))
            : Qt.alpha(root.statusColor, 0.6)

        clip: true

        Behavior on implicitHeight {
            NumberAnimation {
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        states: [
            State {
                name: "expanded"
                when: fileEditView.expanded
                PropertyChanges { target: contentColumn; opacity: 1 }
            },
            State {
                name: "collapsed"
                when: !fileEditView.expanded
                PropertyChanges { target: contentColumn; opacity: 0 }
            }
        ]

        transitions: Transition {
            NumberAnimation {
                properties: "opacity"
                duration: 200
                easing.type: Easing.InOutQuad
            }
        }

        MouseArea {
            id: headerArea
            width: parent.width
            height: headerRow.height + 16
            cursorShape: Qt.PointingHandCursor
            onClicked: fileEditView.expanded = !fileEditView.expanded

            RowLayout {
                id: headerRow

                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    right: actionButtons.left
                    leftMargin: root.contentMargin
                    rightMargin: root.contentMargin
                }
                spacing: root.headerPadding

                Rectangle {
                    width: root.statusIndicatorWidth
                    height: headerText.height
                    radius: 2
                    color: root.statusColor
                }

                Text {
                    id: headerText
                    Layout.fillWidth: true
                    text: {
                        var modeText = root.oldContent.length > 0 ? qsTr("Replace") : qsTr("Append")
                        if (root.oldContent.length > 0) {
                            return qsTr("%1: %2 (+%3 -%4)")
                                .arg(modeText)
                                .arg(root.fileName)
                                .arg(root.addedLines)
                                .arg(root.removedLines)
                        } else {
                            return qsTr("%1: %2 (+%3)")
                                .arg(modeText)
                                .arg(root.fileName)
                                .arg(root.addedLines)
                        }
                    }
                    font.pixelSize: 12
                    font.bold: true
                    color: palette.text
                    elide: Text.ElideMiddle
                }

                Text {
                    text: fileEditView.expanded ? "▼" : "▶"
                    font.pixelSize: 10
                    color: palette.mid
                }

                Rectangle {
                    visible: !root.isPending
                    Layout.preferredWidth: badgeText.width + 12
                    Layout.preferredHeight: badgeText.height + 4
                    color: root.statusBgColor
                    radius: 3

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: root.statusText
                        font.pixelSize: 9
                        font.bold: true
                        color: root.isArchived ? Qt.rgba(0.6, 0.6, 0.6, 1.0) : palette.text
                    }
                }
            }

            Row {
                id: actionButtons

                anchors {
                    right: parent.right
                    rightMargin: 5
                    verticalCenter: parent.verticalCenter
                }
                spacing: 6

                QoAButton {
                    icon {
                        source: "qrc:/qt/qml/ChatView/icons/open-in-editor.svg"
                        height: 15
                        width: 15
                    }
                    hoverEnabled: true
                    onClicked: root.openInEditor(editData.edit_id)
                    
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open file in editor and navigate to changes")
                    ToolTip.delay: 500
                }

                QoAButton {
                    icon {
                        source: "qrc:/qt/qml/ChatView/icons/apply-changes-button.svg"
                        height: 15
                        width: 15
                    }                    enabled: (root.isPending || root.isRejected) && !root.isArchived
                    visible: !root.isApplied && !root.isArchived
                    onClicked: root.applyEdit(editData.edit_id)
                }

                QoAButton {
                    icon {
                        source: "qrc:/qt/qml/ChatView/icons/undo-changes-button.svg"
                        height: 15
                        width: 15
                    }
                    enabled: root.isApplied && !root.isArchived
                    visible: root.isApplied && !root.isArchived
                    onClicked: root.undoEdit(editData.edit_id)
                }

                QoAButton {
                    icon {
                        source: "qrc:/qt/qml/ChatView/icons/reject-changes-button.svg"
                        height: 15
                        width: 15
                    }
                    enabled: root.isPending && !root.isArchived
                    visible: root.isPending && !root.isArchived
                    onClicked: root.rejectEdit(editData.edit_id)
                }
            }
        }

        ColumnLayout {
            id: contentColumn

            anchors {
                left: parent.left
                right: parent.right
                top: headerArea.bottom
                margins: root.contentMargin
            }
            spacing: 8
            visible: opacity > 0

            Text {
                Layout.fillWidth: true
                text: root.filePath
                font.pixelSize: 10
                color: palette.mid
                elide: Text.ElideMiddle
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: oldContentColumn.implicitHeight + 12
                color: Qt.rgba(1, 0.2, 0.2, 0.1)
                radius: 4
                border.width: 1
                border.color: Qt.rgba(1, 0.2, 0.2, 0.3)
                visible: root.oldContent.length > 0

                Column {
                    id: oldContentColumn
                    width: parent.width
                    x: 6
                    y: 6
                    spacing: 4

                    Text {
                        text: qsTr("- Removed:")
                        font.pixelSize: 10
                        font.bold: true
                        color: Qt.rgba(1, 0.2, 0.2, 0.9)
                    }

                    TextEdit {
                        id: oldContentText
                        width: parent.width - 12
                        height: contentHeight
                        text: root.oldContent
                        font.family: root.codeFontFamily
                        font.pixelSize: root.codeFontSize
                        color: palette.text
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        selectByKeyboard: true
                        textFormat: TextEdit.PlainText
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: newContentColumn.implicitHeight + 12
                color: Qt.rgba(0.2, 0.8, 0.2, 0.1)
                radius: 4
                border.width: 1
                border.color: Qt.rgba(0.2, 0.8, 0.2, 0.3)

                Column {
                    id: newContentColumn
                    width: parent.width
                    x: 6
                    y: 6
                    spacing: 4

                    Text {
                        text: qsTr("+ Added:")
                        font.pixelSize: 10
                        font.bold: true
                        color: Qt.rgba(0.2, 0.8, 0.2, 0.9)
                    }

                    TextEdit {
                        id: newContentText
                        width: parent.width - 12
                        height: contentHeight
                        text: root.newContent
                        font.family: root.codeFontFamily
                        font.pixelSize: root.codeFontSize
                        color: palette.text
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        selectByKeyboard: true
                        textFormat: TextEdit.PlainText
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.statusMessage.length > 0
                text: root.statusMessage
                font.pixelSize: 10
                font.italic: true
                color: root.isApplied
                    ? Qt.rgba(0.2, 0.6, 0.2, 1)
                    : Qt.rgba(0.8, 0.2, 0.2, 1)
                wrapMode: Text.WordWrap
            }
        }
    }
}
