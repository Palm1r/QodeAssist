/*
 * Copyright (C) 2025 Petr Mironychev
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
import QtQuick.Layouts
import UIControls

Rectangle {
    id: root

    property int totalEdits: 0
    property int appliedEdits: 0
    property int pendingEdits: 0
    property int rejectedEdits: 0
    property bool hasAppliedEdits: appliedEdits > 0
    property bool hasRejectedEdits: rejectedEdits > 0
    property bool hasPendingEdits: pendingEdits > 0

    signal applyAllClicked()
    signal undoAllClicked()

    visible: totalEdits > 0
    implicitHeight: visible ? 40 : 0

    color: palette.window.hslLightness > 0.5 ?
               Qt.darker(palette.window, 1.05) :
               Qt.lighter(palette.window, 1.05)

    border.width: 1
    border.color: palette.mid

    Behavior on implicitHeight {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }

    RowLayout {
        anchors {
            left: parent.left
            leftMargin: 10
            right: parent.right
            rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            radius: 12
            color: {
                if (root.hasPendingEdits) return Qt.rgba(0.2, 0.6, 1.0, 0.2)
                if (root.hasAppliedEdits) return Qt.rgba(0.2, 0.8, 0.2, 0.2)
                return Qt.rgba(0.8, 0.6, 0.2, 0.2)
            }
            border.width: 2
            border.color: {
                if (root.hasPendingEdits) return Qt.rgba(0.2, 0.6, 1.0, 0.8)
                if (root.hasAppliedEdits) return Qt.rgba(0.2, 0.8, 0.2, 0.8)
                return Qt.rgba(0.8, 0.6, 0.2, 0.8)
            }

            Text {
                anchors.centerIn: parent
                text: root.totalEdits
                font.pixelSize: 10
                font.bold: true
                color: palette.text
            }
        }

        // Status text
        ColumnLayout {
            spacing: 2

            Text {
                text: root.totalEdits === 1 
                    ? qsTr("File Edit in Current Message")
                    : qsTr("%1 File Edits in Current Message").arg(root.totalEdits)
                font.pixelSize: 11
                font.bold: true
                color: palette.text
            }

            Text {
                visible: root.totalEdits > 0
                text: {
                    let parts = [];
                    if (root.appliedEdits > 0) {
                        parts.push(qsTr("%1 applied").arg(root.appliedEdits));
                    }
                    if (root.pendingEdits > 0) {
                        parts.push(qsTr("%1 pending").arg(root.pendingEdits));
                    }
                    if (root.rejectedEdits > 0) {
                        parts.push(qsTr("%1 rejected").arg(root.rejectedEdits));
                    }
                    return parts.join(", ");
                }
                font.pixelSize: 9
                color: palette.mid
            }
        }

        Item {
            Layout.fillWidth: true
        }

        QoAButton {
            id: applyAllButton

            visible: root.hasPendingEdits || root.hasRejectedEdits
            enabled: root.hasPendingEdits || root.hasRejectedEdits
            text: root.hasPendingEdits 
                ? qsTr("Apply All (%1)").arg(root.pendingEdits + root.rejectedEdits)
                : qsTr("Reapply All (%1)").arg(root.rejectedEdits)
            
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: root.hasPendingEdits
                ? qsTr("Apply all pending and rejected edits in this message")
                : qsTr("Reapply all rejected edits in this message")

            onClicked: root.applyAllClicked()
        }

        QoAButton {
            id: undoAllButton

            visible: root.hasAppliedEdits
            enabled: root.hasAppliedEdits
            text: qsTr("Undo All (%1)").arg(root.appliedEdits)
            
            ToolTip.visible: hovered
            ToolTip.delay: 250
            ToolTip.text: qsTr("Undo all applied edits in this message")

            onClicked: root.undoAllClicked()
        }
    }
}

