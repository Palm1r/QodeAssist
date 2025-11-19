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
import QtQuick.Layouts
import ChatView

Flow {
    id: root
    
    property alias attachedFilesModel: attachRepeater.model
    property color accentColor: palette.mid
    property string iconPath

    signal removeFileFromListByIndex(index: int)

    spacing: 5
    leftPadding: 5
    rightPadding: 5
    topPadding: attachRepeater.model.length > 0 ? 2 : 0
    bottomPadding: attachRepeater.model.length > 0 ? 2 : 0
    
    Repeater {
        id: attachRepeater
        
        delegate: FileItem {
            id: fileItem

            required property int index
            required property string modelData

            filePath: modelData
            
            height: 30
            width: contentRow.width + 10

            Rectangle {
                anchors.fill: parent
                radius: 4
                color: palette.button
                border.width: 1
                border.color: mouse.containsMouse ? palette.highlight : root.accentColor
            }

            MouseArea {
                id: mouse

                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        contextMenu.popup()
                    } else if (mouse.button === Qt.MiddleButton ||
                        (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier))) {
                        root.removeFileFromListByIndex(fileItem.index)
                    } else if (mouse.modifiers & Qt.ShiftModifier) {
                        fileItem.openFileInExternalEditor()
                    } else {
                        fileItem.openFileInEditor()
                    }
                }

                ToolTip.visible: containsMouse
                ToolTip.delay: 500
                ToolTip.text: "Click: Open in Qt Creator\nShift+Click: Open in external editor\nCtrl+Click / Middle Click: Remove"
            }

            Menu {
                id: contextMenu

                MenuItem {
                    text: "Open in Qt Creator"
                    onTriggered: fileItem.openFileInEditor()
                }

                MenuItem {
                    text: "Open in External Editor"
                    onTriggered: fileItem.openFileInExternalEditor()
                }

                MenuSeparator {}

                MenuItem {
                    text: "Remove"
                    onTriggered: root.removeFileFromListByIndex(fileItem.index)
                }
            }
            
            Row {
                id: contentRow

                spacing: 5
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 5

                Image {
                    id: icon

                    anchors.verticalCenter: parent.verticalCenter
                    source: root.iconPath
                    sourceSize.width: 8
                    sourceSize.height: 15
                }
                
                Text {
                    id: fileNameText
                    
                    anchors.verticalCenter: parent.verticalCenter
                    color: palette.buttonText
                    
                    text: {
                        const parts = modelData.split('/');
                        return parts[parts.length - 1];
                    }
                }
                
                MouseArea {
                    id: closeButton
                    
                    anchors.verticalCenter: parent.verticalCenter
                    width: closeIcon.width + 5
                    height: closeButton.width + 5
                    
                    onClicked: root.removeFileFromListByIndex(index)
                    
                    Image {
                        id: closeIcon
                        
                        anchors.centerIn: parent
                        source: palette.window.hslLightness > 0.5 ? "qrc:/qt/qml/ChatView/icons/close-dark.svg"
                                                                  : "qrc:/qt/qml/ChatView/icons/close-light.svg"
                        width: 6
                        height: 6
                    }
                }
            }
        }
    }
}
