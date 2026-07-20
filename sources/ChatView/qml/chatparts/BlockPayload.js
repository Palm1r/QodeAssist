// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

.pragma library

function parseMarkerPayload(marker, content) {
    if (!content || !content.startsWith(marker))
        return null;

    try {
        const parsed = JSON.parse(content.substring(marker.length));
        return (parsed && typeof parsed === "object") ? parsed : null;
    } catch (e) {
        return null;
    }
}

function parseFileEdit(content) {
    return parseMarkerPayload("QODEASSIST_FILE_EDIT:", content);
}

function parsePermission(content) {
    return parseMarkerPayload("QODEASSIST_PERMISSION:", content);
}

function parsePlan(content) {
    return parseMarkerPayload("QODEASSIST_PLAN:", content);
}
