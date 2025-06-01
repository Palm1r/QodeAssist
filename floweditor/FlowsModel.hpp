#pragma once

#include <QAbstractListModel>
#include <QObject>

// #include "tasks/Flow.hpp"
#include "tasks/FlowManager.hpp"

namespace QodeAssist::FlowEditor {

class FlowsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum FlowRoles { FlowIdRole = Qt::UserRole, FlowDataRole };

    FlowsModel(FlowManager *flowManager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void onFlowAdded(const QString &flowId);
    void onFlowRemoved(const QString &flowId);

private:
    FlowManager *m_flowManager;
};

} // namespace QodeAssist::FlowEditor
