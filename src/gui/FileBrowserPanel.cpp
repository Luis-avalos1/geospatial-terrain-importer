#include "FileBrowserPanel.hpp"

#include <QVBoxLayout>
#include <QTreeView>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QDir>

FileBrowserPanel::FileBrowserPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_model = new QFileSystemModel(this);
    m_model->setNameFilters({"*.tif", "*.tiff", "*.img", "*.png", "*.jpg", "*.jpeg"});
    m_model->setNameFilterDisables(false);  // hide non-matching entries
    m_model->setRootPath(QDir::homePath());

    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setRootIndex(m_model->index(QDir::homePath()));
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::AscendingOrder);

    // Hide size/type/date columns to save space
    m_tree->header()->hideSection(1);
    m_tree->header()->hideSection(2);
    m_tree->header()->hideSection(3);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    layout->addWidget(m_tree);

    connect(m_tree, &QTreeView::activated,
            this,   &FileBrowserPanel::onActivated);
}

void FileBrowserPanel::setRootPath(const QString &path)
{
    m_model->setRootPath(path);
    m_tree->setRootIndex(m_model->index(path));
}

QString FileBrowserPanel::rootPath() const
{
    return m_model->rootPath();
}

void FileBrowserPanel::onActivated(const QModelIndex &index)
{
    if (m_model->isDir(index)) return;
    emit fileActivated(m_model->filePath(index));
}
