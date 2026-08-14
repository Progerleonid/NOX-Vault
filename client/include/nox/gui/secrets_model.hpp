#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>

namespace nox::gui {
class SecretsModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

  public:
    enum Roles { NameRole = Qt::UserRole + 1, UpdatedAtRole };
    explicit SecretsModel(QObject *parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QString filter() const;
    void setFilter(const QString &value);
    void setNames(QStringList names);
    void setRecords(const QVariantList &records);
    void clear();

  signals:
    void filterChanged();

  private:
    struct Item { QString name; QString updatedAt; };
    QList<Item> items_;
    QList<Item> visible_;
    QString filter_;
    void rebuild();
};
} // namespace nox::gui
