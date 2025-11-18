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
        
        delegate: Rectangle {
            required property int index
            required property string modelData
            
            height: 30
            width: contentRow.width + 10
            radius: 4
            color: palette.button
            border.width: 1
            border.color: mouse.hovered ? palette.highlight : root.accentColor

            HoverHandler {
                id: mouse
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
