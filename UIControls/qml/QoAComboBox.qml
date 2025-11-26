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
import QtQuick.Controls.Basic as Basic

Basic.ComboBox {
    id: control

    implicitWidth: Math.min(contentItem.implicitWidth + 8, 300)
    implicitHeight: 30

    indicator: Image {
        id: dropdownIcon

        x: control.width - width - 10
        y: control.topPadding + (control.availableHeight - height) / 2

        width: 12
        height: 8
        source: palette.window.hslLightness > 0.5
                ? "qrc:/qt/qml/UIControls/icons/dropdown-arrow-light.svg"
                : "qrc:/qt/qml/UIControls/icons/dropdown-arrow-dark.svg"
        sourceSize: Qt.size(width, height)

        rotation: control.popup.visible ? 180 : 0

        Behavior on rotation {
            NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
        }
    }

    background: Rectangle {
        id: bg

        implicitWidth: control.implicitWidth
        implicitHeight: 30
        color: !control.enabled || !control.down ? palette.button : palette.dark
        border.color: !control.enabled || (!control.hovered && !control.visualFocus) 
                      ? palette.mid 
                      : palette.highlight
        border.width: 1
        radius: 5

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }

        Rectangle {
            anchors.fill: bg
            radius: bg.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.alpha(palette.highlight, 0.4) }
                GradientStop { position: 1.0; color: Qt.alpha(palette.highlight, 0.2) }
            }
            opacity: control.hovered ? 0.3 : 0.01
            
            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }
    }

    contentItem: Text {
        leftPadding: 10
        rightPadding: 30
        text: control.displayText
        font.pixelSize: 12
        color: palette.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideNone
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 300)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: ListView.StopAtBounds
            highlightMoveDuration: 0

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }

        background: Rectangle {
            color: palette.base
            border.color: Qt.lighter(palette.mid, 1.1)
            border.width: 1
            radius: 5
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
        }

        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 100 }
        }
    }

    delegate: ItemDelegate {
        width: control.width - 8
        height: 32

        contentItem: Text {
            text: modelData
            color: palette.text
            font.pixelSize: 12
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
        }

        highlighted: control.highlightedIndex === index

        background: Rectangle {
            radius: 4
            color: highlighted
                   ? Qt.alpha(palette.highlight, 0.2)
                   : parent.hovered
                   ? (palette.window.hslLightness > 0.5
                      ? Qt.darker(palette.base, 1.05)
                      : Qt.lighter(palette.base, 1.15))
                   : "transparent"

            Behavior on color {
                ColorAnimation { duration: 100 }
            }
        }
    }
}

