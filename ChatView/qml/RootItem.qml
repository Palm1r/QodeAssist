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

        TopBar {
            id: topBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 40

            saveButton.onClicked: root.showSaveDialog()
            loadButton.onClicked: root.showLoadDialog()
            clearButton.onClicked: root.clearChat()
            tokensBadge {
                text: qsTr("tokens:%1/%2").arg(root.chatModel.totalTokens).arg(root.chatModel.tokensThreshold)
            }
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
                color: model.roleType === ChatModel.User ? root.primaryColor : root.secondaryColor
                fontColor: root.primaryColor.hslLightness > 0.5 ? "black" : "white"
                codeBgColor: root.codeColor
                selectionColor: root.primaryColor.hslLightness > 0.5 ? Qt.darker(root.primaryColor, 1.5)
                                                                     : Qt.lighter(root.primaryColor, 1.5)

            }

            header: Item {
                width: ListView.view.width - scroll.width
                height: 30
            }

            ScrollBar.vertical: ScrollBar {
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
                placeholderTextColor: "#888"
                color: root.primaryColor.hslLightness > 0.5 ? "black" : "white"
                background: Rectangle {
                    radius: 2
                    color: root.primaryColor
                    border.color: root.primaryColor.hslLightness > 0.5 ? Qt.lighter(root.primaryColor, 1.5)
                                                                       : Qt.darker(root.primaryColor, 1.5)
                    border.width: 1
                }
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
        }

        BottomBar {
            id: bottomBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 40

            sendButton.onClicked: root.sendChatMessage()
            stopButton.onClicked: root.cancelRequest()
            sharingCurrentFile.checked: root.isSharingCurrentFile
            attachFiles.onClicked: root.showAttachFilesDialog()
        }
    }

    function clearChat() {
        root.chatModel.clear()
    }

    function scrollToBottom() {
        Qt.callLater(chatListView.positionViewAtEnd)
    }

    function sendChatMessage() {
        root.sendMessage(messageInput.text, bottomBar.sharingCurrentFile.checked)
        messageInput.text = ""
        scrollToBottom()
    }
}
