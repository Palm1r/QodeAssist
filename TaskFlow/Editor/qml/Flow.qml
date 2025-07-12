import QtQuick
import TaskFlow.Editor

FlowItem {
    id: root

    Repeater {
        id: tasks

        model: root.taskModel
        delegate: Task {
            // task: taskData
        }
    }

    Repeater {
        id: connections

        model: root.taskModel
        delegate: TaskConnection {
            // task: taskData
        }
    }

    // property var qtaskItems: []

    // // Flow container background
    // Rectangle {
    //     anchors.fill: parent
    //     color: palette.alternateBase
    //     border.color: palette.mid
    //     border.width: 2
    //     radius: 8

    //     // Flow header
    //     Rectangle {
    //         id: flowHeader
    //         anchors.top: parent.top
    //         anchors.left: parent.left
    //         anchors.right: parent.right
    //         height: 40
    //         color: palette.button
    //         radius: 6

    //         Rectangle {
    //             anchors.bottom: parent.bottom
    //             anchors.left: parent.left
    //             anchors.right: parent.right
    //             height: parent.radius
    //             color: parent.color
    //         }

    //         Text {
    //             anchors.centerIn: parent
    //             text: root.flowId
    //             color: palette.buttonText
    //             font.pixelSize: 14
    //             font.bold: true
    //         }
    //     }

    //     // // Tasks container
    //     // Row {
    //     //     id: tasksRow
    //     //     anchors.top: flowHeader.bottom
    //     //     anchors.left: parent.left
    //     //     anchors.margins: 25
    //     //     anchors.topMargin: 25
    //     //     objectName: "FlowTaskRow"

    //     //     spacing: 40

    //     //     Repeater {
    //     //         model: root.taskModel

    //     //         delegate: Task {
    //     //             task: taskData
    //     //         }

    //     //         onItemAdded: function(index, item){
    //     //             console.log("task added", index, item)
    //     //             qtaskItems.push(item)
    //     //             root.insertTaskItem(index, item)
    //     //         }

    //     //         onItemRemoved: function(index, item){
    //     //             console.log("task added", index, item)
    //     //             var idx = qtaskItems.indexOf(item)
    //     //             if (idx !== -1) qtaskItems.splice(idx, 1)
    //     //         }
    //     //     }
    //     // }

    //     // Repeater {
    //     //     model: root.connectionsModel

    //     //     delegate: TaskConnection {
    //     //         connection: connectionData
    //     //     }
    //     // }
    // }

    // // Flow info tooltip
    // Rectangle {
    //     id: infoTooltip
    //     anchors.top: parent.bottom
    //     anchors.left: parent.left
    //     anchors.topMargin: 5
    //     width: infoText.width + 20
    //     height: infoText.height + 10
    //     color: palette.base
    //     border.color: palette.shadow
    //     border.width: 1
    //     radius: 4
    //     visible: false

    //     Text {
    //         id: infoText
    //         anchors.centerIn: parent
    //         text: "Tasks: " + (root.taskModel ? root.taskModel.rowCount() : 0)
    //         color: palette.text
    //         font.pixelSize: 10
    //     }
    // }

    // MouseArea {
    //     anchors.fill: parent
    //     hoverEnabled: true

    //     onEntered: infoTooltip.visible = true
    //     onExited: infoTooltip.visible = false
    // }

}
