#pragma once

#include <QWidget>
#include <QString>

class QTreeView;
class QFileSystemModel;
class QLineEdit;

class FileBrowserPanel : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowserPanel(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const;

    // Visibly select a file in the tree and emit fileActivated (demo-capture mode).
    void demoSelect(const QString &path);

signals:
    void fileActivated(const QString &path);

private slots:
    void onActivated(const QModelIndex &index);

private:
    QFileSystemModel *m_model = nullptr;
    QTreeView        *m_tree  = nullptr;
};
