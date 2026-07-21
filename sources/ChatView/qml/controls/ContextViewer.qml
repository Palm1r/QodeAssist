// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic as QQC

import UIControls
import ChatView

Popup {
    id: root

    property string baseSystemPrompt

    signal openSettings()

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
            text: qsTr("Final prompt: Base System Prompt + Project Info + Skills")
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
}
