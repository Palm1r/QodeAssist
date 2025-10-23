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
import QtQuick.Controls.Basic as QQC

import UIControls
import ChatView

Popup {
    id: root

    property var activeRules

    property alias rulesCurrentIndex: rulesList.currentIndex
    property alias ruleContentAreaText: ruleContentArea.text

    signal refreshRules()
    signal openRulesFolder()

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: palette.window
        border.color: palette.mid
        border.width: 1
        radius: 4
    }

    ChatUtils {
        id: utils
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Active Project Rules")
                font.pixelSize: 16
                font.bold: true
                color: palette.text
                Layout.fillWidth: true
            }

            QoAButton {
                text: qsTr("Open Folder")
                onClicked: root.openRulesFolder()
            }

            QoAButton {
                text: qsTr("Refresh")
                onClicked: root.refreshRules()
            }

            QoAButton {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: palette.mid
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                SplitView.minimumWidth: 200
                SplitView.preferredWidth: parent.width * 0.3
                color: palette.base
                border.color: palette.mid
                border.width: 1
                radius: 2

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: 5

                    Text {
                        text: qsTr("Rules Files (%1)").arg(rulesList.count)
                        font.pixelSize: 12
                        font.bold: true
                        color: palette.text
                        Layout.fillWidth: true
                    }

                    ListView {
                        id: rulesList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.activeRules
                        currentIndex: 0

                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index

                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem

                            background: Rectangle {
                                color: {
                                    if (parent.highlighted) {
                                        return palette.highlight
                                    } else if (parent.hovered) {
                                        return Qt.tint(palette.base, Qt.rgba(0, 0, 0, 0.05))
                                    }
                                    return "transparent"
                                }
                                radius: 2
                            }

                            contentItem: ColumnLayout {
                                spacing: 2

                                Text {
                                    text: modelData.fileName
                                    font.pixelSize: 11
                                    color: parent.parent.highlighted ? palette.highlightedText : palette.text
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: qsTr("Category: %1").arg(modelData.category)
                                    font.pixelSize: 9
                                    color: parent.parent.highlighted ? palette.highlightedText : palette.mid
                                    Layout.fillWidth: true
                                }
                            }

                            onClicked: {
                                rulesList.currentIndex = index
                            }
                        }

                        ScrollBar.vertical: ScrollBar {}
                    }

                    Text {
                        visible: rulesList.count === 0
                        text: qsTr("No rules found.\nCreate .md files in:\n.qodeassist/rules/common/\n.qodeassist/rules/chat/")
                        font.pixelSize: 10
                        color: palette.mid
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignCenter
                    }
                }
            }

            Rectangle {
                SplitView.fillWidth: true
                color: palette.base
                border.color: palette.mid
                border.width: 1
                radius: 2

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: qsTr("Content")
                            font.pixelSize: 12
                            font.bold: true
                            color: palette.text
                            Layout.fillWidth: true
                        }

                        QoAButton {
                            text: qsTr("Copy")
                            enabled: ruleContentArea.text.length > 0
                            onClicked: utils.copyToClipboard(ruleContentArea.text)
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        QQC.TextArea {
                            id: ruleContentArea

                            readOnly: true
                            wrapMode: TextArea.Wrap
                            selectByMouse: true
                            color: palette.text
                            font.family: "monospace"
                            font.pixelSize: 11

                            background: Rectangle {
                                color: Qt.darker(palette.base, 1.02)
                                border.color: palette.mid
                                border.width: 1
                                radius: 2
                            }

                            placeholderText: qsTr("Select a rule file to view its content")
                        }
                    }
                }
            }
        }

        Text {
            text: qsTr("Rules are loaded from .qodeassist/rules/ directory in your project.\n" +
                      "Common rules apply to all contexts, chat rules apply only to chat assistant.")
            font.pixelSize: 9
            color: palette.mid
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
