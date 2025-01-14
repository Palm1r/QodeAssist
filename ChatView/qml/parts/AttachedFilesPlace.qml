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
import QtQuick.Layouts
import ChatView

Flow {
    id: attachFilesPlace
    
    property alias attachedFilesModel: attachRepeater.model

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
            width: fileNameText.width + closeButton.width + 20
            radius: 4
            color: palette.button
            border.width: 1
            border.color: palette.mid
            
            Row {
                spacing: 5
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 5
                
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
                    width: closeIcon.width
                    height: closeButton.width
                    
                    onClicked: {
                        const newList = [...root.attachmentFiles];
                        newList.splice(index, 1);
                        root.attachmentFiles = newList;
                    }
                    
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
