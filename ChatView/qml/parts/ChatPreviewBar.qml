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

Rectangle {
    id: root

    property ListView targetView: null
    property int previewWidth: 50
    property color userMessageColor: "#92BD6C"
    property color assistantMessageColor: palette.button

    width: previewWidth
    color: palette.window.hslLightness > 0.5 ?
           Qt.darker(palette.window, 1.1) :
           Qt.lighter(palette.window, 1.1)

    Behavior on opacity {
        NumberAnimation {
            duration: 150
            easing.type: Easing.InOutQuad
        }
    }

    Column {
        id: previewContainer
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2

        Repeater {
            model: targetView ? targetView.model : null

            Rectangle {
                required property int index
                required property var model

                width: parent.width
                height: {
                    if (!targetView || !targetView.count) return 0
                    const availableHeight = root.height - ((targetView.count - 1) * previewContainer.spacing)
                    return availableHeight / targetView.count
                }

                radius: 4
                color: model.roleType === ChatModel.User ?
                       userMessageColor :
                       assistantMessageColor

                opacity: root.opacity
                transform: Translate {
                    x: root.opacity * 50 - 50
                }

                Behavior on transform {
                    NumberAnimation {
                        duration: 150
                        easing.type: Easing.InOutQuad
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (targetView) {
                            targetView.positionViewAtIndex(index, ListView.Center)
                        }
                    }

                    HoverHandler {
                        id: hover
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: palette.highlight
                    opacity: hover.hovered ? 0.2 : 0
                    radius: parent.radius

                    Behavior on opacity {
                        NumberAnimation { duration: 150 }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: palette.highlight
                    opacity: {
                        if (!targetView) return 0
                        const viewY = targetView.contentY
                        const viewHeight = targetView.height
                        const totalHeight = targetView.contentHeight
                        const itemPosition = index / targetView.count * totalHeight
                        const itemHeight = totalHeight / targetView.count

                        return (itemPosition + itemHeight > viewY &&
                                itemPosition < viewY + viewHeight) ? 0.2 : 0
                    }
                    radius: parent.radius

                    Behavior on opacity {
                        NumberAnimation { duration: 150 }
                    }
                }

                ToolTip.visible: hover.hovered
                ToolTip.text: {
                    const maxPreviewLength = 100
                    return model.content.length > maxPreviewLength ?
                           model.content.substring(0, maxPreviewLength) + "..." :
                           model.content
                }
            }
        }
    }
}
