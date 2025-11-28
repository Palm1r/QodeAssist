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

Item {
    id: root

    signal filesDroppedToAttach(var urlStrings)
    signal filesDroppedToLink(var urlStrings)

    property string activeZone: ""
    property int filesCount: 0
    property bool isDragActive: false

    Item {
        id: splitDropOverlay

        anchors.fill: parent
        visible: false
        z: 999
        opacity: 0

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(palette.shadow.r, palette.shadow.g, palette.shadow.b, 0.6)
        }

        Rectangle {
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 30
            }
            width: fileCountText.width + 40
            height: 50
            color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.9)
            radius: 25
            visible: root.filesCount > 0
            
            Text {
                id: fileCountText
                anchors.centerIn: parent
                text: qsTr("%n file(s) to drop", "", root.filesCount)
                font.pixelSize: 16
                font.bold: true
                color: palette.highlightedText
            }
        }

        Rectangle {
            id: leftZone

            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: parent.width / 2
            color: root.activeZone === "left"
                   ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.3)
                   : Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.15)
            border.width: root.activeZone === "left" ? 3 : 2
            border.color: root.activeZone === "left"
                          ? palette.highlight
                          : Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.5)

            Column {
                anchors.centerIn: parent
                spacing: 15

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Attach")
                    font.pixelSize: 24
                    font.bold: true
                    color: root.activeZone === "left" ? palette.highlightedText : palette.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Images & Text Files")
                    font.pixelSize: 14
                    color: root.activeZone === "left" ? palette.highlightedText : palette.text
                    opacity: 0.8
                }
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("(for one-time use)")
                    font.pixelSize: 12
                    font.italic: true
                    color: root.activeZone === "left" ? palette.highlightedText : palette.text
                    opacity: 0.6
                }
            }

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }

        Rectangle {
            id: rightZone

            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            width: parent.width / 2
            color: root.activeZone === "right"
                   ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.3)
                   : Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.15)
            border.width: root.activeZone === "right" ? 3 : 2
            border.color: root.activeZone === "right"
                          ? palette.highlight
                          : Qt.rgba(palette.mid.r, palette.mid.g, palette.mid.b, 0.5)

            Column {
                anchors.centerIn: parent
                spacing: 15

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("LINK")
                    font.pixelSize: 24
                    font.bold: true
                    color: root.activeZone === "right" ? palette.highlightedText : palette.text
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Text Files")
                    font.pixelSize: 14
                    color: root.activeZone === "right" ? palette.highlightedText : palette.text
                    opacity: 0.8
                }
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("(added to context)")
                    font.pixelSize: 12
                    font.italic: true
                    color: root.activeZone === "right" ? palette.highlightedText : palette.text
                    opacity: 0.6
                }
            }

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
        }

        Rectangle {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                bottom: parent.bottom
            }
            width: 2
            color: palette.mid
            opacity: 0.4
        }

        MouseArea {
            id: leftDropArea

            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: parent.width / 2
            hoverEnabled: true

            onEntered: {
                root.activeZone = "left"
            }
        }

        MouseArea {
            id: rightDropArea

            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            width: parent.width / 2
            hoverEnabled: true

            onEntered: {
                root.activeZone = "right"
            }
        }
    }

    DropArea {
        id: globalDropArea

        anchors.fill: parent
        
        onEntered: (drag) => {
                       if (drag.hasUrls) {
                           root.isDragActive = true
                           root.filesCount = drag.urls.length
                           splitDropOverlay.visible = true
                           splitDropOverlay.opacity = 1
                           root.activeZone = ""
                       }
                   }

        onExited: {
            root.isDragActive = false
            root.filesCount = 0
            splitDropOverlay.opacity = 0

            Qt.callLater(function() {
                if (!root.isDragActive) {
                    splitDropOverlay.visible = false
                    root.activeZone = ""
                }
            })
        }

        onPositionChanged: (drag) => {
                               if (drag.hasUrls) {
                                   root.activeZone = drag.x < globalDropArea.width / 2 ? "left" : "right"
                               }
                           }

        onDropped: (drop) => {
                       const targetZone = root.activeZone
                       root.isDragActive = false
                       root.filesCount = 0
                       splitDropOverlay.opacity = 0

                       Qt.callLater(function() {
                           splitDropOverlay.visible = false
                           root.activeZone = ""
                       })

                       if (!drop.hasUrls || drop.urls.length === 0) {
                           return
                       }

                       var urlStrings = []
                       for (var i = 0; i < drop.urls.length; i++) {
                           var urlString = drop.urls[i].toString()
                           if (urlString.startsWith("file://") || urlString.indexOf("://") === -1) {
                               urlStrings.push(urlString)
                           }
                       }

                       if (urlStrings.length === 0) {
                           return
                       }

                       drop.accept(Qt.CopyAction)

                       if (targetZone === "right") {
                           root.filesDroppedToLink(urlStrings)
                       } else {
                           root.filesDroppedToAttach(urlStrings)
                       }
                   }
    }
}

