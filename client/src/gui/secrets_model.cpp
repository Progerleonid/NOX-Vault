#include "nox/gui/secrets_model.hpp"
#include <algorithm>

namespace nox::gui {
SecretsModel::SecretsModel(QObject *parent) : QAbstractListModel(parent) {
}
int SecretsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : visible_.size();
}
QVariant SecretsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= visible_.size())
        return {};
    const auto &item = visible_.at(index.row());
    if (role == NameRole)
        return item.name;
    if (role == UpdatedAtRole)
        return item.updatedAt;
    return {};
}
QHash<int, QByteArray> SecretsModel::roleNames() const {
    return {{NameRole, "name"}, {UpdatedAtRole, "updatedAt"}};
}
QString SecretsModel::filter() const {
    return filter_;
}
void SecretsModel::setFilter(const QString &value) {
    if (filter_ == value)
        return;
    filter_ = value;
    rebuild();
    emit filterChanged();
}
void SecretsModel::setNames(QStringList names) {
    names.sort(Qt::CaseInsensitive);
    items_.clear();
    for (auto &name : names)
        items_.push_back({std::move(name), {}});
    rebuild();
}
void SecretsModel::setRecords(const QVariantList &records) {
    items_.clear();
    for (const auto &record : records) {
        const auto map = record.toMap();
        items_.push_back({map.value("name").toString(), map.value("updatedAt").toString()});
    }
    std::sort(items_.begin(), items_.end(), [](const Item &left, const Item &right) {
        return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
    });
    rebuild();
}
void SecretsModel::clear() {
    items_.clear();
    rebuild();
}
void SecretsModel::rebuild() {
    beginResetModel();
    visible_.clear();
    for (const auto &item : items_) {
        if (filter_.isEmpty() || item.name.contains(filter_, Qt::CaseInsensitive))
            visible_.push_back(item);
    }
    endResetModel();
}
} // namespace nox::gui
