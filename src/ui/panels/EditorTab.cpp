#include "ui/panels/EditorTab.h"

#include "core/StatusModels.h"
#include "core/StatusService.h"
#include "shared/Logging.h"

#include <KLocalizedString>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

namespace qtcode::ui {

EditorTab::EditorTab(
    const QString &absolutePath,
    qtcode::core::StatusService *statusService,
    QWidget *parent)
    : QWidget(parent)
    , m_statusService(statusService)
    , m_filePath(QFileInfo(absolutePath).absoluteFilePath())
{
    configureEditor();
}

void EditorTab::configureEditor()
{
    KTextEditor::Editor *editor = KTextEditor::Editor::instance();
    if (editor == nullptr) {
        m_loadErrorMessage = i18n("KTextEditor is unavailable.");
        qCWarning(qtcodeUi) << m_loadErrorMessage;
        return;
    }

    m_document = editor->createDocument(this);
    if (m_document == nullptr) {
        m_loadErrorMessage = i18n("Could not create an editor document.");
        qCWarning(qtcodeUi) << m_loadErrorMessage;
        return;
    }

    m_view = m_document->createView(this);
    if (m_view == nullptr) {
        m_loadErrorMessage = i18n("Could not create an editor view.");
        qCWarning(qtcodeUi) << m_loadErrorMessage;
        return;
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view);

    const QUrl fileUrl = QUrl::fromLocalFile(m_filePath);
    if (!m_document->openUrl(fileUrl)) {
        m_loadErrorMessage = i18n("Could not load %1.", QFileInfo(m_filePath).fileName());
        qCWarning(qtcodeUi) << "Failed to open editor document for" << m_filePath;
        return;
    }

    m_loadSucceeded = true;
    connect(
        m_document,
        &KTextEditor::Document::modifiedChanged,
        this,
        [this](KTextEditor::Document *) {
            emit modificationChanged(isModified());
        });
}

QString EditorTab::filePath() const
{
    return m_filePath;
}

QString EditorTab::displayName() const
{
    return QFileInfo(m_filePath).fileName();
}

bool EditorTab::isLoadSuccessful() const
{
    return m_loadSucceeded;
}

QString EditorTab::loadErrorMessage() const
{
    return m_loadErrorMessage;
}

bool EditorTab::isModified() const
{
    return m_document != nullptr && m_document->isModified();
}

QString EditorTab::documentText() const
{
    if (m_document == nullptr) {
        return {};
    }

    return m_document->text();
}

bool EditorTab::save(QString *errorMessage)
{
    if (m_document == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = i18n("No editor document is available to save.");
        }
        return false;
    }

    if (!m_document->save()) {
        const QString message = i18n("Could not save %1.", displayName());
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        if (m_statusService != nullptr) {
            m_statusService->showMessage(message, qtcode::core::StatusSeverity::Error);
        }
        qCWarning(qtcodeUi) << message << m_filePath;
        return false;
    }

    if (m_statusService != nullptr) {
        m_statusService->showMessage(i18n("Saved %1.", displayName()));
    }

    return true;
}

void EditorTab::repathTo(const QString &newAbsolutePath)
{
    m_filePath = QFileInfo(newAbsolutePath).absoluteFilePath();
    if (m_document != nullptr) {
        m_document->openUrl(QUrl::fromLocalFile(m_filePath));
    }
}

EditorCloseChoice EditorTab::promptClose(QWidget *parentWidget)
{
    if (!isModified()) {
        return EditorCloseChoice::Discard;
    }

    QMessageBox dialog(parentWidget);
    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(i18n("Unsaved Changes"));
    dialog.setText(i18n("Save changes to %1 before closing?", displayName()));
    dialog.setStandardButtons(
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    dialog.setDefaultButton(QMessageBox::Save);

    switch (dialog.exec()) {
    case QMessageBox::Save: {
        QString saveError;
        if (!save(&saveError)) {
            if (!saveError.isEmpty() && parentWidget != nullptr) {
                QMessageBox::warning(parentWidget, i18n("Save Failed"), saveError);
            }
            return EditorCloseChoice::Cancel;
        }
        return EditorCloseChoice::Save;
    }
    case QMessageBox::Discard:
        return EditorCloseChoice::Discard;
    default:
        return EditorCloseChoice::Cancel;
    }
}

} // namespace qtcode::ui
