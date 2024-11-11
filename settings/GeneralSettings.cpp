/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include "GeneralSettings.hpp"

#include <QInputDialog>
#include <QMessageBox>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <utils/utilsicons.h>

#include "Logger.hpp"
#include "SettingsConstants.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"

namespace QodeAssist::Settings {

GeneralSettings &generalSettings()
{
    static GeneralSettings settings;
    return settings;
}

GeneralSettings::GeneralSettings()
{
    setAutoApply(false);

    setDisplayName(TrConstants::GENERAL);

    enableQodeAssist.setSettingsKey(Constants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setLabelText(TrConstants::ENABLE_QODE_ASSIST);
    enableQodeAssist.setDefaultValue(true);

    enableLogging.setSettingsKey(Constants::ENABLE_LOGGING);
    enableLogging.setLabelText(TrConstants::ENABLE_LOG);
    enableLogging.setDefaultValue(false);

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;

    initStringAspect(ccProvider, Constants::CC_PROVIDER, TrConstants::PROVIDER, "Ollama");
    ccProvider.setReadOnly(true);
    ccSelectProvider.m_buttonText = TrConstants::SELECT;

    initStringAspect(ccModel, Constants::CC_MODEL, TrConstants::MODEL, "codellama:7b-code");
    ccModel.setHistoryCompleter(Constants::CC_MODEL_HISTORY);
    ccSelectModel.m_buttonText = TrConstants::SELECT;

    initStringAspect(ccTemplate, Constants::CC_TEMPLATE, TrConstants::TEMPLATE, "CodeLlama FIM");
    ccTemplate.setReadOnly(true);
    ccSelectTemplate.m_buttonText = TrConstants::SELECT;

    initStringAspect(ccUrl, Constants::CC_URL, TrConstants::URL, "http://localhost:11434");
    ccUrl.setHistoryCompleter(Constants::CC_URL_HISTORY);
    ccSetUrl.m_buttonText = TrConstants::SELECT;

    ccStatus.setDisplayStyle(Utils::StringAspect::LabelDisplay);
    ccStatus.setLabelText(TrConstants::STATUS);
    ccStatus.setDefaultValue("");
    ccTest.m_buttonText = TrConstants::TEST;

    initStringAspect(caProvider, Constants::CA_PROVIDER, TrConstants::PROVIDER, "Ollama");
    caProvider.setReadOnly(true);
    caSelectProvider.m_buttonText = TrConstants::SELECT;

    initStringAspect(caModel, Constants::CA_MODEL, TrConstants::MODEL, "codellama:7b-instruct");
    caModel.setHistoryCompleter(Constants::CA_MODEL_HISTORY);
    caSelectModel.m_buttonText = TrConstants::SELECT;

    initStringAspect(caTemplate, Constants::CA_TEMPLATE, TrConstants::TEMPLATE, "CodeLlama Chat");
    caTemplate.setReadOnly(true);

    caSelectTemplate.m_buttonText = TrConstants::SELECT;

    initStringAspect(caUrl, Constants::CA_URL, TrConstants::URL, "http://localhost:11434");
    caUrl.setHistoryCompleter(Constants::CA_URL_HISTORY);
    caSetUrl.m_buttonText = TrConstants::SELECT;

    caStatus.setDisplayStyle(Utils::StringAspect::LabelDisplay);
    caStatus.setLabelText(TrConstants::STATUS);
    caStatus.setDefaultValue("");
    caTest.m_buttonText = TrConstants::TEST;

    readSettings();

    Logger::instance().setLoggingEnabled(enableLogging());

    setupConnections();

    setLayouter([this]() {
        using namespace Layouting;

        auto ccGrid = Grid{};
        ccGrid.addRow({ccProvider, ccSelectProvider});
        ccGrid.addRow({ccModel, ccSelectModel});
        ccGrid.addRow({ccTemplate, ccSelectTemplate});
        ccGrid.addRow({ccUrl, ccSetUrl});
        ccGrid.addRow({ccStatus, ccTest});

        auto caGrid = Grid{};
        caGrid.addRow({caProvider, caSelectProvider});
        caGrid.addRow({caModel, caSelectModel});
        caGrid.addRow({caTemplate, caSelectTemplate});
        caGrid.addRow({caUrl, caSetUrl});
        caGrid.addRow({caStatus, caTest});

        auto ccGroup = Group{title(TrConstants::CODE_COMPLETION), ccGrid};
        auto caGroup = Group{title(TrConstants::CHAT_ASSISTANT), caGrid};

        auto rootLayout = Column{Row{enableQodeAssist, Stretch{1}, resetToDefaults},
                                 Row{enableLogging, Stretch{1}},
                                 Space{8},
                                 ccGroup,
                                 Space{8},
                                 caGroup,
                                 Stretch{1}};

        return rootLayout;
    });
}

void GeneralSettings::showSelectionDialog(const QStringList &data,
                                          Utils::StringAspect &aspect,
                                          const QString &title,
                                          const QString &text)
{
    if (data.isEmpty())
        return;

    bool ok;
    QInputDialog dialog(Core::ICore::dialogParent());
    dialog.setWindowTitle(title);
    dialog.setLabelText(text);
    dialog.setComboBoxItems(data);
    dialog.setComboBoxEditable(false);
    dialog.setFixedSize(400, 150);

    if (dialog.exec() == QDialog::Accepted) {
        QString result = dialog.textValue();
        if (!result.isEmpty()) {
            aspect.setValue(result);
            writeSettings();
        }
    }
}

void GeneralSettings::setupConnections()
{
    connect(&enableLogging, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        Logger::instance().setLoggingEnabled(enableLogging.volatileValue());
    });
    connect(&resetToDefaults, &ButtonAspect::clicked, this, &GeneralSettings::resetPageToDefaults);
}

void GeneralSettings::resetPageToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(Core::ICore::dialogParent(),
                                  TrConstants::RESET_SETTINGS,
                                  TrConstants::CONFIRMATION,
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        resetAspect(enableQodeAssist);
        resetAspect(enableLogging);
        resetAspect(ccProvider);
        resetAspect(ccModel);
        resetAspect(ccTemplate);
        resetAspect(ccUrl);
        resetAspect(caProvider);
        resetAspect(caModel);
        resetAspect(caTemplate);
        resetAspect(caUrl);
        writeSettings();
    }
}

class GeneralSettingsPage : public Core::IOptionsPage
{
public:
    GeneralSettingsPage()
    {
        setId(Constants::QODE_ASSIST_GENERAL_SETTINGS_PAGE_ID);
        setDisplayName(TrConstants::GENERAL);
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setSettingsProvider([] { return &generalSettings(); });
    }
};

const GeneralSettingsPage generalSettingsPage;

} // namespace QodeAssist::Settings
