import QtQuick
import TaskFlow.Editor

TaskPortItem {
    id: root

    property bool isInput: true

    width: 20
    height: 20

    // Port circle
    Rectangle {
        id: portCircle
        anchors.centerIn: parent
        width: 16
        height: 16
        radius: 8
        color: getPortColor()
        border.color: palette.windowText
        border.width: 1

        // Inner circle for connected state simulation
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            color: root.port ? palette.windowText : "transparent"
            visible: root.port !== null
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onEntered: {
            portCircle.scale = 1.3
            portCircle.border.width = 2
        }

        onExited: {
            portCircle.scale = 1.0
            portCircle.border.width = 1
        }
    }

    function getPortColor() {
        if (!root.port) return palette.mid

        // Different colors for input/output using system palette
        if (root.isInput) {
            return palette.highlight  // System highlight color for inputs
        } else {
            return Qt.lighter(palette.highlight, 1.3)  // Lighter highlight for outputs
        }
    }

    Behavior on scale {
        NumberAnimation { duration: 100 }
    }
}
