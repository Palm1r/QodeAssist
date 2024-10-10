import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ChatView

ChatRootView {
    id: root
    width: 400
    height: 600

    Rectangle {
        id: bg
        anchors.fill: parent
        color: root.backgroundColor
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        ListView {
            id: chatListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.chatModel
            clip: true
            spacing: 10

            delegate: Rectangle {
                width: ListView.view.width
                height: messageText.height + 20
                color: model.roleType === ChatRole.User ? "#e6e6e6" : "#f0f0f0"
                radius: 10

                Text {
                    id: messageText
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        margins: 10
                    }
                    text: model.content
                    wrapMode: Text.Wrap
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: messageInput
                Layout.fillWidth: true
                placeholderText: "Type your message here..."
                onAccepted: sendButton.clicked()
            }

            Button {
                id: sendButton
                text: "Send"
                onClicked: {
                    if (messageInput.text.trim() !== "") {
                        root.sendMessage(messageInput.text);
                        messageInput.text = ""
                    }
                }
            }
        }
    }
}
