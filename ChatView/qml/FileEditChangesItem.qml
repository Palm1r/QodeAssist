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
import QtQuick.Layouts
import ChatView
import UIControls
import "./parts"

FileEditItem {
    id: root

    implicitHeight: fileEditView.implicitHeight

    Component.onCompleted: {
        root.parseFromContent(model.content)
    }

    readonly property int borderRadius: 4
    readonly property int contentMargin: 10
    readonly property int contentBottomPadding: 20
    readonly property int headerPadding: 8
    readonly property int statusIndicatorWidth: 4

    readonly property bool isPending: status === FileEditItem.Pending
    readonly property bool isApplied: status === FileEditItem.Applied
    readonly property bool isReverted: status === FileEditItem.Reverted
    readonly property bool isRejected: status === FileEditItem.Rejected

    readonly property color appliedColor: Qt.rgba(0.2, 0.8, 0.2, 0.8)
    readonly property color revertedColor: Qt.rgba(0.8, 0.6, 0.2, 0.8)
    readonly property color rejectedColor: palette.mid
    readonly property color pendingColor: palette.highlight

    readonly property color appliedBgColor: Qt.rgba(0.2, 0.8, 0.2, 0.3)
    readonly property color revertedBgColor: Qt.rgba(0.8, 0.6, 0.2, 0.3)
    readonly property color rejectedBgColor: Qt.rgba(0.8, 0.2, 0.2, 0.3)

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
        if (isApplied) return appliedColor
        if (isReverted) return revertedColor
        if (isRejected) return rejectedColor
        return pendingColor
    }

    readonly property color statusBgColor: {
        if (isApplied) return appliedBgColor
        if (isReverted) return revertedBgColor
        if (isRejected) return rejectedBgColor
        return palette.button
    }

    readonly property string statusText: {
        if (isApplied) return qsTr("APPLIED")
        if (isReverted) return qsTr("REVERTED")
        if (isRejected) return qsTr("REJECTED")
        return ""
    }

    Rectangle {
        id: fileEditView

        property bool expanded: false

        anchors.fill: parent
        implicitHeight: expanded ? headerArea.height + contentColumn.height + root.contentBottomPadding
                                 : headerArea.height
        radius: root.borderRadius

        color: palette.base

        border.width: 1
        border.color: root.isPending
            ? (color.hslLightness > 0.5 ? Qt.darker(color, 1.3) : Qt.lighter(color, 1.3))
            : Qt.alpha(root.statusColor, 0.6)

        clip: true

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
                properties: "implicitHeight,opacity"
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
                        var modeText = ""
                        switch(root.editMode) {
                            case "replace": modeText = qsTr("Replace"); break;
                            case "insert_before": modeText = qsTr("Insert Before"); break;
                            case "insert_after": modeText = qsTr("Insert After"); break;
                            case "append": modeText = qsTr("Append"); break;
                            default: modeText = root.editMode;
                        }
                        return qsTr("%1: %2 (+%3 -%4)")
                            .arg(modeText)
                            .arg(root.filePath)
                            .arg(root.addedLines)
                            .arg(root.removedLines)
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

                Badge {
                    visible: !root.isPending
                    text: root.statusText
                    color: root.statusBgColor
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
                    text: qsTr("Apply")
                    enabled: root.isPending || root.isReverted || root.isRejected
                    visible: !root.isApplied
                    onClicked: root.applyEdit()
                }

                QoAButton {
                    text: qsTr("Revert")
                    enabled: root.isApplied
                    visible: !root.isReverted && !root.isRejected
                    onClicked: root.revertEdit()
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
            spacing: 4
            visible: opacity > 0

            TextEdit {
                Layout.fillWidth: true
                visible: root.contextBefore.length > 0
                text: root.contextBefore
                font.family: root.codeFontFamily
                font.pixelSize: root.codeFontSize
                color: palette.mid
                wrapMode: TextEdit.Wrap
                opacity: 0.6
                readOnly: true
                selectByMouse: true
                selectByKeyboard: true
                textFormat: TextEdit.PlainText
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: oldContentText.implicitHeight + 8
                color: Qt.rgba(1, 0.2, 0.2, 0.1)
                radius: 4
                border.width: 1
                border.color: Qt.rgba(1, 0.2, 0.2, 0.3)
                visible: root.originalContent.length > 0

                TextEdit {
                    id: oldContentText
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 4
                    }
                    text: "- " + root.originalContent
                    font.family: root.codeFontFamily
                    font.pixelSize: root.codeFontSize
                    color: Qt.rgba(1, 0.2, 0.2, 0.9)
                    wrapMode: TextEdit.Wrap
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    textFormat: TextEdit.PlainText
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: newContentText.implicitHeight + 8
                color: Qt.rgba(0.2, 0.8, 0.2, 0.1)
                radius: 4
                border.width: 1
                border.color: Qt.rgba(0.2, 0.8, 0.2, 0.3)

                TextEdit {
                    id: newContentText
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 4
                    }
                    text: "+ " + root.newContent
                    font.family: root.codeFontFamily
                    font.pixelSize: root.codeFontSize
                    color: Qt.rgba(0.2, 0.8, 0.2, 0.9)
                    wrapMode: TextEdit.Wrap
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    textFormat: TextEdit.PlainText
                }
            }

            TextEdit {
                Layout.fillWidth: true
                visible: root.contextAfter.length > 0
                text: root.contextAfter
                font.family: root.codeFontFamily
                font.pixelSize: root.codeFontSize
                color: palette.mid
                wrapMode: TextEdit.Wrap
                opacity: 0.6
                readOnly: true
                selectByMouse: true
                selectByKeyboard: true
                textFormat: TextEdit.PlainText
            }

            Text {
                Layout.fillWidth: true
                visible: root.statusMessage.length > 0
                text: root.statusMessage
                font.pixelSize: 11
                font.italic: true
                color: root.isApplied
                    ? Qt.rgba(0.2, 0.6, 0.2, 1)
                    : Qt.rgba(0.8, 0.2, 0.2, 1)
                wrapMode: Text.WordWrap
            }
        }
    }
}
