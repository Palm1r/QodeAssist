// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

import QtQuick

Item {
    id: root

    property bool running: true
    property color color: palette.highlightedText
    property real lineWidth: Math.max(1.5, Math.min(width, height) * 0.13)

    implicitWidth: 24
    implicitHeight: 24

    visible: opacity > 0
    opacity: running ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }

    Canvas {
        id: canvas

        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            const cx = width / 2
            const cy = height / 2
            const lw = root.lineWidth
            const radius = Math.min(width, height) / 2 - lw / 2 - 1
            if (radius <= 0)
                return

            const segments = 90
            const sweep = Math.PI * 1.5

            ctx.lineCap = "butt"
            ctx.lineWidth = lw
            for (let i = 0; i < segments; ++i) {
                const a0 = (i / segments) * sweep
                const a1 = ((i + 1) / segments) * sweep + 0.012
                const t = i / (segments - 1)
                ctx.beginPath()
                ctx.strokeStyle = Qt.rgba(root.color.r, root.color.g, root.color.b, 0.08 + 0.92 * t)
                ctx.arc(cx, cy, radius, a0, a1, false)
                ctx.stroke()
            }

            ctx.lineCap = "round"
            ctx.strokeStyle = root.color
            ctx.beginPath()
            ctx.arc(cx, cy, radius, sweep - 0.001, sweep, false)
            ctx.stroke()
        }

        RotationAnimator {
            target: canvas
            from: 0
            to: 360
            duration: 850
            loops: Animation.Infinite
            running: root.visible
        }
    }

    onColorChanged: canvas.requestPaint()
    onLineWidthChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}
