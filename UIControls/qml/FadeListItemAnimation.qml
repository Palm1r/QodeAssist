import QtQuick

ParallelAnimation {
    id: root

    property Item targetObject: parent

    NumberAnimation {
        target: root.targetObject
        property: "opacity"
        to: 0
        duration: 200
        easing.type: Easing.InQuad
    }
    NumberAnimation {
        target: root.targetObject
        property: "height"
        to: 0
        duration: 250
        easing.type: Easing.InQuad
    }
}
