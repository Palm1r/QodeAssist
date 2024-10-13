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

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            QQC.TextField {
                id: messageInput

                Layout.fillWidth: true
                Layout.minimumWidth: 60
                Layout.minimumHeight: 30
                rightInset: -(parent.width - sendButton.width - clearButton.width)
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

                onAccepted: sendButton.clicked()
            }

            Button {
                id: sendButton

                Layout.alignment: Qt.AlignVCenter
                Layout.minimumHeight: 30
                text: qsTr("Send")
                onClicked: {
                    if (messageInput.text.trim() !== "") {
                        root.sendMessage(messageInput.text);
                        messageInput.text = ""
                        scrollToBottom()
                    }
                }
            }
            Button {
                id: clearButton

                Layout.alignment: Qt.AlignVCenter
                Layout.minimumHeight: 30
                text: qsTr("Clear")
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
}
