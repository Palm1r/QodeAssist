import QtQuick
import QtQuick.Shapes
import TaskFlow.Editor

TaskConnectionItem {
    id: root

    property color connectionColor: "red"

    Rectangle {
        width: 10
        height: 10
        radius: width / 2
        color: "blue"
    }

    // width: Math.abs(endPoint.x - startPoint.x) + 40
    // height: Math.abs(endPoint.y - startPoint.y) + 40
    // x: Math.min(startPoint.x, endPoint.x) - 20
    // y: Math.min(startPoint.y, endPoint.y) - 20

    // Shape {
    //     anchors.fill: parent

    //     ShapePath {
    //         strokeWidth: 2
    //         strokeColor: connectionColor
    //         fillColor: "transparent"

    //         property point localStart: Qt.point(
    //             root.startPoint.x - root.x,
    //             root.startPoint.y - root.y
    //         )
    //         property point localEnd: Qt.point(
    //             root.endPoint.x - root.x,
    //             root.endPoint.y - root.y
    //         )

    //         // Bezier curve
    //         property real controlOffset: Math.max(50, Math.abs(localEnd.x - localStart.x) * 0.4)

    //         startX: localStart.x
    //         startY: localStart.y

    //         PathCubic {
    //             x: parent.localEnd.x
    //             y: parent.localEnd.y
    //             control1X: parent.localStart.x + parent.controlOffset
    //             control1Y: parent.localStart.y
    //             control2X: parent.localEnd.x - parent.controlOffset
    //             control2Y: parent.localEnd.y
    //         }
    //     }

    //     // Arrow head
    //     Rectangle {
    //         width: 8
    //         height: 8
    //         color: connectionColor
    //         rotation: 45
    //         x: root.endPoint.x - root.x - 4
    //         y: root.endPoint.y - root.y - 4
    //     }
    // }

    // // Update positions when tasks might have moved
    // Component.onCompleted: updatePositions()
}
