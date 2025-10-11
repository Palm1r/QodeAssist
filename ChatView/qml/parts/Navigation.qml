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
import UIControls

Row {
    id: root

    property alias currentMessageNumber: currentMessageNumber.text

    signal messageUp()
    signal messageDown()

    spacing: 4

    QoAButton {
        text: "▲"
        onClicked: root.messageUp()

        ToolTip.visible: hovered
        ToolTip.delay: 250
        ToolTip.text: qsTr("Previous message")
    }

    QoAButton {
        text: "▼"
        onClicked: root.messageDown()

        ToolTip.visible: hovered
        ToolTip.delay: 250
        ToolTip.text: qsTr("Next message")
    }

    Badge {
        id: currentMessageNumber

        anchors.verticalCenter: parent.verticalCenter

        ToolTip.visible: hovered
        ToolTip.delay: 250
        ToolTip.text: qsTr("Current message position")
    }
}
