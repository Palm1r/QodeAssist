// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProvidersSettingsPage.hpp"

#include <algorithm>
#include <vector>

#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>

#include <utils/filepath.h>

#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include "NewProviderDialog.hpp"
#include "ProviderDetailPane.hpp"
#include "ProviderInstance.hpp"
#include "ProviderInstanceFactory.hpp"
#include "ProviderInstanceWriter.hpp"
#include "ProviderLauncher.hpp"
#include "ProviderListItem.hpp"
#include "ProviderSecretsStore.hpp"
#include "ProvidersSettingsHelpers.hpp"
#include "SettingsConstants.hpp"

namespace QodeAssist::Settings {

ProvidersPageNavigator::ProvidersPageNavigator(QObject *parent)
    : QObject(parent)
{}

void ProvidersPageNavigator::requestSelectInstance(const QString &name)
{
    m_pending = name;
    emit selectInstanceRequested(name);
}

QString ProvidersPageNavigator::takePendingSelection()
{
    QString p = m_pending;
    m_pending.clear();
    return p;
}

namespace {

class ProvidersPageWidget : public Core::IOptionsPageWidget
{
    Q_OBJECT
public:
    ProvidersPageWidget(
        Providers::ProviderInstanceFactory *factory,
        Providers::ProviderSecretsStore *secrets,
        Providers::ProviderLauncher *launcher,
        ProvidersPageNavigator *navigator)
        : m_factory(factory)
        , m_secrets(secrets)
        , m_launcher(launcher)
        , m_navigator(navigator)
    {
        m_titleLabel = new QLabel(tr("Providers"), this);
        QFont tf = m_titleLabel->font();
        tf.setBold(true);
        tf.setPixelSize(13);
        m_titleLabel->setFont(tf);

        m_newBtn = new QPushButton(tr("+ New provider…"), this);

        auto *headerRow = new QHBoxLayout;
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(8);
        headerRow->addWidget(m_titleLabel, 1);
        headerRow->addWidget(m_newBtn);

        auto *headerSep = new QFrame(this);
        headerSep->setFrameShape(QFrame::HLine);
        headerSep->setFrameShadow(QFrame::Sunken);

        m_filterEdit = new QLineEdit(this);
        m_filterEdit->setPlaceholderText(tr("Filter providers…"));

        m_listScroll = new QScrollArea(this);
        m_listScroll->setWidgetResizable(true);
        m_listScroll->setFrameShape(QFrame::NoFrame);
        m_listContent = new QWidget(this);
        m_listLayout = new QVBoxLayout(m_listContent);
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        m_listLayout->setSpacing(0);
        m_listLayout->addStretch(1);
        m_listScroll->setWidget(m_listContent);

        auto *leftBox = new QFrame(this);
        leftBox->setFrameShape(QFrame::StyledPanel);
        auto *leftLay = new QVBoxLayout(leftBox);
        leftLay->setContentsMargins(0, 0, 0, 0);
        leftLay->setSpacing(0);
        auto *filterRow = new QHBoxLayout;
        filterRow->setContentsMargins(6, 6, 6, 6);
        filterRow->addWidget(m_filterEdit, 1);
        leftLay->addLayout(filterRow);
        leftLay->addWidget(m_listScroll, 1);

        m_detailPane = new ProviderDetailPane(this);
        connect(m_detailPane, &ProviderDetailPane::saveRequested,
                this, &ProvidersPageWidget::onSaveEdited);
        connect(m_detailPane, &ProviderDetailPane::duplicateRequested,
                this, &ProvidersPageWidget::onDuplicateClicked);
        connect(m_detailPane, &ProviderDetailPane::deleteRequested,
                this, &ProvidersPageWidget::onRemoveClicked);
        connect(m_detailPane, &ProviderDetailPane::apiKeySaveRequested,
                this, &ProvidersPageWidget::onApiKeySave);
        connect(m_detailPane, &ProviderDetailPane::apiKeyClearRequested,
                this, &ProvidersPageWidget::onApiKeyClear);
        connect(m_detailPane, &ProviderDetailPane::launchStartRequested,
                this, &ProvidersPageWidget::onLaunchStart);
        connect(m_detailPane, &ProviderDetailPane::launchStopRequested,
                this, &ProvidersPageWidget::onLaunchStop);
        connect(m_detailPane, &ProviderDetailPane::launchRestartRequested,
                this, &ProvidersPageWidget::onLaunchRestart);
        connect(m_detailPane, &ProviderDetailPane::openInEditorRequested,
                this, [this](const QString &path) {
                    if (path.isEmpty() || path.startsWith(QLatin1String(":/"))) {
                        QMessageBox::information(
                            this, tr("Open in editor"),
                            tr("Bundled providers are read-only. "
                               "Use Duplicate to create an editable user copy first."));
                        return;
                    }
                    Core::EditorManager::openEditor(Utils::FilePath::fromString(path));
                });
        if (m_launcher) {
            connect(m_launcher.data(), &Providers::ProviderLauncher::stateChanged,
                    this, [this](const QString &name,
                                 Providers::ProviderLauncher::State newState) {
                        if (name == m_currentName)
                            refreshDetailLaunch();
                        const ProviderListItem::Status status = rowStatusFromState(newState);
                        for (auto *row : m_rows) {
                            if (row->providerName() == name)
                                row->setStatus(status);
                        }
                    });
            connect(m_launcher.data(), &Providers::ProviderLauncher::bytesReceived,
                    this, [this](const QString &name, const QByteArray &chunk) {
                        if (name == m_currentName)
                            m_detailPane->appendLaunchBytes(chunk);
                    });
        }
        m_detailScroll = new QScrollArea(this);
        m_detailScroll->setWidgetResizable(true);
        m_detailScroll->setFrameShape(QFrame::StyledPanel);
        m_detailScroll->setWidget(m_detailPane);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(leftBox);
        splitter->addWidget(m_detailScroll);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({320, 700});

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);
        root->addLayout(headerRow);
        root->addWidget(headerSep);
        root->addWidget(splitter, 1);

        connect(m_newBtn, &QPushButton::clicked, this, &ProvidersPageWidget::onNewClicked);
        m_filterDebounce = new QTimer(this);
        m_filterDebounce->setSingleShot(true);
        m_filterDebounce->setInterval(100);
        connect(m_filterDebounce, &QTimer::timeout, this,
                &ProvidersPageWidget::rebuildList);
        connect(m_filterEdit, &QLineEdit::textChanged, this,
                [this](const QString &) { m_filterDebounce->start(); });

        if (m_factory) {
            connect(m_factory.data(),
                    &Providers::ProviderInstanceFactory::instancesReloaded,
                    this, &ProvidersPageWidget::rebuildList);
        }
        if (m_navigator) {
            connect(m_navigator.data(),
                    &ProvidersPageNavigator::selectInstanceRequested,
                    this, &ProvidersPageWidget::selectInstance);
        }

        rebuildList();

        const QString pending
            = m_navigator ? m_navigator->takePendingSelection() : QString{};
        if (!pending.isEmpty())
            selectInstance(pending);
        else if (m_factory && !m_factory->instances().empty())
            selectInstance(m_factory->instances().front().name);
    }

    void apply() final {}

private slots:
    void rebuildList()
    {
        if (!m_factory)
            return;
        while (m_listLayout->count() > 0) {
            QLayoutItem *item = m_listLayout->takeAt(0);
            if (auto *w = item->widget())
                w->deleteLater();
            delete item;
        }
        m_rows.clear();
        m_listLayout->addStretch(1); // re-add trailing stretch

        const QString filter = m_filterEdit->text().trimmed().toLower();
        auto matches = [&](const Providers::ProviderInstance &inst) {
            if (filter.isEmpty())
                return true;
            return inst.name.toLower().contains(filter)
                   || inst.clientApi.toLower().contains(filter)
                   || inst.url.toLower().contains(filter);
        };


        auto addSection = [&](const QString &title, bool userSection) {
            auto *header = new QLabel(title.toUpper(), m_listContent);
            QFont hf = header->font();
            hf.setPixelSize(10);
            hf.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
            header->setFont(hf);
            QPalette hp = header->palette();
            hp.setColor(QPalette::WindowText, hp.color(QPalette::Mid));
            header->setPalette(hp);
            header->setContentsMargins(8, 4, 8, 4);
            header->setAutoFillBackground(true);
            const bool dark = isDarkPalette(palette());
            const QString bg = dark ? QStringLiteral("#262626") : QStringLiteral("#f0f0f0");
            header->setStyleSheet(QStringLiteral("QLabel { background:%1; }").arg(bg));
            m_listLayout->insertWidget(m_listLayout->count() - 1, header);

            std::vector<const Providers::ProviderInstance *> sorted;
            for (const auto &inst : m_factory->instances()) {
                if (inst.isUserSource() != userSection)
                    continue;
                if (!matches(inst))
                    continue;
                sorted.push_back(&inst);
            }
            std::sort(sorted.begin(), sorted.end(),
                      [](const Providers::ProviderInstance *a,
                         const Providers::ProviderInstance *b) {
                          return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
                      });

            int shown = 0;
            for (const auto *inst : sorted) {
                auto *row = new ProviderListItem(*inst, m_listContent);
                connect(row, &ProviderListItem::clicked,
                        this, &ProvidersPageWidget::selectInstance);
                if (m_launcher)
                    row->setStatus(rowStatusFromLauncher(inst->name));
                m_rows.append(row);
                m_listLayout->insertWidget(m_listLayout->count() - 1, row);
                ++shown;
            }
            if (shown == 0) {
                auto *empty = new QLabel(
                    userSection ? tr("No user instances yet.")
                                : tr("No bundled instances loaded."),
                    m_listContent);
                empty->setContentsMargins(10, 6, 10, 6);
                QPalette ep = empty->palette();
                ep.setColor(QPalette::WindowText, ep.color(QPalette::Mid));
                empty->setPalette(ep);
                m_listLayout->insertWidget(m_listLayout->count() - 1, empty);
            }
        };

        addSection(tr("User"), true);
        addSection(tr("Bundled"), false);

        for (auto *row : m_rows)
            row->setSelected(row->providerName() == m_currentName);

        if (!m_currentName.isEmpty())
            populateDetail(m_currentName);
        else
            m_detailPane->clear();
    }

    void selectInstance(const QString &name)
    {
        if (name.isEmpty())
            return;
        const auto *inst = m_factory ? m_factory->instanceByName(name) : nullptr;
        if (!inst)
            return;
        m_currentName = inst->name;
        for (auto *row : m_rows)
            row->setSelected(row->providerName() == inst->name);
        populateDetail(inst->name);
    }

    void onNewClicked()
    {
        if (!m_factory)
            return;
        NewProviderDialog dlg(m_factory->knownClientApis(), this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        Providers::ProviderInstance inst;
        inst.name = dlg.providerName();
        inst.clientApi = dlg.providerType();
        inst.description = dlg.description();
        inst.url = dlg.url();
        inst.apiKeyRef = QStringLiteral("qodeassist/providers/%1").arg(inst.name);

        if (inst.name.isEmpty()) {
            QMessageBox::warning(this, tr("New provider"), tr("Name cannot be empty."));
            return;
        }
        if (m_factory->instanceByName(inst.name)) {
            QMessageBox::warning(this, tr("New provider"),
                                 tr("An instance named '%1' already exists.").arg(inst.name));
            return;
        }
        const QString validation = Providers::ProviderInstance::validate(
            inst, m_factory->knownClientApis());
        if (!validation.isEmpty()) {
            QMessageBox::warning(this, tr("New provider"), validation);
            return;
        }
        const QString softWarning = Providers::ProviderInstance::warnings(inst);
        if (!softWarning.isEmpty()) {
            if (QMessageBox::warning(this, tr("New provider"),
                                     softWarning + QStringLiteral("\n\n")
                                         + tr("Save anyway?"),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No)
                != QMessageBox::Yes)
                return;
        }
        QString writeErr;
        if (Providers::ProviderInstanceWriter::writeToUserDir(
                inst, /*previousPath=*/QString{}, &writeErr).isEmpty()) {
            QMessageBox::warning(this, tr("New provider"), writeErr);
            return;
        }
        if (m_secrets && !dlg.apiKey().isEmpty())
            m_secrets->writeKey(inst.apiKeyRef, dlg.apiKey());
        m_factory->reload();
        selectInstance(inst.name);
    }

    void onDuplicateClicked()
    {
        if (!m_factory || m_currentName.isEmpty())
            return;
        const Providers::ProviderInstance *srcPtr
            = m_factory->instanceByName(m_currentName);
        if (!srcPtr)
            return;
        const Providers::ProviderInstance srcCopy = *srcPtr;
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("Duplicate provider"),
            tr("Name for the new provider:"), QLineEdit::Normal,
            QStringLiteral("%1 (copy)").arg(srcCopy.name), &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        if (m_factory->instanceByName(name.trimmed())) {
            QMessageBox::warning(this, tr("Duplicate provider"),
                                 tr("An instance named '%1' already exists.").arg(name.trimmed()));
            return;
        }
        Providers::ProviderInstance copy = srcCopy;
        copy.name = name.trimmed();
        copy.apiKeyRef = QStringLiteral("qodeassist/providers/%1").arg(copy.name);
        copy.sourcePath.clear();
        copy.overridesBundled = false;
        QString writeErr;
        if (Providers::ProviderInstanceWriter::writeToUserDir(
                copy, /*previousPath=*/QString{}, &writeErr).isEmpty()) {
            QMessageBox::warning(this, tr("Duplicate provider"), writeErr);
            return;
        }
        m_factory->reload();
        selectInstance(copy.name);
    }

    void onRemoveClicked()
    {
        if (!m_factory || m_currentName.isEmpty())
            return;
        const Providers::ProviderInstance *instPtr
            = m_factory->instanceByName(m_currentName);
        if (!instPtr || !instPtr->isUserSource())
            return;

        const QString instName = instPtr->name;
        const QString sourcePath = instPtr->sourcePath;
        if (QMessageBox::question(
                this, tr("Delete provider"),
                tr("Delete user provider '%1'?\n\nFile: %2").arg(instName, sourcePath))
            != QMessageBox::Yes)
            return;
        if (!QFile::remove(sourcePath)) {
            QMessageBox::warning(this, tr("Delete provider"),
                                 tr("Failed to delete file:\n%1").arg(sourcePath));
            return;
        }
        m_currentName.clear();
        m_factory->reload();
        m_detailPane->clear();
    }

    void onSaveEdited(const Providers::ProviderInstance &edited)
    {
        if (!m_factory)
            return;
        Providers::ProviderInstance e = edited;
        if (e.name.isEmpty()) {
            QMessageBox::warning(this, tr("Save"), tr("Name cannot be empty."));
            return;
        }
        const auto *prior = m_factory->instanceByName(m_currentName);
        const QString priorRef = prior ? prior->apiKeyRef : QString{};
        const QString priorName = prior ? prior->name : QString{};
        const bool nameChanged = !priorName.isEmpty() && priorName != e.name;
        if (e.apiKeyRef.isEmpty() || (nameChanged && e.apiKeyRef == priorRef))
            e.apiKeyRef = QStringLiteral("qodeassist/providers/%1").arg(e.name);

        const QString validation = Providers::ProviderInstance::validate(
            e, m_factory->knownClientApis());
        if (!validation.isEmpty()) {
            QMessageBox::warning(this, tr("Save"), validation);
            return;
        }
        if (nameChanged) {
            const auto *clash = m_factory->instanceByName(e.name);
            if (clash) {
                QMessageBox::warning(this, tr("Save"),
                                     tr("An instance named '%1' already exists.").arg(e.name));
                return;
            }
        }
        const QString softWarning = Providers::ProviderInstance::warnings(e);
        if (!softWarning.isEmpty()) {
            if (QMessageBox::warning(this, tr("Save"),
                                     softWarning + QStringLiteral("\n\n")
                                         + tr("Save anyway?"),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No)
                != QMessageBox::Yes)
                return;
        }

        const QString previousPath
            = (prior && prior->isUserSource()) ? prior->sourcePath : QString{};
        QString writeErr;
        const QString writtenPath = Providers::ProviderInstanceWriter::writeToUserDir(
            e, previousPath, &writeErr);
        if (writtenPath.isEmpty()) {
            QMessageBox::warning(this, tr("Save"), writeErr);
            return;
        }
        if (!previousPath.isEmpty()
            && QFileInfo(writtenPath).absoluteFilePath()
                   != QFileInfo(previousPath).absoluteFilePath()) {
            if (!QFile::remove(previousPath)) {
                QMessageBox::warning(
                    this, tr("Save"),
                    tr("Saved to:\n%1\n\nbut could not remove the old file:\n%2\n\n"
                       "Two provider files now describe this instance — delete the "
                       "old file manually to avoid a duplicate-name error.")
                        .arg(writtenPath, previousPath));
            }
        }

        if (m_secrets && !priorRef.isEmpty() && priorRef != e.apiKeyRef) {
            const QString carried = m_secrets->readKeySync(priorRef);
            if (!carried.isEmpty())
                m_secrets->writeKey(e.apiKeyRef, carried);
            m_secrets->eraseKey(priorRef);
        }
        m_factory->reload();
        selectInstance(e.name);
    }

    void onApiKeySave(const QString &newKey)
    {
        if (!m_factory || !m_secrets || m_currentName.isEmpty() || newKey.isEmpty())
            return;
        const auto *inst = m_factory->instanceByName(m_currentName);
        if (!inst || inst->apiKeyRef.isEmpty())
            return;
        m_secrets->writeKey(inst->apiKeyRef, newKey);
        m_detailPane->refreshKeyStatus(true);
    }

    void onApiKeyClear()
    {
        if (!m_factory || !m_secrets || m_currentName.isEmpty())
            return;
        const Providers::ProviderInstance *instPtr
            = m_factory->instanceByName(m_currentName);
        if (!instPtr || instPtr->apiKeyRef.isEmpty())
            return;
        const QString instName = instPtr->name;
        const QString apiKeyRef = instPtr->apiKeyRef;
        if (QMessageBox::question(
                this, tr("Clear API key"),
                tr("Erase the stored API key for '%1'?").arg(instName))
            != QMessageBox::Yes)
            return;
        m_secrets->eraseKey(apiKeyRef);
        m_detailPane->refreshKeyStatus(false);
    }

    void onLaunchStart(const QString &name)
    {
        if (!m_factory || !m_launcher)
            return;
        const auto *inst = m_factory->instanceByName(name);
        if (!inst || inst->launch.isEmpty())
            return;
        m_launcher->start(name, inst->launch);
    }

    void onLaunchStop(const QString &name)
    {
        if (!m_launcher)
            return;
        m_launcher->stop(name);
    }

    void onLaunchRestart(const QString &name)
    {
        if (!m_factory || !m_launcher)
            return;
        const auto *inst = m_factory->instanceByName(name);
        if (!inst || inst->launch.isEmpty())
            return;
        m_launcher->restart(name, inst->launch);
    }

private:
    void populateDetail(const QString &name)
    {
        if (!m_factory)
            return;
        const auto *inst = m_factory->instanceByName(name);
        if (!inst) {
            m_detailPane->clear();
            return;
        }
        const bool hasStoredKey
            = m_secrets && !inst->apiKeyRef.isEmpty() && m_secrets->hasKey(inst->apiKeyRef);
        m_detailPane->populate(*inst, hasStoredKey);

        if (m_launcher) {
            m_detailPane->setLaunchState(
                m_launcher->state(inst->name),
                m_launcher->lastError(inst->name));
            m_detailPane->resetLaunchTerminal(m_launcher->scrollback(inst->name));
        } else {
            m_detailPane->setLaunchState(Providers::ProviderLauncher::Idle, {});
            m_detailPane->resetLaunchTerminal({});
        }
    }

    QPointer<Providers::ProviderInstanceFactory> m_factory;
    QPointer<Providers::ProviderSecretsStore> m_secrets;
    QPointer<ProvidersPageNavigator> m_navigator;

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_newBtn = nullptr;
    QLineEdit *m_filterEdit = nullptr;

    QScrollArea *m_listScroll = nullptr;
    QWidget *m_listContent = nullptr;
    QVBoxLayout *m_listLayout = nullptr;
    QList<ProviderListItem *> m_rows;

    QScrollArea *m_detailScroll = nullptr;
    ProviderDetailPane *m_detailPane = nullptr;

    QString m_currentName;

    QPointer<Providers::ProviderLauncher> m_launcher;
    QTimer *m_filterDebounce = nullptr;

    void refreshDetailLaunch()
    {
        if (!m_launcher || m_currentName.isEmpty())
            return;
        m_detailPane->setLaunchState(
            m_launcher->state(m_currentName),
            m_launcher->lastError(m_currentName));
    }

    static ProviderListItem::Status rowStatusFromState(
        Providers::ProviderLauncher::State state)
    {
        switch (state) {
        case Providers::ProviderLauncher::Ready:
            return ProviderListItem::Status::Ok;
        case Providers::ProviderLauncher::Failed:
            return ProviderListItem::Status::Fail;
        case Providers::ProviderLauncher::Idle:
        case Providers::ProviderLauncher::Starting:
        case Providers::ProviderLauncher::Probing:
        case Providers::ProviderLauncher::Stopping:
            return ProviderListItem::Status::Unknown;
        }
        return ProviderListItem::Status::Unknown;
    }

    ProviderListItem::Status rowStatusFromLauncher(const QString &name) const
    {
        if (!m_launcher)
            return ProviderListItem::Status::Unknown;
        return rowStatusFromState(m_launcher->state(name));
    }
};

class ProvidersOptionsPage : public Core::IOptionsPage
{
public:
    ProvidersOptionsPage(
        Providers::ProviderInstanceFactory *factory,
        Providers::ProviderSecretsStore *secrets,
        Providers::ProviderLauncher *launcher,
        ProvidersPageNavigator *navigator)
    {
        setId(Constants::QODE_ASSIST_PROVIDER_SETTINGS_PAGE_ID);
        setDisplayName(QObject::tr("Providers"));
        setCategory(Constants::QODE_ASSIST_GENERAL_OPTIONS_CATEGORY);
        setWidgetCreator([factory, secrets, launcher, navigator] {
            return new ProvidersPageWidget(factory, secrets, launcher, navigator);
        });
    }
};

} // namespace

std::unique_ptr<Core::IOptionsPage> createProvidersSettingsPage(
    Providers::ProviderInstanceFactory *instanceFactory,
    Providers::ProviderSecretsStore *secrets,
    Providers::ProviderLauncher *launcher,
    ProvidersPageNavigator *navigator)
{
    return std::make_unique<ProvidersOptionsPage>(
        instanceFactory, secrets, launcher, navigator);
}

} // namespace QodeAssist::Settings

#include "ProvidersSettingsPage.moc"
