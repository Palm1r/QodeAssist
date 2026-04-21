// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import TaskFlow.Editor

FlowEditor {
    id: root

    width: 1200
    height: 800

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

    // Background with grid pattern
    Rectangle {
        anchors.fill: parent
        color: palette.window

        // Grid pattern using C++ implementation
        GridBackground {
            anchors.fill: parent
            gridSize: 20
            gridColor: palette.mid
            opacity: 0.3
        }
    }

    // Header panel
    Rectangle {
        id: headerPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: palette.base
        border.color: palette.mid
        border.width: 1

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 20
            spacing: 20

            Text {
                text: "Flow Editor"
                color: palette.windowText
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle {
                width: 2
                height: 30
                color: palette.mid
            }

            Text {
                text: "Flow:"
                color: palette.text
                font.pixelSize: 14
            }

            ComboBox {
                id: flowComboBox

                model: root.flowsModel
                textRole: "flowId"
                currentIndex: root.currentFlowIndex

                onActivated: {
                    root.currentFlowIndex = currentIndex
                }
            }

            Text {
                text: "Available Tasks: " + root.availableTaskTypes.join(", ")
                color: palette.text
                font.pixelSize: 12
            }
        }
    }

    // Main flow area
    ScrollView {
        id: scrollView
        anchors.top: headerPanel.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        contentWidth: flow.width
        contentHeight: flow.height

        Flow {
            id: flow

            // flow: root.currentFlow

            width: Math.max(root.width, 0)
            height: Math.min(root.height, 0)
        }
    }
}
