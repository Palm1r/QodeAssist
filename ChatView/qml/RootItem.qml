/*
 * Copyright (C) 2024 Petr Mironychev
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
import QtQuick.Controls.Basic as QQC
import QtQuick.Layouts
import ChatView
import "./controls"
import "./parts"

ChatRootView {
    id: root

    property SystemPalette sysPalette: SystemPalette {
        colorGroup: SystemPalette.Active
    }

    palette {
        window: sysPalette.window
        windowText: sysPalette.windowText
        base: sysPalette.base
        alternateBase: sysPalette.alternateBase
        text: sysPalette.text
        button: sysPalette.button
        buttonText: sysPalette.buttonText
        highlight: sysPalette.highlight
        highlightedText: sysPalette.highlightedText
        light: sysPalette.light
        mid: sysPalette.mid
        dark: sysPalette.dark
        shadow: sysPalette.shadow
        brightText: sysPalette.brightText
    }

    Rectangle {
        id: bg

        anchors.fill: parent
        color: palette.window
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 40

            saveButton.onClicked: root.showSaveDialog()
            loadButton.onClicked: root.showLoadDialog()
            clearButton.onClicked: root.clearChat()
            tokensBadge {
                text: qsTr("tokens:%1/%2").arg(root.inputTokensCount).arg(root.chatModel.tokensThreshold)
            }
            recentPath {
                text: qsTr("Latest chat file name: %1").arg(root.chatFileName.length > 0 ? root.chatFileName : "Unsaved")
            }
            openChatHistory.onClicked: root.openChatHistoryFolder()
        }

        ListView {
            id: chatListView

            Layout.fillWidth: true
            Layout.fillHeight: true
            leftMargin: 5
            model: root.chatModel
            clip: true
            spacing: 10
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 2000

            delegate: ChatItem {
                required property var model

                width: ListView.view.width - scroll.width
                msgModel: root.chatModel.processMessageContent(model.content)
                messageAttachments: model.attachments
                color: model.roleType === ChatModel.User ? palette.alternateBase
                                                         : palette.base
            }

            header: Item {
                width: ListView.view.width - scroll.width
                height: 30
            }

            ScrollBar.vertical: QQC.ScrollBar {
                id: scroll
            }

            onCountChanged: {
                root.scrollToBottom()
            }

            onContentHeightChanged: {
                if (atYEnd) {
                    root.scrollToBottom()
                }
            }
        }

        ScrollView {
            id: view

            Layout.fillWidth: true
            Layout.minimumHeight: 30
            Layout.maximumHeight: root.height / 2

            QQC.TextArea {
                id: messageInput

                placeholderText: qsTr("Type your message here...")
                placeholderTextColor: palette.mid
                color: palette.text
                background: Rectangle {
                    radius: 2
                    color: palette.base
                    border.color:  messageInput.activeFocus ? palette.highlight : palette.button
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation { duration: 150 }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: palette.highlight
                        opacity: messageInput.hovered ? 0.1 : 0
                        radius: parent.radius
                    }
                }

                onTextChanged: root.calculateMessageTokensCount(messageInput.text)

                Keys.onPressed: function(event) {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                        root.sendChatMessage()
                        event.accepted = true;
                    }
                }
            }
        }

        AttachedFilesPlace {
            id: attachedFilesPlace

            Layout.fillWidth: true
            attachedFilesModel: root.attachmentFiles
            iconPath: palette.window.hslLightness > 0.5 ? "qrc:/qt/qml/ChatView/icons/attach-file-dark.svg"
                                                        : "qrc:/qt/qml/ChatView/icons/attach-file-light.svg"
            accentColor: Qt.tint(palette.mid, Qt.rgba(0, 0.8, 0.3, 0.4))
            onRemoveFileFromListByIndex: (index) => root.removeFileFromAttachList(index)
        }

        AttachedFilesPlace {
            id: linkedFilesPlace

            Layout.fillWidth: true
            attachedFilesModel: root.linkedFiles
            iconPath: palette.window.hslLightness > 0.5 ? "qrc:/qt/qml/ChatView/icons/link-file-dark.svg"
                                                        : "qrc:/qt/qml/ChatView/icons/link-file-light.svg"
            accentColor: Qt.tint(palette.mid, Qt.rgba(0, 0.3, 0.8, 0.4))
            onRemoveFileFromListByIndex: (index) => root.removeFileFromLinkList(index)
        }

        BottomBar {
            id: bottomBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 40

            sendButton.onClicked: root.sendChatMessage()
            stopButton.onClicked: root.cancelRequest()
            syncOpenFiles {
                checked: root.isSyncOpenFiles
                onCheckedChanged: root.setIsSyncOpenFiles(bottomBar.syncOpenFiles.checked)
            }
            attachFiles.onClicked: root.showAttachFilesDialog()
            linkFiles.onClicked: root.showLinkFilesDialog()
            testRag.onClicked: root.testRAG(messageInput.text)
            testChunks.onClicked: root.testChunking()
        }
    }

    function clearChat() {
        root.chatModel.clear()
        root.clearAttachmentFiles()
        root.updateInputTokensCount()
    }

    function scrollToBottom() {
        Qt.callLater(chatListView.positionViewAtEnd)
    }

    function sendChatMessage() {
        root.sendMessage(messageInput.text)
        messageInput.text = ""
        scrollToBottom()
    }
}
