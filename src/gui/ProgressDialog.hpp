#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QProgressBar;
class QPushButton;

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget *parent = nullptr);

    void setMessage(const QString &msg);
    void setProgress(int percent);       // 0..100; -1 for indeterminate
    void setError(const QString &err);

    void setCancellable(bool on);
    bool wasCancelled() const { return m_cancelled; }

signals:
    void cancelRequested();

private:
    QLabel       *m_label    = nullptr;
    QProgressBar *m_bar      = nullptr;
    QPushButton  *m_cancelBtn = nullptr;
    bool          m_cancelled = false;
};
