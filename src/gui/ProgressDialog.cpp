#include "ProgressDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setModal(true);
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    m_label = new QLabel("Loading…", this);
    m_label->setWordWrap(true);
    layout->addWidget(m_label);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    layout->addWidget(m_bar);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setVisible(false);
    btnRow->addWidget(m_cancelBtn);
    layout->addLayout(btnRow);

    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_cancelled = true;
        emit cancelRequested();
        m_cancelBtn->setEnabled(false);
        m_cancelBtn->setText("Cancelling…");
    });
}

void ProgressDialog::setMessage(const QString &msg)
{
    m_label->setText(msg);
}

void ProgressDialog::setProgress(int percent)
{
    if (percent < 0) {
        m_bar->setRange(0, 0);  // indeterminate (spinning bar)
    } else {
        m_bar->setRange(0, 100);
        m_bar->setValue(percent);
    }
}

void ProgressDialog::setError(const QString &err)
{
    m_label->setText("<span style='color:#cc4444;'>Error: " + err.toHtmlEscaped() + "</span>");
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
}

void ProgressDialog::setCancellable(bool on)
{
    m_cancelBtn->setVisible(on);
}
