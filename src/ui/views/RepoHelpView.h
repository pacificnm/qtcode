#pragma once

#include <QWidget>

class QTextBrowser;

namespace qtcode::core {
class StatusService;
} // namespace qtcode::core

namespace qtcode::ui {

class RepoHelpView final : public QWidget
{
    Q_OBJECT

public:
    explicit RepoHelpView(
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);

    void loadHelpEntry(const QString &entryFilePath);
    [[nodiscard]] QString docRootPath() const;
    [[nodiscard]] QString currentFilePath() const;

private slots:
    void onAnchorClicked(const QUrl &url);

private:
    void configureLayout();
    void loadMarkdownFile(const QString &absolutePath);
    void showMessage(const QString &message);
    [[nodiscard]] bool isPathInsideDocRoot(const QString &absolutePath) const;
    [[nodiscard]] static QString indexPathForDocRoot(const QString &docRootPath);

    qtcode::core::StatusService *m_statusService = nullptr;
    QTextBrowser *m_browser = nullptr;
    QString m_docRootPath;
    QString m_currentFilePath;
};

} // namespace qtcode::ui
