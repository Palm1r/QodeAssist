// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatAssistantSettings.hpp"

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <QApplication>
#include <QFontDatabase>
#include <QMessageBox>

#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

ChatAssistantSettings &chatAssistantSettings()
{
    static ChatAssistantSettings settings;
    return settings;
}

ChatAssistantSettings::ChatAssistantSettings()
{
    setAutoApply(false);

    setDisplayName(Tr::tr("Chat Assistant"));

    linkOpenFiles.setSettingsKey(Constants::CA_LINK_OPEN_FILES);
    linkOpenFiles.setLabelText(Tr::tr("Sync open files with assistant by default"));
    linkOpenFiles.setDefaultValue(false);

    autosave.setSettingsKey(Constants::CA_AUTOSAVE);
    autosave.setDefaultValue(true);
    autosave.setLabelText(Tr::tr("Enable autosave when message received"));

    enableChatInBottomToolBar.setSettingsKey(Constants::CA_ENABLE_CHAT_IN_BOTTOM_TOOLBAR);
    enableChatInBottomToolBar.setLabelText(Tr::tr("Enable chat in bottom toolbar"));
    enableChatInBottomToolBar.setDefaultValue(false);

    enableChatInNavigationPanel.setSettingsKey(Constants::CA_ENABLE_CHAT_IN_NAVIGATION_PANEL);
    enableChatInNavigationPanel.setLabelText(Tr::tr("Enable chat in navigation panel"));
    enableChatInNavigationPanel.setDefaultValue(false);

    autoCompress.setSettingsKey(Constants::CA_AUTO_COMPRESS);
    autoCompress.setLabelText(Tr::tr("Auto-compress chat when session tokens exceed:"));
    autoCompress.setToolTip(Tr::tr(
        "After each assistant response, if the running session token total exceeds the "
        "threshold, the chat is summarized and a new compressed chat is started "
        "automatically. The original chat is preserved on disk."));
    autoCompress.setDefaultValue(false);

    autoCompressThreshold.setSettingsKey(Constants::CA_AUTO_COMPRESS_THRESHOLD);
    autoCompressThreshold.setRange(1000, 99999999);
    autoCompressThreshold.setDefaultValue(40000);

    autosave.setDefaultValue(true);
    autosave.setLabelText(Tr::tr("Enable autosave when message received"));

    textFontFamily.setSettingsKey(Constants::CA_TEXT_FONT_FAMILY);
    textFontFamily.setLabelText(Tr::tr("Text Font:"));
    textFontFamily.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        textFontFamily.addOption(family);
    }
    textFontFamily.setDefaultValue(QApplication::font().family());

    textFontSize.setSettingsKey(Constants::CA_TEXT_FONT_SIZE);
    textFontSize.setLabelText(Tr::tr("Text Font Size:"));
    textFontSize.setDefaultValue(QApplication::font().pointSize());

    codeFontFamily.setSettingsKey(Constants::CA_CODE_FONT_FAMILY);
    codeFontFamily.setLabelText(Tr::tr("Code Font:"));
    codeFontFamily.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    const QStringList monospaceFamilies = QFontDatabase::families(QFontDatabase::Latin);
    for (const QString &family : monospaceFamilies) {
        if (QFontDatabase::isFixedPitch(family)) {
            codeFontFamily.addOption(family);
        }
    }

    QString defaultMonoFont;
    QStringList fixedPitchFamilies;

    for (const QString &family : monospaceFamilies) {
        if (QFontDatabase::isFixedPitch(family)) {
            fixedPitchFamilies.append(family);
        }
    }

    if (fixedPitchFamilies.contains("Consolas")) {
        defaultMonoFont = "Consolas";
    } else if (fixedPitchFamilies.contains("Courier New")) {
        defaultMonoFont = "Courier New";
    } else if (fixedPitchFamilies.contains("Monospace")) {
        defaultMonoFont = "Monospace";
    } else if (!fixedPitchFamilies.isEmpty()) {
        defaultMonoFont = fixedPitchFamilies.first();
    } else {
        defaultMonoFont = QApplication::font().family();
    }
    codeFontFamily.setDefaultValue(defaultMonoFont);
    codeFontSize.setSettingsKey(Constants::CA_CODE_FONT_SIZE);
    codeFontSize.setLabelText(Tr::tr("Code Font Size:"));
    codeFontSize.setDefaultValue(QApplication::font().pointSize());

    textFormat.setSettingsKey(Constants::CA_TEXT_FORMAT);
    textFormat.setLabelText(Tr::tr("Text Format:"));
    textFormat.setDefaultValue(0);
    textFormat.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    textFormat.addOption("Markdown");
    textFormat.addOption("HTML");
    textFormat.addOption("Plain Text");

    chatRenderer.setSettingsKey(Constants::CA_CHAT_RENDERER);
    chatRenderer.setLabelText(Tr::tr("Chat Renderer:"));
    chatRenderer.addOption("rhi");
    chatRenderer.addOption("software");
    chatRenderer.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
#ifdef Q_OS_WIN
    chatRenderer.setDefaultValue("software");
#else
    chatRenderer.setDefaultValue("rhi");
#endif

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;

    readSettings();

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto chatViewSettingsGrid = Grid{};
        chatViewSettingsGrid.addRow({textFontFamily, textFontSize});
        chatViewSettingsGrid.addRow({codeFontFamily, codeFontSize});
        chatViewSettingsGrid.addRow({textFormat});
        chatViewSettingsGrid.addRow({chatRenderer});

        return Column{
            Row{Stretch{1}, resetToDefaults},
            Space{8},
            Group{
                title(Tr::tr("Chat Settings")),
                Column{linkOpenFiles, autosave, Row{autoCompress, autoCompressThreshold, Stretch{1}}}},
            Space{8},
            Group{title(Tr::tr("Chat Settings")), Row{chatViewSettingsGrid, Stretch{1}}},
            Stretch{1}};
    });
}

void ChatAssistantSettings::setupConnections()
{
    connect(
        &resetToDefaults,
        &ButtonAspect::clicked,
        this,
        &ChatAssistantSettings::resetSettingsToDefaults);
}

void ChatAssistantSettings::resetSettingsToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
        Tr::tr("Reset Settings"),
        Tr::tr("Are you sure you want to reset all settings to default values?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(autoCompress);
        resetAspect(autoCompressThreshold);
        resetAspect(linkOpenFiles);
        resetAspect(textFontFamily);
        resetAspect(codeFontFamily);
        resetAspect(textFontSize);
        resetAspect(codeFontSize);
        resetAspect(textFormat);
        resetAspect(chatRenderer);
        writeSettings();
    }
}

class ChatAssistantSettingsPage : public Core::IOptionsPage
{
public:
    ChatAssistantSettingsPage()
    {
        setId(Constants::QODE_ASSIST_CHAT_ASSISTANT_SETTINGS_PAGE_ID);
        setDisplayName(Tr::tr("Chat Assistant"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &chatAssistantSettings(); });
    }
};

const ChatAssistantSettingsPage chatAssistantSettingsPage;

} // namespace QodeAssist::Settings
