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

pragma ComponentBehavior: Bound

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
        anchors.fill: parent

        RowLayout {
            id: topBar

            Layout.leftMargin: 5
            Layout.rightMargin: 5
            spacing: 10

            Button {
                text: qsTr("Save")
                onClicked: root.showSaveDialog()
            }

            Button {
                text: qsTr("Load")
                onClicked: root.showLoadDialog()
            }

            Button {
                text: qsTr("Clear")
                onClicked: root.clearChat()
            }

            Item {
                Layout.fillWidth: true
            }

            Badge {
                text: qsTr("tokens:%1/%2").arg(root.chatModel.totalTokens).arg(root.chatModel.tokensThreshold)
                color: root.codeColor
                fontColor: root.primaryColor.hslLightness > 0.5 ? "black" : "white"
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Button {
                id: sendButton

                Layout.alignment: Qt.AlignBottom
                text: qsTr("Send")
                onClicked: root.sendChatMessage()
            }

            Button {
                id: stopButton

                Layout.alignment: Qt.AlignBottom
                text: qsTr("Stop")
                onClicked: root.cancelRequest()
            }

            CheckBox {
                id: sharingCurrentFile

                text: "Share current file with models"
                checked: root.isSharingCurrentFile
            }
        }
    }

    function clearChat() {
        root.chatModel.clear()
    }

    function scrollToBottom() {
        Qt.callLater(chatListView.positionViewAtEnd)
    }

    function sendChatMessage() {
        root.sendMessage(messageInput.text, sharingCurrentFile.checked)
        messageInput.text = ""
        scrollToBottom()
    }
}
