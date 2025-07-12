import QtQuick
import TaskFlow.Editor

TaskItem{
    id: root

    width: 280
    height: Math.max(200, contentColumn.height + 40)

    DragHandler {
        id: dragHandler

        target: root
        onActiveChanged: {
            if (active) {
                root.z = 1000; // Поднять над остальными
            } else {
                root.z = 0;
            }
        }
    }

    // Task node background
    Rectangle {
        anchors.fill: parent
        color: palette.window
        border.color: palette.shadow
        border.width: 1
        radius: 6

        // Task header
        Rectangle {
            id: taskHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 40
            color: palette.button
            radius: 6

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.radius
                color: parent.color
            }

            Text {
                anchors.centerIn: parent
                // text: root.taskType
                color: palette.buttonText
                font.pixelSize: 14
                font.bold: true
            }
        }

        // Task content
        Column {
            id: contentColumn
            anchors.top: taskHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            spacing: 8

            // Task ID
            Text {
                text: "ID: " + root.taskId
                color: palette.text
                font.pixelSize: 11
                width: parent.width
                elide: Text.ElideRight
            }

            // Parameters section
            Item {
                width: parent.width
                height: paramColumn.height
                // visible: root.parameters && root.parameters.rowCount() > 0

                Column {
                    id: paramColumn
                    width: parent.width
                    spacing: 6

                    Text {
                        text: "Parameters:"
                        color: palette.text
                        font.pixelSize: 10
                        font.bold: true
                    }

                    Repeater {
                        model: root.parameters
                        delegate: Rectangle {
                            width: parent.width
                            height: 24
                            color: palette.base
                            radius: 4

                            Row {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 8
                                spacing: 6

                                Text {
                                    text: paramKey + ":"
                                    color: palette.text
                                    font.pixelSize: 9
                                    font.bold: true
                                }

                                Text {
                                    text: paramValue
                                    color: palette.windowText
                                    font.pixelSize: 9
                                    width: Math.min(150, implicitWidth)
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Input ports section (left side)
    Column {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: -8
        spacing: 6
        // visible: root.inputPorts && root.inputPorts.rowCount() > 0

        // Input label
        Text {
            text: "IN"
            color: palette.highlight
            font.pixelSize: 10
            font.bold: true
            anchors.left: parent.left
            anchors.leftMargin: -20
        }

        // Repeater {
        //     model: root.inputPorts
        //     delegate: Row {
        //         spacing: 6

        //         Text {
        //             text: taskPortName
        //             color: palette.text
        //             font.pixelSize: 9
        //             anchors.verticalCenter: parent.verticalCenter
        //             horizontalAlignment: Text.AlignRight
        //             width: 60
        //             elide: Text.ElideLeft
        //         }

        //         TaskPort {
        //             port: taskPortData
        //             isInput: true
        //         }
        //     }
        // }
    }

    // Output ports section (right side)
    Column {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: -10
        spacing: 8
        // visible: root.outputPorts && root.outputPorts.rowCount() > 0

        // Output label
        Text {
            text: "OUT"
            color: palette.highlight
            font.pixelSize: 10
            font.bold: true
            anchors.right: parent.right
            anchors.rightMargin: -24
        }

        // Repeater {
        //     model: root.outputPorts
        //     delegate: Row {
        //         spacing: 6

        //         TaskPort {
        //             port: taskPortData
        //             isInput: false
        //         }

        //         Text {
        //             text: taskPortName
        //             color: palette.text
        //             font.pixelSize: 9
        //             anchors.verticalCenter: parent.verticalCenter
        //             width: 60
        //             elide: Text.ElideRight
        //         }
        //     }
        // }
    }
}
