/*
 * Copyright (C) 2024-2025 Petr Mironychev
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
import UIControls
import Qt.labs.platform as Platform

import "./chatparts"
import "./controls"

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

    SplitDropZone {
        anchors.fill: parent
        z: 99

        onFilesDroppedToAttach: (urlStrings) => {
            var localPaths = root.convertUrlsToLocalPaths(urlStrings)
            if (localPaths.length > 0) {
                root.addFilesToAttachList(localPaths)
            }
        }

        onFilesDroppedToLink: (urlStrings) => {
            var localPaths = root.convertUrlsToLocalPaths(urlStrings)
            if (localPaths.length > 0) {
                root.addFilesToLinkList(localPaths)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: childrenRect.height + 10

            isCompressing: root.isCompressing
            saveButton.onClicked: root.showSaveDialog()
            loadButton.onClicked: root.showLoadDialog()
            clearButton.onClicked: root.clearChat()
            compressButton.onClicked: compressConfirmDialog.open()
            cancelCompressButton.onClicked: root.cancelCompression()
            tokensBadge {
                text: qsTr("%1/%2").arg(root.inputTokensCount).arg(root.chatModel.tokensThreshold)
            }
            recentPath {
                text: qsTr("Сhat name: %1").arg(root.chatFileName.length > 0 ? root.chatFileName : "Unsaved")
            }
            openChatHistory.onClicked: root.openChatHistoryFolder()
            contextButton.onClicked: contextViewer.open()
            pinButton {
                visible: typeof _chatview !== 'undefined'
                checked: typeof _chatview !== 'undefined' ? _chatview.isPin : false
                onCheckedChanged: _chatview.isPin = topBar.pinButton.checked
            }
            toolsButton {
                checked: root.useTools
                onCheckedChanged: {
                    root.useTools = toolsButton.checked
                }
            }
            thinkingMode {
                checked: root.useThinking
                enabled: root.isThinkingSupport
                onCheckedChanged: {
                    root.useThinking = thinkingMode.checked
                }
            }
            settingsButton.onClicked: root.openSettings()
            configSelector {
                model: root.availableConfigurations
                displayText: root.currentConfiguration
                onActivated: function(index) {
                    if (index > 0) {
                        root.applyConfiguration(root.availableConfigurations[index])
                    }
                }
                
                popup.onAboutToShow: {
                    root.loadAvailableConfigurations()
                }
            }
            
            roleSelector {
                model: root.availableAgentRoles
                displayText: root.currentAgentRole
                onActivated: function(index) {
                    root.applyAgentRole(root.availableAgentRoles[index])
                }
                
                popup.onAboutToShow: {
                    root.loadAvailableAgentRoles()
                }
            }
        }

        ListView {
            id: chatListView

            property bool userScrolledUp: false

            Layout.fillWidth: true
            Layout.fillHeight: true
            leftMargin: 5
            model: root.chatModel
            clip: true
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 2000

            onMovingChanged: {
                if (moving) {
                    userScrolledUp = !atYEnd
                }
            }

            onAtYEndChanged: {
                if (atYEnd) {
                    userScrolledUp = false
                }
            }

            delegate: Loader {
                id: componentLoader

                required property var model
                required property int index

                width: ListView.view.width - scroll.width

                sourceComponent: {
                    if (model.roleType === ChatModel.Tool) {
                        return toolMessageComponent
                    } else if (model.roleType === ChatModel.FileEdit) {
                        return fileEditMessageComponent
                    } else if (model.roleType === ChatModel.Thinking) {
                        return thinkingMessageComponent
                    } else {
                        return chatItemComponent
                    }
                }

            }

            header: Item {
                width: ListView.view.width - scroll.width
                height: 30
            }

            ScrollBar.vertical: QQC.ScrollBar {
                id: scroll
            }

            Rectangle {
                id: scrollToBottomButton

                anchors {
                    bottom: parent.bottom
                    horizontalCenter: parent.horizontalCenter
                    bottomMargin: 10
                }
                width: 36
                height: 36
                radius: 18
                color: palette.button
                border.color: palette.mid
                border.width: 1
                visible: chatListView.userScrolledUp
                opacity: 0.9
                z: 100

                Text {
                    anchors.centerIn: parent
                    text: "▼"
                    font.pixelSize: 14
                    color: palette.buttonText
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        chatListView.userScrolledUp = false
                        root.scrollToBottom()
                    }
                }

                Behavior on visible {
                    enabled: false
                }
            }

            onCountChanged: {
                if (!userScrolledUp) {
                    root.scrollToBottom()
                }
            }

            onContentHeightChanged: {
                if (!userScrolledUp && atYEnd) {
                    root.scrollToBottom()
                }
            }

            Component {
                id: chatItemComponent

                ChatItem {
                    id: chatItemInstance

                    width: parent.width
                    msgModel: root.chatModel.processMessageContent(model.content)
                    messageAttachments: model.attachments
                    messageImages: model.images
                    chatFilePath: root.chatFilePath()
                    isUserMessage: model.roleType === ChatModel.User
                    messageIndex: index
                    textFontFamily: root.textFontFamily
                    codeFontFamily: root.codeFontFamily
                    codeFontSize: root.codeFontSize
                    textFontSize: root.textFontSize
                    textFormat: root.textFormat

                    onResetChatToMessage: function(idx) {
                        messageInput.text = model.content
                        messageInput.cursorPosition = model.content.length
                        root.chatModel.resetModelTo(idx)
                    }
                }
            }

            Component {
                id: toolMessageComponent

                ToolBlock {
                    width: parent.width
                    toolContent: model.content
                }
            }

            Component {
                id: fileEditMessageComponent

                FileEditBlock {
                    width: parent.width
                    editContent: model.content

                    onApplyEdit: function(editId) {
                        root.applyFileEdit(editId)
                    }

                    onRejectEdit: function(editId) {
                        root.rejectFileEdit(editId)
                    }

                    onUndoEdit: function(editId) {
                        root.undoFileEdit(editId)
                    }

                    onOpenInEditor: function(editId) {
                        root.openFileEditInEditor(editId)
                    }
                }
            }

            Component {
                id: thinkingMessageComponent

                ThinkingBlock {
                    width: parent.width
                    thinkingContent: {
                        let content = model.content
                        let signatureStart = content.indexOf("\n[Signature:")
                        if (signatureStart >= 0) {
                            return content.substring(0, signatureStart)
                        }
                        return content
                    }
                    isRedacted: model.isRedacted !== undefined ? model.isRedacted : false
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

                placeholderText: Qt.platform.os === "osx"
                                 ? qsTr("Type your message here... (⌘+↩ to send)")
                                 : qsTr("Type your message here... (Ctrl+Enter to send)")
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

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: messageContextMenu.open()
                    propagateComposedEvents: true
                }
            }
        }

        Platform.Menu {
            id: messageContextMenu

            Platform.MenuItem {
                text: qsTr("Cut")
                enabled: messageInput.selectedText.length > 0
                onTriggered: messageInput.cut()
            }

            Platform.MenuItem {
                text: qsTr("Copy")
                enabled: messageInput.selectedText.length > 0
                onTriggered: messageInput.copy()
            }

            Platform.MenuItem {
                text: qsTr("Paste")
                enabled: messageInput.canPaste
                onTriggered: messageInput.paste()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Select All")
                enabled: messageInput.text.length > 0
                onTriggered: messageInput.selectAll()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Clear")
                enabled: messageInput.text.length > 0
                onTriggered: messageInput.clear()
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

        FileEditsActionBar {
            id: fileEditsActionBar

            Layout.fillWidth: true
            totalEdits: root.currentMessageTotalEdits
            appliedEdits: root.currentMessageAppliedEdits
            pendingEdits: root.currentMessagePendingEdits
            rejectedEdits: root.currentMessageRejectedEdits

            onApplyAllClicked: root.applyAllFileEditsForCurrentMessage()
            onUndoAllClicked: root.undoAllFileEditsForCurrentMessage()
        }

        BottomBar {
            id: bottomBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: 40

            sendButton.onClicked: !root.isRequestInProgress ? root.sendChatMessage()
                                                            : root.cancelRequest()
            sendButton.icon.source: !root.isRequestInProgress ? "qrc:/qt/qml/ChatView/icons/chat-icon.svg"
                                                              : "qrc:/qt/qml/ChatView/icons/chat-pause-icon.svg"
            sendButton.ToolTip.text: !root.isRequestInProgress ? qsTr("Send message to LLM %1").arg(Qt.platform.os === "osx" ? "Cmd+Return" : "Ctrl+Return")
                                                               : qsTr("Stop")
            syncOpenFiles {
                checked: root.isSyncOpenFiles
                onCheckedChanged: root.setIsSyncOpenFiles(bottomBar.syncOpenFiles.checked)
            }
            attachFiles.onClicked: root.showAttachFilesDialog()
            attachImages.onClicked: root.showAddImageDialog()
            linkFiles.onClicked: root.showLinkFilesDialog()
        }
    }

    Shortcut {
        id: sendMessageShortcut

        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        context: Qt.WindowShortcut
        onActivated: {
            if (messageInput.activeFocus && !Qt.inputMethod.visible) {
                root.sendChatMessage()
            }
        }
    }

    function clearChat() {
        root.clearMessages()
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

    Dialog {
        id: compressConfirmDialog

        anchors.centerIn: parent
        title: qsTr("Compress Chat")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        Label {
            text: qsTr("Create a summarized copy of this chat?\n\nThe summary will be generated by LLM and saved as a new chat file.")
            wrapMode: Text.WordWrap
        }

        onAccepted: root.compressCurrentChat()
    }

    Toast {
        id: errorToast
        z: 1000

        color: Qt.rgba(0.8, 0.2, 0.2, 0.9)
        border.color: Qt.darker(infoToast.color, 1.3)
        toastTextColor: "#FFFFFF"
    }

    Toast {
        id: infoToast
        z: 1000

        color: Qt.rgba(0.2, 0.8, 0.2, 0.9)
        border.color: Qt.darker(infoToast.color, 1.3)
        toastTextColor: "#FFFFFF"
    }

    ContextViewer {
        id: contextViewer

        width: Math.min(parent.width * 0.85, 800)
        height: Math.min(parent.height * 0.85, 700)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        baseSystemPrompt: root.baseSystemPrompt
        currentAgentRole: root.currentAgentRole
        currentAgentRoleDescription: root.currentAgentRoleDescription
        currentAgentRoleSystemPrompt: root.currentAgentRoleSystemPrompt
        activeRules: root.activeRules
        activeRulesCount: root.activeRulesCount

        onOpenSettings: root.openSettings()
        onOpenAgentRolesSettings: root.openAgentRolesSettings()
        onOpenRulesFolder: root.openRulesFolder()
        onRefreshRules: root.refreshRules()
        onRuleSelected: function(index) {
            contextViewer.selectedRuleContent = root.getRuleContent(index)
        }
    }

    Connections {
        target: root
        function onLastErrorMessageChanged() {
            if (root.lastErrorMessage.length > 0) {
                errorToast.show(root.lastErrorMessage)
            }
        }
        function onLastInfoMessageChanged() {
            if (root.lastInfoMessage.length > 0) {
                infoToast.show(root.lastInfoMessage)
            }
        }
    }

    Component.onCompleted: {
        messageInput.forceActiveFocus()
    }
}
