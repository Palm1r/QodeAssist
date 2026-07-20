// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick
import QtQuick.Layouts
import UIControls
import "BlockPayload.js" as BlockPayload

Rectangle {
    id: root

    property string permissionContent: ""

    readonly property var permissionData: parsePermissionData(permissionContent)
    readonly property bool isMalformed: permissionData === null
    readonly property string requestId: isMalformed ? "" : (permissionData.requestId || "")
    readonly property string title: isMalformed
                                    ? qsTr("Unreadable permission request")
                                    : (permissionData.title || qsTr("The agent asks for permission"))
    readonly property string toolKind: isMalformed ? "" : (permissionData.toolKind || "")
    readonly property var options: isMalformed ? [] : (permissionData.options || [])
    readonly property string status: isMalformed ? "error" : (permissionData.status || "pending")
    readonly property string selectedOptionId: isMalformed ? "" : (permissionData.selectedOptionId || "")
    readonly property bool automatic: !isMalformed && permissionData.automatic === true

    readonly property bool isPending: status === "pending" && options.length > 0
    readonly property bool isCancelled: status === "cancelled"
    readonly property bool wasDeclinedUnpresentable: isCancelled && options.length === 0

    readonly property var selectedOption: findOption(selectedOptionId)
    readonly property bool wasAllowed: selectedOption !== null
                                       && selectedOption.allows === true

    readonly property int borderRadius: 6
    readonly property int contentMargin: 10
    readonly property int badgeRadius: 3
    readonly property int badgePaddingH: 12
    readonly property int badgePaddingV: 6
    readonly property int titleMaxLines: 4
    readonly property int rowSpacing: 8

    readonly property color allowedColor: Qt.rgba(0.2, 0.8, 0.2, 0.8)
    readonly property color deniedColor: Qt.rgba(0.8, 0.2, 0.2, 0.8)
    readonly property color cancelledColor: Qt.rgba(0.5, 0.5, 0.5, 0.8)

    readonly property color statusColor: {
        if (isMalformed)
            return deniedColor;
        if (isPending)
            return palette.highlight;
        if (isCancelled)
            return cancelledColor;
        return wasAllowed ? allowedColor : deniedColor;
    }

    readonly property string statusText: {
        if (isMalformed)
            return qsTr("UNREADABLE");
        if (isPending)
            return qsTr("WAITING FOR YOU");
        if (wasDeclinedUnpresentable)
            return qsTr("DECLINED");
        if (isCancelled)
            return qsTr("NO LONGER AVAILABLE");
        if (automatic)
            return wasAllowed ? qsTr("ALLOWED AUTOMATICALLY") : qsTr("DENIED AUTOMATICALLY");
        return wasAllowed ? qsTr("ALLOWED") : qsTr("DENIED");
    }

    readonly property string explanation: {
        if (isMalformed)
            return qsTr("This permission record could not be read, so it cannot be answered.");
        if (wasDeclinedUnpresentable)
            return qsTr("The agent did not offer a set of options this chat could present safely, so the request was declined.");
        if (isCancelled)
            return qsTr("This request ended before it was answered.");
        if (isPending)
            return "";
        if (selectedOption === null)
            return "";
        return automatic
                ? qsTr("Answered automatically with \"%1\", because you chose that for this action type for the rest of the conversation.").arg(selectedOption.name)
                : qsTr("You answered \"%1\".").arg(selectedOption.name);
    }

    signal respond(string requestId, string optionId)

    implicitHeight: layout.implicitHeight + 2 * contentMargin
    radius: borderRadius
    color: palette.base
    border.width: 1
    border.color: statusColor

    function parsePermissionData(content) {
        return BlockPayload.parsePermission(content);
    }

    function findOption(optionId) {
        if (!optionId)
            return null;
        for (let i = 0; i < root.options.length; ++i) {
            if (root.options[i].id === optionId)
                return root.options[i];
        }
        return null;
    }

    ColumnLayout {
        id: layout

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: root.contentMargin
        }
        spacing: root.rowSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: root.rowSpacing

            Rectangle {
                Layout.preferredWidth: statusLabel.implicitWidth + root.badgePaddingH
                Layout.preferredHeight: statusLabel.implicitHeight + root.badgePaddingV
                Layout.alignment: Qt.AlignTop
                radius: root.badgeRadius
                color: root.statusColor

                Text {
                    id: statusLabel

                    anchors.centerIn: parent
                    text: root.statusText
                    textFormat: Text.PlainText
                    font.pixelSize: 10
                    font.bold: true
                    color: palette.base
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.title
                textFormat: Text.PlainText
                font.pixelSize: 13
                font.bold: true
                color: palette.text
                wrapMode: Text.WordWrap
                maximumLineCount: root.titleMaxLines
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.toolKind.length > 0
            text: qsTr("Action type: %1").arg(root.toolKind)
            textFormat: Text.PlainText
            font.pixelSize: 11
            color: palette.placeholderText
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.explanation
            textFormat: Text.PlainText
            font.pixelSize: 11
            color: palette.placeholderText
            wrapMode: Text.WordWrap
        }

        Flow {
            Layout.fillWidth: true
            visible: root.isPending
            spacing: root.rowSpacing

            Repeater {
                model: root.options

                delegate: QoAButton {
                    id: optionButton

                    required property var modelData

                    text: optionButton.modelData.name || optionButton.modelData.id
                    accentColor: optionButton.modelData.allows === true
                                 ? root.allowedColor
                                 : root.deniedColor

                    contentItem: Text {
                        text: optionButton.text
                        textFormat: Text.PlainText
                        color: palette.buttonText
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    onClicked: root.respond(root.requestId, optionButton.modelData.id)
                }
            }
        }
    }
}
