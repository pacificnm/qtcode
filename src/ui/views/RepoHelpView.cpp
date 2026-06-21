#include "ui/views/RepoHelpView.h"

#include "core/StatusModels.h"
#include "core/StatusService.h"

#include <KLocalizedString>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace qtcode::ui {

RepoHelpView::RepoHelpView(qtcode::core::StatusService *statusService, QWidget *parent)
    : QWidget(parent)
    , m_statusService(statusService)
{
    configureLayout();
    showMessage(i18n("Select Help > Repo Help to load project documentation."));
}

void RepoHelpView::configureLayout()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &RepoHelpView::onAnchorClicked);

    layout->addWidget(m_browser, 1);
}

void RepoHelpView::loadHelpEntry(const QString &entryFilePath)
{
    const QString normalizedEntry = QFileInfo(entryFilePath).absoluteFilePath();
    if (normalizedEntry.isEmpty()) {
        showMessage(i18n("The active project does not define a repository help entry file."));
        return;
    }

    m_docRootPath = QFileInfo(normalizedEntry).absolutePath();
    m_entryFilePath = normalizedEntry;
    if (!QFileInfo(normalizedEntry).isFile()) {
        showMessage(i18n("No repository help found at %1.", normalizedEntry));
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Repository help entry file is missing."),
                qtcode::core::StatusSeverity::Warning);
        }
        return;
    }

    loadMarkdownFile(normalizedEntry);
}

QString RepoHelpView::docRootPath() const
{
    return m_docRootPath;
}

QString RepoHelpView::currentFilePath() const
{
    return m_currentFilePath;
}

void RepoHelpView::loadMarkdownFile(const QString &absolutePath)
{
    const QString normalizedPath = QFileInfo(absolutePath).absoluteFilePath();
    if (normalizedPath.isEmpty() || !isPathInsideDocRoot(normalizedPath)) {
        showMessage(i18n("That documentation page is outside the repository doc folder."));
        return;
    }

    QFile file(normalizedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showMessage(i18n("Could not read %1.", QFileInfo(normalizedPath).fileName()));
        if (m_statusService != nullptr) {
            m_statusService->showMessage(
                i18n("Could not read repository help page."),
                qtcode::core::StatusSeverity::Error);
        }
        return;
    }

    QString markdown = QString::fromUtf8(file.readAll());
    m_currentFilePath = normalizedPath;

    const QUrl baseUrl = QUrl::fromLocalFile(QFileInfo(normalizedPath).absolutePath() + QDir::separator());
    m_browser->document()->setBaseUrl(baseUrl);

    const QString mainReadmeLink = mainReadmeLinkMarkdown(normalizedPath);
    if (!mainReadmeLink.isEmpty()) {
        markdown = mainReadmeLink + QStringLiteral("\n\n---\n\n") + markdown;
    }

    m_browser->setMarkdown(markdown);
}

void RepoHelpView::showMessage(const QString &message)
{
    m_currentFilePath.clear();
    m_entryFilePath.clear();
    m_browser->clear();
    m_browser->setPlainText(message);
}

QString RepoHelpView::mainReadmeLinkMarkdown(const QString &absolutePath) const
{
    if (m_entryFilePath.isEmpty()) {
        return {};
    }

    const QString normalizedPath = QFileInfo(absolutePath).absoluteFilePath();
    const QString normalizedEntry = QFileInfo(m_entryFilePath).absoluteFilePath();
    if (normalizedPath.isEmpty() || normalizedPath == normalizedEntry) {
        return {};
    }

    const QString relativePath =
        QDir(QFileInfo(normalizedPath).absolutePath()).relativeFilePath(normalizedEntry);
    if (relativePath.isEmpty() || relativePath == QLatin1String(".")) {
        return {};
    }

    return QStringLiteral("[") + i18n("Main Readme") + QStringLiteral("](") + relativePath
        + QStringLiteral(")");
}

void RepoHelpView::onAnchorClicked(const QUrl &url)
{
    if (url.isEmpty()) {
        return;
    }

    if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")
        || url.scheme() == QLatin1String("mailto")) {
        QDesktopServices::openUrl(url);
        return;
    }

    const QUrl baseUrl = QUrl::fromLocalFile(m_currentFilePath);
    const QUrl resolved = url.isRelative() ? baseUrl.resolved(url) : url;
    const QString localPath = QFileInfo(resolved.toLocalFile()).absoluteFilePath();

    if (!localPath.isEmpty()) {
        if (localPath == QFileInfo(m_currentFilePath).absoluteFilePath() && !url.fragment().isEmpty()) {
            m_browser->scrollToAnchor(url.fragment());
            return;
        }

        if (!isPathInsideDocRoot(localPath)) {
            if (m_statusService != nullptr) {
                m_statusService->showMessage(
                    i18n("That link points outside the repository doc folder."),
                    qtcode::core::StatusSeverity::Warning);
            }
            return;
        }

        const QFileInfo fileInfo(localPath);
        if (fileInfo.isDir()) {
            const QString indexPath = indexPathForDocRoot(localPath);
            if (QFileInfo(indexPath).isFile()) {
                loadMarkdownFile(indexPath);
            }
            return;
        }

        if (fileInfo.suffix().compare(QLatin1String("md"), Qt::CaseInsensitive) == 0) {
            loadMarkdownFile(localPath);
            return;
        }

        QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
        return;
    }

    if (!url.fragment().isEmpty()) {
        m_browser->scrollToAnchor(url.fragment());
        return;
    }

    QDesktopServices::openUrl(resolved);
}

bool RepoHelpView::isPathInsideDocRoot(const QString &absolutePath) const
{
    if (m_docRootPath.isEmpty()) {
        return false;
    }

    const QString normalizedPath = QDir::cleanPath(absolutePath);
    const QString normalizedRoot = QDir::cleanPath(m_docRootPath);
    if (normalizedPath == normalizedRoot) {
        return true;
    }

    return normalizedPath.startsWith(normalizedRoot + QDir::separator());
}

QString RepoHelpView::indexPathForDocRoot(const QString &docRootPath)
{
    return QDir(docRootPath).absoluteFilePath(QStringLiteral("index.md"));
}

} // namespace qtcode::ui
