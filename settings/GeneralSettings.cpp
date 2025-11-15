/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include <utils/utilsicons.h>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QTextEdit>
#include <QTimer>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcompleter.h>
#include <QtWidgets/qgroupbox.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qstackedwidget.h>

#include "../Version.hpp"
#include "Logger.hpp"
#include "SettingsConstants.hpp"
#include "SettingsDialog.hpp"
#include "SettingsTr.hpp"
#include "SettingsUtils.hpp"
#include "UpdateDialog.hpp"

namespace QodeAssist::Settings {

void addDialogButtons(QBoxLayout *layout, QAbstractButton *okButton, QAbstractButton *cancelButton)
{
#if defined(Q_OS_MACOS)
    layout->addWidget(cancelButton);
    layout->addWidget(okButton);
#else
    layout->addWidget(okButton);
    layout->addWidget(cancelButton);
#endif
}

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
    enableLogging.setToolTip(TrConstants::ENABLE_LOG_TOOLTIP);
    enableLogging.setDefaultValue(false);

    enableCheckUpdate.setSettingsKey(Constants::ENABLE_CHECK_UPDATE);
    enableCheckUpdate.setLabelText(TrConstants::ENABLE_CHECK_UPDATE_ON_START);
    enableCheckUpdate.setDefaultValue(true);

    resetToDefaults.m_buttonText = TrConstants::RESET_TO_DEFAULTS;
    checkUpdate.m_buttonText = TrConstants::CHECK_UPDATE;

    initStringAspect(ccProvider, Constants::CC_PROVIDER, TrConstants::PROVIDER, "Ollama");
    ccProvider.setReadOnly(true);
    ccSelectProvider.m_buttonText = TrConstants::SELECT;

    initStringAspect(ccModel, Constants::CC_MODEL, TrConstants::MODEL, "qwen2.5-coder:7b");
    ccModel.setHistoryCompleter(Constants::CC_MODEL_HISTORY);
    ccSelectModel.m_buttonText = TrConstants::SELECT;

    initStringAspect(ccTemplate, Constants::CC_TEMPLATE, TrConstants::TEMPLATE, "Ollama FIM");
    ccTemplate.setReadOnly(true);
    ccSelectTemplate.m_buttonText = TrConstants::SELECT;

    initStringAspect(ccUrl, Constants::CC_URL, TrConstants::URL, "http://localhost:11434");
    ccUrl.setHistoryCompleter(Constants::CC_CUSTOM_ENDPOINT_HISTORY);
    ccSetUrl.m_buttonText = TrConstants::SELECT;

    ccEndpointMode.setSettingsKey(Constants::CC_ENDPOINT_MODE);
    ccEndpointMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    ccEndpointMode.addOption("Auto");
    ccEndpointMode.addOption("Custom");
    ccEndpointMode.addOption("FIM");
    ccEndpointMode.addOption("Chat");
    ccEndpointMode.setDefaultValue("Auto");

    initStringAspect(ccCustomEndpoint, Constants::CC_CUSTOM_ENDPOINT, TrConstants::ENDPOINT_MODE, "");
    ccCustomEndpoint.setHistoryCompleter(Constants::CC_CUSTOM_ENDPOINT_HISTORY);

    ccStatus.setDisplayStyle(Utils::StringAspect::LabelDisplay);
    ccStatus.setLabelText(TrConstants::STATUS);
    ccStatus.setDefaultValue("");
    ccTest.m_buttonText = TrConstants::TEST;

    ccTemplateDescription.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    ccTemplateDescription.setReadOnly(true);
    ccTemplateDescription.setDefaultValue("");

    // preset1
    specifyPreset1.setSettingsKey(Constants::CC_SPECIFY_PRESET1);
    specifyPreset1.setLabelText(TrConstants::ADD_NEW_PRESET);
    specifyPreset1.setDefaultValue(false);

    preset1Language.setSettingsKey(Constants::CC_PRESET1_LANGUAGE);
    preset1Language.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    // see ProgrammingLanguageUtils
    preset1Language.addOption("qml");
    preset1Language.addOption("c/c++");
    preset1Language.addOption("python");

    initStringAspect(
        ccPreset1Provider, Constants::CC_PRESET1_PROVIDER, TrConstants::PROVIDER, "Ollama");
    ccPreset1Provider.setReadOnly(true);
    ccPreset1SelectProvider.m_buttonText = TrConstants::SELECT;

    initStringAspect(
        ccPreset1Url, Constants::CC_PRESET1_URL, TrConstants::URL, "http://localhost:11434");
    ccPreset1Url.setHistoryCompleter(Constants::CC_PRESET1_URL_HISTORY);
    ccPreset1SetUrl.m_buttonText = TrConstants::SELECT;

    ccPreset1EndpointMode.setSettingsKey(Constants::CC_PRESET1_ENDPOINT_MODE);
    ccPreset1EndpointMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    ccPreset1EndpointMode.addOption("Auto");
    ccPreset1EndpointMode.addOption("Custom");
    ccPreset1EndpointMode.addOption("FIM");
    ccPreset1EndpointMode.addOption("Chat");
    ccPreset1EndpointMode.setDefaultValue("Auto");

    initStringAspect(
        ccPreset1CustomEndpoint,
        Constants::CC_PRESET1_CUSTOM_ENDPOINT,
        TrConstants::ENDPOINT_MODE,
        "");
    ccPreset1CustomEndpoint.setHistoryCompleter(Constants::CC_PRESET1_CUSTOM_ENDPOINT_HISTORY);

    initStringAspect(
        ccPreset1Model, Constants::CC_PRESET1_MODEL, TrConstants::MODEL, "qwen2.5-coder:7b");
    ccPreset1Model.setHistoryCompleter(Constants::CC_PRESET1_MODEL_HISTORY);
    ccPreset1SelectModel.m_buttonText = TrConstants::SELECT;

    initStringAspect(
        ccPreset1Template, Constants::CC_PRESET1_TEMPLATE, TrConstants::TEMPLATE, "Ollama FIM");
    ccPreset1Template.setReadOnly(true);
    ccPreset1SelectTemplate.m_buttonText = TrConstants::SELECT;

    // chat assistance
    initStringAspect(caProvider, Constants::CA_PROVIDER, TrConstants::PROVIDER, "Ollama");
    caProvider.setReadOnly(true);
    caSelectProvider.m_buttonText = TrConstants::SELECT;

    initStringAspect(caModel, Constants::CA_MODEL, TrConstants::MODEL, "qwen2.5-coder:7b");
    caModel.setHistoryCompleter(Constants::CA_MODEL_HISTORY);
    caSelectModel.m_buttonText = TrConstants::SELECT;

    initStringAspect(caTemplate, Constants::CA_TEMPLATE, TrConstants::TEMPLATE, "Ollama Chat");
    caTemplate.setReadOnly(true);

    caSelectTemplate.m_buttonText = TrConstants::SELECT;

    initStringAspect(caUrl, Constants::CA_URL, TrConstants::URL, "http://localhost:11434");
    caUrl.setHistoryCompleter(Constants::CA_URL_HISTORY);
    caSetUrl.m_buttonText = TrConstants::SELECT;

    caEndpointMode.setSettingsKey(Constants::CA_ENDPOINT_MODE);
    caEndpointMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    caEndpointMode.addOption("Auto");
    caEndpointMode.addOption("Custom");
    caEndpointMode.addOption("FIM");
    caEndpointMode.addOption("Chat");
    caEndpointMode.setDefaultValue("Auto");

    initStringAspect(caCustomEndpoint, Constants::CA_CUSTOM_ENDPOINT, TrConstants::ENDPOINT_MODE, "");
    caCustomEndpoint.setHistoryCompleter(Constants::CA_CUSTOM_ENDPOINT_HISTORY);

    caStatus.setDisplayStyle(Utils::StringAspect::LabelDisplay);
    caStatus.setLabelText(TrConstants::STATUS);
    caStatus.setDefaultValue("");
    caTest.m_buttonText = TrConstants::TEST;

    caTemplateDescription.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    caTemplateDescription.setReadOnly(true);
    caTemplateDescription.setDefaultValue("");

    // quick refactor settings
    initStringAspect(qrProvider, Constants::QR_PROVIDER, TrConstants::PROVIDER, "Ollama");
    qrProvider.setReadOnly(true);
    qrSelectProvider.m_buttonText = TrConstants::SELECT;

    initStringAspect(qrModel, Constants::QR_MODEL, TrConstants::MODEL, "qwen2.5-coder:7b");
    qrModel.setHistoryCompleter(Constants::QR_MODEL_HISTORY);
    qrSelectModel.m_buttonText = TrConstants::SELECT;

    initStringAspect(qrTemplate, Constants::QR_TEMPLATE, TrConstants::TEMPLATE, "Ollama Chat");
    qrTemplate.setReadOnly(true);

    qrSelectTemplate.m_buttonText = TrConstants::SELECT;

    initStringAspect(qrUrl, Constants::QR_URL, TrConstants::URL, "http://localhost:11434");
    qrUrl.setHistoryCompleter(Constants::QR_URL_HISTORY);
    qrSetUrl.m_buttonText = TrConstants::SELECT;

    qrEndpointMode.setSettingsKey(Constants::QR_ENDPOINT_MODE);
    qrEndpointMode.setDisplayStyle(Utils::SelectionAspect::DisplayStyle::ComboBox);
    qrEndpointMode.addOption("Auto");
    qrEndpointMode.addOption("Custom");
    qrEndpointMode.addOption("FIM");
    qrEndpointMode.addOption("Chat");
    qrEndpointMode.setDefaultValue("Auto");

    initStringAspect(qrCustomEndpoint, Constants::QR_CUSTOM_ENDPOINT, TrConstants::ENDPOINT_MODE, "");
    qrCustomEndpoint.setHistoryCompleter(Constants::QR_CUSTOM_ENDPOINT_HISTORY);

    qrStatus.setDisplayStyle(Utils::StringAspect::LabelDisplay);
    qrStatus.setLabelText(TrConstants::STATUS);
    qrStatus.setDefaultValue("");
    qrTest.m_buttonText = TrConstants::TEST;

    qrTemplateDescription.setDisplayStyle(Utils::StringAspect::TextEditDisplay);
    qrTemplateDescription.setReadOnly(true);
    qrTemplateDescription.setDefaultValue("");

    ccShowTemplateInfo.m_icon = Utils::Icons::INFO.icon();
    ccShowTemplateInfo.m_tooltip = Tr::tr("Show template information");
    ccShowTemplateInfo.m_isCompact = true;

    caShowTemplateInfo.m_icon = Utils::Icons::INFO.icon();
    caShowTemplateInfo.m_tooltip = Tr::tr("Show template information");
    caShowTemplateInfo.m_isCompact = true;

    qrShowTemplateInfo.m_icon = Utils::Icons::INFO.icon();
    qrShowTemplateInfo.m_tooltip = Tr::tr("Show template information");
    qrShowTemplateInfo.m_isCompact = true;

    readSettings();

    Logger::instance().setLoggingEnabled(enableLogging());

    setupConnections();

    updatePreset1Visiblity(specifyPreset1.value());
    ccCustomEndpoint.setEnabled(ccEndpointMode.stringValue() == "Custom");
    ccPreset1CustomEndpoint.setEnabled(ccPreset1EndpointMode.stringValue() == "Custom");
    caCustomEndpoint.setEnabled(caEndpointMode.stringValue() == "Custom");
    qrCustomEndpoint.setEnabled(qrEndpointMode.stringValue() == "Custom");

    setLayouter([this]() {
        using namespace Layouting;

        auto ccGrid = Grid{};
        ccGrid.addRow({ccProvider, ccSelectProvider});
        ccGrid.addRow({ccUrl, ccSetUrl});
        ccGrid.addRow({ccCustomEndpoint, ccEndpointMode});
        ccGrid.addRow({ccModel, ccSelectModel});
        ccGrid.addRow({ccTemplate, ccSelectTemplate, ccShowTemplateInfo});

        auto ccPreset1Grid = Grid{};
        ccPreset1Grid.addRow({ccPreset1Provider, ccPreset1SelectProvider});
        ccPreset1Grid.addRow({ccPreset1Url, ccPreset1SetUrl});
        ccPreset1Grid.addRow({ccPreset1CustomEndpoint, ccPreset1EndpointMode});
        ccPreset1Grid.addRow({ccPreset1Model, ccPreset1SelectModel});
        ccPreset1Grid.addRow({ccPreset1Template, ccPreset1SelectTemplate});

        auto caGrid = Grid{};
        caGrid.addRow({caProvider, caSelectProvider});
        caGrid.addRow({caUrl, caSetUrl});
        caGrid.addRow({caCustomEndpoint, caEndpointMode});
        caGrid.addRow({caModel, caSelectModel});
        caGrid.addRow({caTemplate, caSelectTemplate, caShowTemplateInfo});

        auto qrGrid = Grid{};
        qrGrid.addRow({qrProvider, qrSelectProvider});
        qrGrid.addRow({qrUrl, qrSetUrl});
        qrGrid.addRow({qrCustomEndpoint, qrEndpointMode});
        qrGrid.addRow({qrModel, qrSelectModel});
        qrGrid.addRow({qrTemplate, qrSelectTemplate, qrShowTemplateInfo});

        auto ccGroup = Group{
            title(TrConstants::CODE_COMPLETION),
            Column{
                ccGrid,
                Row{specifyPreset1, preset1Language, Stretch{1}},
                ccPreset1Grid}};

        auto caGroup = Group{
            title(TrConstants::CHAT_ASSISTANT), Column{caGrid}};

        auto qrGroup = Group{
            title(TrConstants::QUICK_REFACTOR), Column{qrGrid}};

        auto rootLayout = Column{
            Row{enableQodeAssist, Stretch{1}, Row{checkUpdate, resetToDefaults}},
            Row{enableLogging, Stretch{1}},
            Row{enableCheckUpdate, Stretch{1}},
            Space{8},
            ccGroup,
            Space{8},
            caGroup,
            Space{8},
            qrGroup,
            Stretch{1}};

        return rootLayout;
    });
}

void GeneralSettings::showSelectionDialog(
    const QStringList &data, Utils::StringAspect &aspect, const QString &title, const QString &text)
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

void GeneralSettings::showModelsNotFoundDialog(Utils::StringAspect &aspect)
{
    SettingsDialog dialog(TrConstants::CONNECTION_ERROR);
    dialog.addLabel(TrConstants::NO_MODELS_FOUND);
    dialog.addLabel(TrConstants::CHECK_CONNECTION);
    dialog.addSpacing();

    ButtonAspect *providerButton = nullptr;
    ButtonAspect *urlButton = nullptr;

    if (&aspect == &ccModel) {
        providerButton = &ccSelectProvider;
        urlButton = &ccSetUrl;
    } else if (&aspect == &caModel) {
        providerButton = &caSelectProvider;
        urlButton = &caSetUrl;
    } else if (&aspect == &qrModel) {
        providerButton = &qrSelectProvider;
        urlButton = &qrSetUrl;
    }

    if (providerButton && urlButton) {
        auto selectProviderBtn = new QPushButton(TrConstants::SELECT_PROVIDER);
        auto selectUrlBtn = new QPushButton(TrConstants::SELECT_URL);
        auto enterManuallyBtn = new QPushButton(TrConstants::ENTER_MODEL_MANUALLY);
        auto configureApiKeyBtn = new QPushButton(TrConstants::CONFIGURE_API_KEY);

        connect(selectProviderBtn, &QPushButton::clicked, &dialog, [this, providerButton, &dialog]() {
            dialog.close();
            emit providerButton->clicked();
        });

        connect(selectUrlBtn, &QPushButton::clicked, &dialog, [this, urlButton, &dialog]() {
            dialog.close();
            emit urlButton->clicked();
        });

        connect(enterManuallyBtn, &QPushButton::clicked, &dialog, [this, &aspect, &dialog]() {
            dialog.close();
            showModelsNotSupportedDialog(aspect);
        });

        connect(configureApiKeyBtn, &QPushButton::clicked, &dialog, [&dialog]() {
            dialog.close();
            Core::ICore::showOptionsDialog(Constants::QODE_ASSIST_PROVIDER_SETTINGS_PAGE_ID);
        });

        dialog.buttonLayout()->addWidget(selectProviderBtn);
        dialog.buttonLayout()->addWidget(selectUrlBtn);
        dialog.buttonLayout()->addWidget(enterManuallyBtn);
        dialog.buttonLayout()->addWidget(configureApiKeyBtn);
    }

    auto closeBtn = new QPushButton(TrConstants::CLOSE);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::close);
    dialog.buttonLayout()->addWidget(closeBtn);

    dialog.exec();
}

void GeneralSettings::showModelsNotSupportedDialog(Utils::StringAspect &aspect)
{
    SettingsDialog dialog(TrConstants::MODEL_SELECTION);
    dialog.addLabel(TrConstants::MODEL_LISTING_NOT_SUPPORTED_INFO);
    dialog.addSpacing();

    QString key = QString("CompleterHistory/")
                      .append(
                          (&aspect == &ccModel) ? Constants::CC_MODEL_HISTORY
                          : (&aspect == &caModel) ? Constants::CA_MODEL_HISTORY
                                                  : Constants::QR_MODEL_HISTORY);
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 0)
    QStringList historyList
        = Utils::QtcSettings().value(Utils::Key(key.toLocal8Bit())).toStringList();
#else
    QStringList historyList = qtcSettings()->value(Utils::Key(key.toLocal8Bit())).toStringList();
#endif

    auto modelList = dialog.addComboBox(historyList, aspect.value());
    dialog.addSpacing();

    auto okButton = new QPushButton(TrConstants::OK);
    connect(okButton, &QPushButton::clicked, &dialog, [this, &aspect, modelList, &dialog]() {
        QString value = modelList->currentText().trimmed();
        if (!value.isEmpty()) {
            aspect.setValue(value);
            writeSettings();
            dialog.accept();
        }
    });

    auto cancelButton = new QPushButton(TrConstants::CANCEL);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    addDialogButtons(dialog.buttonLayout(), okButton, cancelButton);

    modelList->setFocus();
    dialog.exec();
}

void GeneralSettings::showUrlSelectionDialog(
    Utils::StringAspect &aspect, const QStringList &predefinedUrls)
{
    SettingsDialog dialog(TrConstants::URL_SELECTION);
    dialog.addLabel(TrConstants::URL_SELECTION_INFO);
    dialog.addSpacing();

    QStringList allUrls = predefinedUrls;
    QString key = QString("CompleterHistory/")
                      .append(
                          (&aspect == &ccUrl)          ? Constants::CC_URL_HISTORY
                          : (&aspect == &ccPreset1Url) ? Constants::CC_PRESET1_URL_HISTORY
                          : (&aspect == &caUrl)        ? Constants::CA_URL_HISTORY
                                                       : Constants::QR_URL_HISTORY);
#if QODEASSIST_QT_CREATOR_VERSION >= QT_VERSION_CHECK(18, 0, 0)
    QStringList historyList
        = Utils::QtcSettings().value(Utils::Key(key.toLocal8Bit())).toStringList();
#else
    QStringList historyList = qtcSettings()->value(Utils::Key(key.toLocal8Bit())).toStringList();
#endif
    allUrls.append(historyList);
    allUrls.removeDuplicates();

    auto urlList = dialog.addComboBox(allUrls, aspect.value());
    dialog.addSpacing();

    auto okButton = new QPushButton(TrConstants::OK);
    connect(okButton, &QPushButton::clicked, &dialog, [this, &aspect, urlList, &dialog]() {
        QString value = urlList->currentText().trimmed();
        if (!value.isEmpty()) {
            aspect.setValue(value);
            writeSettings();
            dialog.accept();
        }
    });

    auto cancelButton = new QPushButton(TrConstants::CANCEL);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);

    addDialogButtons(dialog.buttonLayout(), okButton, cancelButton);

    urlList->setFocus();
    dialog.exec();
}

void GeneralSettings::showTemplateInfoDialog(const Utils::StringAspect &descriptionAspect, const QString &templateName)
{
    SettingsDialog dialog(Tr::tr("Template Information"));
    dialog.addLabel(QString("<b>%1:</b> %2").arg(Tr::tr("Template"), templateName));
    dialog.addSpacing();

    auto *descriptionLabel = new QLabel(Tr::tr("Description:"));
    dialog.layout()->addWidget(descriptionLabel);

    auto *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setMinimumHeight(200);
    textEdit->setMinimumWidth(500);
    textEdit->setText(descriptionAspect.value());
    dialog.layout()->addWidget(textEdit);

    dialog.addSpacing();

    auto *closeButton = new QPushButton(TrConstants::CLOSE);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.buttonLayout()->addWidget(closeButton);

    dialog.exec();
}

void GeneralSettings::updatePreset1Visiblity(bool state)
{
    ccPreset1Provider.setVisible(specifyPreset1.volatileValue());
    ccPreset1SelectProvider.updateVisibility(specifyPreset1.volatileValue());
    ccPreset1Url.setVisible(specifyPreset1.volatileValue());
    ccPreset1SetUrl.updateVisibility(specifyPreset1.volatileValue());
    ccPreset1Model.setVisible(specifyPreset1.volatileValue());
    ccPreset1SelectModel.updateVisibility(specifyPreset1.volatileValue());
    ccPreset1Template.setVisible(specifyPreset1.volatileValue());
    ccPreset1SelectTemplate.updateVisibility(specifyPreset1.volatileValue());
    ccPreset1EndpointMode.setVisible(specifyPreset1.volatileValue());
    ccPreset1CustomEndpoint.setVisible(specifyPreset1.volatileValue());
}

void GeneralSettings::setupConnections()
{
    connect(&enableLogging, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        Logger::instance().setLoggingEnabled(enableLogging.volatileValue());
    });
    connect(&resetToDefaults, &ButtonAspect::clicked, this, &GeneralSettings::resetPageToDefaults);
    connect(&checkUpdate, &ButtonAspect::clicked, this, [this]() {
        QodeAssist::UpdateDialog::checkForUpdatesAndShow(Core::ICore::dialogParent());
    });

    connect(&specifyPreset1, &Utils::BoolAspect::volatileValueChanged, this, [this]() {
        updatePreset1Visiblity(specifyPreset1.volatileValue());
    });
    connect(&ccEndpointMode, &Utils::BaseAspect::volatileValueChanged, this, [this]() {
        ccCustomEndpoint.setEnabled(
            ccEndpointMode.volatileValue() == ccEndpointMode.indexForDisplay("Custom"));
    });
    connect(&ccPreset1EndpointMode, &Utils::BaseAspect::volatileValueChanged, this, [this]() {
        ccPreset1CustomEndpoint.setEnabled(
            ccPreset1EndpointMode.volatileValue()
            == ccPreset1EndpointMode.indexForDisplay("Custom"));
    });
    connect(&caEndpointMode, &Utils::BaseAspect::volatileValueChanged, this, [this]() {
        caCustomEndpoint.setEnabled(
            caEndpointMode.volatileValue() == caEndpointMode.indexForDisplay("Custom"));
    });
    connect(&qrEndpointMode, &Utils::BaseAspect::volatileValueChanged, this, [this]() {
        qrCustomEndpoint.setEnabled(
            qrEndpointMode.volatileValue() == qrEndpointMode.indexForDisplay("Custom"));
    });

    connect(&ccShowTemplateInfo, &ButtonAspect::clicked, this, [this]() {
        showTemplateInfoDialog(ccTemplateDescription, ccTemplate.value());
    });

    connect(&caShowTemplateInfo, &ButtonAspect::clicked, this, [this]() {
        showTemplateInfoDialog(caTemplateDescription, caTemplate.value());
    });

    connect(&qrShowTemplateInfo, &ButtonAspect::clicked, this, [this]() {
        showTemplateInfoDialog(qrTemplateDescription, qrTemplate.value());
    });
}

void GeneralSettings::resetPageToDefaults()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        Core::ICore::dialogParent(),
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
        resetAspect(enableCheckUpdate);
        resetAspect(specifyPreset1);
        resetAspect(preset1Language);
        resetAspect(ccPreset1Provider);
        resetAspect(ccPreset1Model);
        resetAspect(ccPreset1Template);
        resetAspect(ccPreset1Url);
        resetAspect(ccEndpointMode);
        resetAspect(ccCustomEndpoint);
        resetAspect(ccPreset1EndpointMode);
        resetAspect(ccPreset1CustomEndpoint);
        resetAspect(caEndpointMode);
        resetAspect(caCustomEndpoint);
        resetAspect(qrProvider);
        resetAspect(qrModel);
        resetAspect(qrTemplate);
        resetAspect(qrUrl);
        resetAspect(qrEndpointMode);
        resetAspect(qrCustomEndpoint);
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
#if QODEASSIST_QT_CREATOR_VERSION < QT_VERSION_CHECK(15, 0, 83)
        setDisplayCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_DISPLAY_CATEGORY);
        setCategoryIconPath(":/resources/images/qoderassist-icon.png");
#endif
        setSettingsProvider([] { return &generalSettings(); });
    }
};

const GeneralSettingsPage generalSettingsPage;

} // namespace QodeAssist::Settings
