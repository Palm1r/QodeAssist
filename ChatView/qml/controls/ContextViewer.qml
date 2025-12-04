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

    property string baseSystemPrompt
    property string currentAgentRole
    property string currentAgentRoleDescription
    property string currentAgentRoleSystemPrompt
    property var activeRules
    property int activeRulesCount
    property string selectedRuleContent

    signal openSettings()
    signal openAgentRolesSettings()
    signal openRulesFolder()
    signal refreshRules()
    signal ruleSelected(int index)

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
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Chat Context")
                font.pixelSize: 16
                font.bold: true
                color: palette.text
                Layout.fillWidth: true
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

        Flickable {
            id: mainFlickable

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: sectionsColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: sectionsColumn

                width: mainFlickable.width
                spacing: 8

                CollapsibleSection {
                    id: systemPromptSection

                    Layout.fillWidth: true
                    title: qsTr("Base System Prompt")
                    badge: root.baseSystemPrompt.length > 0 ? qsTr("Active") : qsTr("Empty")
                    badgeColor: root.baseSystemPrompt.length > 0 ? Qt.rgba(0.2, 0.6, 0.3, 1.0) : palette.mid

                    sectionContent: ColumnLayout {
                        spacing: 5

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(Math.max(systemPromptText.implicitHeight + 16, 50), 200)
                            color: palette.base
                            border.color: palette.mid
                            border.width: 1
                            radius: 2

                            Flickable {
                                id: systemPromptFlickable

                                anchors.fill: parent
                                anchors.margins: 8
                                contentHeight: systemPromptText.implicitHeight
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds

                                TextEdit {
                                    id: systemPromptText

                                    width: systemPromptFlickable.width
                                    text: root.baseSystemPrompt.length > 0 ? root.baseSystemPrompt : qsTr("No system prompt configured")
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: Text.WordWrap
                                    color: root.baseSystemPrompt.length > 0 ? palette.text : palette.mid
                                    font.family: "monospace"
                                    font.pixelSize: 11
                                }

                                QQC.ScrollBar.vertical: QQC.ScrollBar {
                                    policy: systemPromptFlickable.contentHeight > systemPromptFlickable.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Item { Layout.fillWidth: true }

                            QoAButton {
                                text: qsTr("Copy")
                                enabled: root.baseSystemPrompt.length > 0
                                onClicked: utils.copyToClipboard(root.baseSystemPrompt)
                            }

                            QoAButton {
                                text: qsTr("Edit in Settings")
                                onClicked: {
                                    root.openSettings()
                                    root.close()
                                }
                            }
                        }
                    }
                }

                CollapsibleSection {
                    id: agentRoleSection

                    Layout.fillWidth: true
                    title: qsTr("Agent Role")
                    badge: root.currentAgentRole
                    badgeColor: root.currentAgentRoleSystemPrompt.length > 0 ? Qt.rgba(0.3, 0.4, 0.7, 1.0) : palette.mid

                    sectionContent: ColumnLayout {
                        spacing: 8

                        Text {
                            text: root.currentAgentRoleDescription
                            font.pixelSize: 11
                            font.italic: true
                            color: palette.mid
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            visible: root.currentAgentRoleDescription.length > 0
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(Math.max(agentPromptText.implicitHeight + 16, 50), 200)
                            color: palette.base
                            border.color: palette.mid
                            border.width: 1
                            radius: 2
                            visible: root.currentAgentRoleSystemPrompt.length > 0

                            Flickable {
                                id: agentPromptFlickable

                                anchors.fill: parent
                                anchors.margins: 8
                                contentHeight: agentPromptText.implicitHeight
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds

                                TextEdit {
                                    id: agentPromptText

                                    width: agentPromptFlickable.width
                                    text: root.currentAgentRoleSystemPrompt
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: Text.WordWrap
                                    color: palette.text
                                    font.family: "monospace"
                                    font.pixelSize: 11
                                }

                                QQC.ScrollBar.vertical: QQC.ScrollBar {
                                    policy: agentPromptFlickable.contentHeight > agentPromptFlickable.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff
                                }
                            }
                        }

                        Text {
                            text: qsTr("No role selected. Using base system prompt only.")
                            font.pixelSize: 11
                            color: palette.mid
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            visible: root.currentAgentRoleSystemPrompt.length === 0
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Item { Layout.fillWidth: true }

                            QoAButton {
                                text: qsTr("Copy")
                                enabled: root.currentAgentRoleSystemPrompt.length > 0
                                onClicked: utils.copyToClipboard(root.currentAgentRoleSystemPrompt)
                            }

                            QoAButton {
                                text: qsTr("Manage Roles")
                                onClicked: {
                                    root.openAgentRolesSettings()
                                    root.close()
                                }
                            }
                        }
                    }
                }

                CollapsibleSection {
                    id: projectRulesSection

                    Layout.fillWidth: true
                    title: qsTr("Project Rules")
                    badge: root.activeRulesCount > 0 ? qsTr("%1 active").arg(root.activeRulesCount) : qsTr("None")
                    badgeColor: root.activeRulesCount > 0 ? Qt.rgba(0.6, 0.5, 0.2, 1.0) : palette.mid

                    sectionContent: ColumnLayout {
                        spacing: 8

                        SplitView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 220
                            orientation: Qt.Horizontal
                            visible: root.activeRulesCount > 0

                            Rectangle {
                                SplitView.minimumWidth: 120
                                SplitView.preferredWidth: 180
                                color: palette.base
                                border.color: palette.mid
                                border.width: 1
                                radius: 2

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 5

                                    Text {
                                        text: qsTr("Rules (%1)").arg(rulesList.count)
                                        font.pixelSize: 11
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
                                        boundsBehavior: Flickable.StopAtBounds

                                        delegate: ItemDelegate {
                                            required property var modelData
                                            required property int index

                                            width: ListView.view.width
                                            height: ruleItemContent.implicitHeight + 8
                                            highlighted: ListView.isCurrentItem

                                            background: Rectangle {
                                                color: {
                                                    if (parent.highlighted)
                                                        return palette.highlight
                                                    if (parent.hovered)
                                                        return Qt.tint(palette.base, Qt.rgba(0, 0, 0, 0.05))
                                                    return "transparent"
                                                }
                                                radius: 2
                                            }

                                            contentItem: ColumnLayout {
                                                id: ruleItemContent
                                                spacing: 2

                                                Text {
                                                    text: modelData.fileName
                                                    font.pixelSize: 10
                                                    color: parent.parent.highlighted ? palette.highlightedText : palette.text
                                                    elide: Text.ElideMiddle
                                                    Layout.fillWidth: true
                                                }

                                                Text {
                                                    text: modelData.category
                                                    font.pixelSize: 9
                                                    color: parent.parent.highlighted ? palette.highlightedText : palette.mid
                                                    Layout.fillWidth: true
                                                }
                                            }

                                            onClicked: {
                                                rulesList.currentIndex = index
                                                root.ruleSelected(index)
                                            }
                                        }

                                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                                            policy: rulesList.contentHeight > rulesList.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                SplitView.fillWidth: true
                                SplitView.minimumWidth: 200
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
                                            font.pixelSize: 11
                                            font.bold: true
                                            color: palette.text
                                            Layout.fillWidth: true
                                        }

                                        QoAButton {
                                            text: qsTr("Copy")
                                            enabled: root.selectedRuleContent.length > 0
                                            onClicked: utils.copyToClipboard(root.selectedRuleContent)
                                        }
                                    }

                                    Flickable {
                                        id: ruleContentFlickable

                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        contentHeight: ruleContentArea.implicitHeight
                                        clip: true
                                        boundsBehavior: Flickable.StopAtBounds

                                        TextEdit {
                                            id: ruleContentArea

                                            width: ruleContentFlickable.width
                                            text: root.selectedRuleContent
                                            readOnly: true
                                            selectByMouse: true
                                            wrapMode: Text.WordWrap
                                            selectionColor: palette.highlight
                                            color: palette.text
                                            font.family: "monospace"
                                            font.pixelSize: 11
                                        }

                                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                                            policy: ruleContentFlickable.contentHeight > ruleContentFlickable.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            text: qsTr("No project rules found.\nCreate .md files in .qodeassist/rules/common/ or .qodeassist/rules/chat/")
                            font.pixelSize: 11
                            color: palette.mid
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                            visible: root.activeRulesCount === 0
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Item { Layout.fillWidth: true }

                            QoAButton {
                                text: qsTr("Open Rules Folder")
                                onClicked: root.openRulesFolder()
                            }
                        }
                    }
                }
            }

            QQC.ScrollBar.vertical: QQC.ScrollBar {
                policy: mainFlickable.contentHeight > mainFlickable.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: palette.mid
        }

        Text {
            text: qsTr("Final prompt: Base System Prompt + Agent Role + Project Info + Project Rules + Linked Files")
            font.pixelSize: 9
            color: palette.mid
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    component CollapsibleSection: ColumnLayout {
        id: sectionRoot

        property string title
        property string badge
        property color badgeColor: palette.mid
        property Component sectionContent: null
        property bool expanded: false

        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: sectionMouseArea.containsMouse ? Qt.tint(palette.button, Qt.rgba(0, 0, 0, 0.05)) : palette.button
            border.color: palette.mid
            border.width: 1
            radius: 2

            MouseArea {
                id: sectionMouseArea

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: sectionRoot.expanded = !sectionRoot.expanded
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: sectionRoot.expanded ? "▼" : "▶"
                    font.pixelSize: 10
                    color: palette.text
                }

                Text {
                    text: sectionRoot.title
                    font.pixelSize: 12
                    font.bold: true
                    color: palette.text
                    Layout.fillWidth: true
                }

                Rectangle {
                    implicitWidth: badgeText.implicitWidth + 12
                    implicitHeight: 18
                    color: sectionRoot.badgeColor
                    radius: 3

                    Text {
                        id: badgeText

                        anchors.centerIn: parent
                        text: sectionRoot.badge
                        font.pixelSize: 10
                        color: "#FFFFFF"
                    }
                }
            }
        }

        Loader {
            id: contentLoader

            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.topMargin: 8
            Layout.bottomMargin: 4
            sourceComponent: sectionRoot.sectionContent
            visible: sectionRoot.expanded
            active: sectionRoot.expanded
        }
    }

    onOpened: {
        if (root.activeRulesCount > 0) {
            root.ruleSelected(0)
        }
    }
}
