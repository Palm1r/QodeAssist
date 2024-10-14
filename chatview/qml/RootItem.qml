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

ChatRootView {
    id: root

    Rectangle {
        id: bg
        anchors.fill: parent
        color: root.backgroundColor
    }

    ColumnLayout {
        anchors {
            fill: parent
        }
        spacing: 10

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
                width: ListView.view.width - scroll.width
                msgModel: root.chatModel.processMessageContent(model.content)
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
                scrollToBottom()
            }

            onContentHeightChanged: {
                if (atYEnd) {
                    scrollToBottom()
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
                        sendChatMessage()
                        event.accepted = true;
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Button {
                id: sendButton

                Layout.alignment: Qt.AlignBottom
                text: qsTr("Send")
                onClicked: sendChatMessage()
            }
            Button {
                id: clearButton

                Layout.alignment: Qt.AlignBottom
                text: qsTr("Clear Chat")
                onClicked: clearChat()
            }
        }
    }

    Row {
        id: bar

        layoutDirection: Qt.RightToLeft

        anchors {
            left: parent.left
            leftMargin: 5
            right: parent.right
            rightMargin: scroll.width
        }
        spacing: 10

        Badge {
            text: "%1/%2".arg(root.chatModel.totalTokens).arg(root.chatModel.tokensThreshold)
            color: root.codeColor
            fontColor: root.primaryColor.hslLightness > 0.5 ? "black" : "white"
        }
    }

    function clearChat() {
        root.chatModel.clear()
    }

    function scrollToBottom() {
        Qt.callLater(chatListView.positionViewAtEnd)
    }

    function sendChatMessage() {
        root.sendMessage(messageInput.text);
        messageInput.text = ""
        scrollToBottom()
    }
}
