#pragma once

#include <QAbstractListModel>

namespace qtcode::core {
class ProjectManager;
} // namespace qtcode::core

namespace qtcode::ui {

class RepositoryListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RepositoryRole {
        ProjectIdRole = Qt::UserRole + 1,
        NameRole,
        RootPathRole,
        IsActiveRole,
    };
    Q_ENUM(RepositoryRole)

    explicit RepositoryListModel(qtcode::core::ProjectManager *projectManager, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

public slots:
    void reload();

private:
    qtcode::core::ProjectManager *m_projectManager = nullptr;
};

} // namespace qtcode::ui
