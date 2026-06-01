// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

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

    property bool hasActiveError: false
    readonly property color errorColor: "#d32f2f"

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

    QoABusyOverlay {
        id: compressingOverlay

        z: 50

        anchors.fill: mainColumn
        anchors.topMargin: topBar.height
        anchors.bottomMargin: bottomBar.height

        active: root.isCompressing
        text: qsTr("Compressing chat…")
    }

    ColumnLayout {
        id: mainColumn

        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar

            Layout.preferredWidth: parent.width
            Layout.preferredHeight: childrenRect.height + 10

            isInEditor: root.isInEditor
            saveButton.onClicked: root.showSaveDialog()
            loadButton.onClicked: root.showLoadDialog()
            clearButton.onClicked: root.clearChat()
            newChatButton.onClicked: root.requestNewChat()
            tokensBadge {
                readonly property int sessionPrompt: root.chatModel.sessionPromptTokens || 0
                readonly property int sessionCompletion: root.chatModel.sessionCompletionTokens || 0
                readonly property int sessionCached: root.chatModel.sessionCachedPromptTokens || 0
                text: sessionCached > 0
                    ? qsTr("next ~%1  ·  session ↑%2 ↓%3 ↻%4")
                          .arg(root.inputTokensCount)
                          .arg(sessionPrompt)
                          .arg(sessionCompletion)
                          .arg(sessionCached)
                    : qsTr("next ~%1  ·  session ↑%2 ↓%3")
                          .arg(root.inputTokensCount)
                          .arg(sessionPrompt)
                          .arg(sessionCompletion)
                ToolTip.text: sessionCached > 0
                    ? qsTr("next request (estimate)  ·  session prompt ↑ / completion ↓ / cached ↻ (provider cache hits)")
                    : qsTr("next request (estimate)  ·  session prompt ↑ / completion ↓")
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
            relocateButton {
                icon.source: (typeof _chatview !== 'undefined')
                             ? "qrc:/qt/qml/ChatView/icons/open-in-editor.svg"
                             : "qrc:/qt/qml/ChatView/icons/open-in-window.svg"
                onClicked: {
                    if (typeof _chatview !== 'undefined')
                        root.relocateToSplit()
                    else
                        root.relocateToWindow()
                }
            }
            relocateTooltip.text: (typeof _chatview !== 'undefined')
                                   ? qsTr("Move this chat to an editor tab")
                                   : qsTr("Move this chat to a separate window")
            settingsButton.onClicked: root.openSettings()
            agentSelector {
                model: root.availableChatAgents
                displayText: root.currentChatAgent
                onActivated: function(index) {
                    root.currentChatAgent = root.availableChatAgents[index]
                }

                Component.onCompleted: root.loadAvailableChatAgents()

                popup.onAboutToShow: {
                    root.loadAvailableChatAgents()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            MessageNavigator {
                id: messageNavigator

                Layout.preferredWidth: 16
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4

                chatModel: root.chatModel

                onMessageClicked: function(messageIndex) {
                    chatListView.userScrolledUp = true
                    chatListView.positionViewAtIndex(messageIndex, ListView.Beginning)
                }
            }

        ListView {
            id: chatListView

            property bool userScrolledUp: false

            function syncNavigatorCurrent() {
                const top = indexAt(10, contentY + 4)
                messageNavigator.updateCurrentFromModelIndex(top)
            }

            Layout.fillWidth: true
            Layout.fillHeight: true
            leftMargin: 3
            model: root.chatModel
            clip: true
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 2000

            onContentYChanged: Qt.callLater(syncNavigatorCurrent)

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
                Qt.callLater(syncNavigatorCurrent)
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
                    chatViewport: chatListView
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
                    promptTokens: model.promptTokens || 0
                    completionTokens: model.completionTokens || 0
                    cachedPromptTokens: model.cachedPromptTokens || 0
                    reasoningTokens: model.reasoningTokens || 0

                    onResetChatToMessage: function(idx) {
                        messageInput.text = model.content
                        messageInput.cursorPosition = model.content.length
                        root.chatModel.resetModelTo(idx)
                    }

                    onOpenFileRequested: function(filePath) {
                        root.openFileInEditor(filePath)
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
        }

        ScrollView {
            id: view

            Layout.fillWidth: true
            Layout.minimumHeight: 30
            Layout.maximumHeight: root.height / 2

            QQC.TextArea {
                id: messageInput

                placeholderText: qsTr("Type your message here... (%1 to send)").arg(root.sendShortcutText)
                placeholderTextColor: palette.mid
                color: palette.text
                wrapMode: TextArea.Wrap
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

                onTextChanged: {
                    root.calculateMessageTokensCount(messageInput.text)
                    var cursorPos = messageInput.cursorPosition
                    var textBefore = messageInput.text.substring(0, cursorPos)

                    var atIndex = textBefore.lastIndexOf('@')
                    if (atIndex >= 0) {
                        var query = textBefore.substring(atIndex + 1)
                        if (query.indexOf(' ') === -1 && query.indexOf('\n') === -1) {
                            fileMentionPopup.updateSearch(query)
                            skillCommandPopup.dismiss()
                            return
                        }
                    }
                    fileMentionPopup.dismiss()

                    const slashIndex = textBefore.lastIndexOf('/')
                    if (slashIndex >= 0) {
                        const beforeSlash = slashIndex === 0
                                            ? ' '
                                            : textBefore.charAt(slashIndex - 1)
                        const skillQuery = textBefore.substring(slashIndex + 1)
                        if ((beforeSlash === ' ' || beforeSlash === '\n')
                                && /^[a-z0-9-]*$/.test(skillQuery)) {
                            skillCommandPopup.updateSearch(skillQuery)
                            return
                        }
                    }
                    skillCommandPopup.dismiss()
                }

                Keys.onPressed: function(event) {
                    if (fileMentionPopup.visible) {
                        if (event.key === Qt.Key_Down) {
                            fileMentionPopup.moveDown()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up) {
                            fileMentionPopup.moveUp()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.applyMentionSelection()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            fileMentionPopup.dismiss()
                            event.accepted = true
                        }
                    } else if (skillCommandPopup.visible) {
                        if (event.key === Qt.Key_Down) {
                            skillCommandPopup.moveDown()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up) {
                            skillCommandPopup.moveUp()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.applySkillSelection()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            skillCommandPopup.dismiss()
                            event.accepted = true
                        }
                    } else if (root.isSendShortcut(event.key, event.modifiers)) {
                        root.sendChatMessage()
                        event.accepted = true
                    }
                }

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

            isCompressing: root.isCompressing
            isProcessing: root.isRequestInProgress
            sendButton.onClicked: !root.isRequestInProgress ? root.sendChatMessage()
                                                            : root.cancelRequest()
            sendButton.icon.source: root.isRequestInProgress
                                    ? ""
                                    : (root.hasActiveError ? "qrc:/qt/qml/ChatView/icons/warning-icon.svg"
                                                           : "qrc:/qt/qml/ChatView/icons/chat-icon.svg")
            sendButton.text: root.isRequestInProgress ? qsTr("Stop") : qsTr("Send")
            sendButton.accentColor: (root.hasActiveError && !root.isRequestInProgress)
                                    ? root.errorColor : "transparent"
            sendButtonTooltip.text: root.isRequestInProgress
                                    ? qsTr("Stop")
                                    : (root.hasActiveError
                                       ? root.lastErrorMessage
                                       : qsTr("Send message to LLM %1").arg(root.sendShortcutText))
            compressButton.onClicked: compressConfirmDialog.open()
            cancelCompressButton.onClicked: root.cancelCompression()
            syncOpenFiles {
                checked: root.isSyncOpenFiles
                onCheckedChanged: root.setIsSyncOpenFiles(bottomBar.syncOpenFiles.checked)
            }
            attachFiles.onClicked: root.showAttachFilesDialog()
            attachImages.onClicked: root.showAddImageDialog()
            linkFiles.onClicked: root.showLinkFilesDialog()
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

    function focusInput() {
        messageInput.forceActiveFocus()
    }

    property Item focusGuard: Window.activeFocusItem
    onFocusGuardChanged: Qt.callLater(returnFocusToInputIfNeeded)

    function returnFocusToInputIfNeeded() {
        var item = Window.activeFocusItem
        if (!item || item === messageInput)
            return
        if (item.cursorVisible !== undefined || item.selectByMouse !== undefined)
            return
        if (item.popup !== undefined)
            return
        var p = item
        while (p) {
            if (p === root) {
                messageInput.forceActiveFocus()
                return
            }
            p = p.parent
        }
    }

    function applyMentionSelection() {
        var result = fileMentionPopup.applyCurrentSelection(
            messageInput.text, messageInput.cursorPosition, root.useTools)
        if (result.text !== undefined) {
            messageInput.text = result.text
            messageInput.cursorPosition = result.cursorPosition
        }
    }

    function applySkillSelection() {
        const name = skillCommandPopup.currentName()
        if (name === "")
            return
        const cursorPos = messageInput.cursorPosition
        const textBefore = messageInput.text.substring(0, cursorPos)
        const slashIndex = textBefore.lastIndexOf('/')
        if (slashIndex < 0)
            return
        const before = messageInput.text.substring(0, slashIndex)
        const after = messageInput.text.substring(cursorPos)
        const token = '/' + name + ' '
        messageInput.text = before + token + after
        messageInput.cursorPosition = before.length + token.length
        skillCommandPopup.dismiss()
    }

    function sendChatMessage() {
        root.hasActiveError = false
        root.sendMessage(fileMentionPopup.expandMentions(messageInput.text))
        messageInput.text = ""
        fileMentionPopup.clearMentions()
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

    Rectangle {
        id: errorBanner

        z: 1000
        visible: root.hasActiveError && root.lastErrorMessage.length > 0

        width: parent.width / 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 10
        anchors.bottomMargin: bottomBar.height + 48

        height: visible ? errorRow.implicitHeight + 12 : 0

        color: Qt.rgba(0.83, 0.18, 0.18, 0.96)
        radius: 6
        border.color: Qt.darker(color, 1.3)
        border.width: 1

        RowLayout {
            id: errorRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 6
            spacing: 8

            TextEdit {
                Layout.fillWidth: true
                text: root.lastErrorMessage
                color: "#FFFFFF"
                font.pixelSize: 12
                wrapMode: TextEdit.Wrap
                readOnly: true
                selectByMouse: true
                selectionColor: Qt.darker(errorBanner.color, 1.3)
            }

            Rectangle {
                id: copyErrorButton

                property bool copied: false

                Layout.alignment: Qt.AlignTop
                implicitWidth: copyErrorLabel.implicitWidth + 18
                implicitHeight: 22
                radius: 4
                color: copyErrorMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.28)
                                                    : Qt.rgba(1, 1, 1, 0.16)
                border.color: Qt.rgba(1, 1, 1, 0.45)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 120 } }

                Text {
                    id: copyErrorLabel

                    anchors.centerIn: parent
                    text: copyErrorButton.copied ? qsTr("Copied") : qsTr("Copy")
                    color: "#FFFFFF"
                    font.pixelSize: 12
                }

                MouseArea {
                    id: copyErrorMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.copyToClipboard(root.lastErrorMessage)
                        copyErrorButton.copied = true
                        copyErrorResetTimer.restart()
                    }
                }

                Timer {
                    id: copyErrorResetTimer

                    interval: 1500
                    onTriggered: copyErrorButton.copied = false
                }
            }

            Rectangle {
                id: closeErrorButton

                Layout.alignment: Qt.AlignTop
                implicitWidth: 22
                implicitHeight: 22
                radius: 4
                color: closeErrorMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.28) : "transparent"
                border.color: Qt.rgba(1, 1, 1, 0.45)
                border.width: closeErrorMouse.containsMouse ? 1 : 0

                Behavior on color { ColorAnimation { duration: 120 } }

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                }

                MouseArea {
                    id: closeErrorMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.hasActiveError = false
                }
            }
        }
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

        onOpenSettings: root.openSettings()
    }

    Connections {
        target: root
        function onLastErrorMessageChanged() {
            if (root.lastErrorMessage.length > 0) {
                root.hasActiveError = true
            }
        }
        function onLastInfoMessageChanged() {
            if (root.lastInfoMessage.length > 0) {
                infoToast.show(root.lastInfoMessage)
            }
        }
        function onOpenFilesChanged() {
            if (fileMentionPopup.visible)
                Qt.callLater(fileMentionPopup.refreshSearch)
        }
    }

    FileMentionPopup {
        id: fileMentionPopup

        z: 999
        width: Math.min(480, root.width - 20)

        x: Math.max(5, Math.min(view.x + 5, root.width - width - 5))
        y: view.y - height - 4

        onSelectionRequested: root.applyMentionSelection()

        onFileAttachRequested: function(filePaths) {
            root.addFilesToAttachList(filePaths)
        }
    }

    SkillCommandPopup {
        id: skillCommandPopup

        z: 999
        width: Math.min(480, root.width - 20)

        x: Math.max(5, Math.min(view.x + 5, root.width - width - 5))
        y: view.y - height - 4

        skillProvider: root

        onSelectionRequested: root.applySkillSelection()
    }

    Component.onCompleted: {
        focusInput()
    }
}
