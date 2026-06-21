#pragma once

#include <QWidget>

namespace KTextEditor {
class Document;
class View;
} // namespace KTextEditor

namespace qtcode::core {
class StatusService;
} // namespace qtcode::core

namespace qtcode::ui {

enum class EditorCloseChoice {
    Save,
    Discard,
    Cancel,
};

class EditorTab final : public QWidget
{
    Q_OBJECT

public:
    EditorTab(
        const QString &absolutePath,
        qtcode::core::StatusService *statusService,
        QWidget *parent = nullptr);

    [[nodiscard]] QString filePath() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool isLoadSuccessful() const;
    [[nodiscard]] QString loadErrorMessage() const;
    [[nodiscard]] bool isModified() const;
    [[nodiscard]] bool save(QString *errorMessage = nullptr);
    [[nodiscard]] EditorCloseChoice promptClose(QWidget *parentWidget);
    void repathTo(const QString &newAbsolutePath);

signals:
    void modificationChanged(bool modified);

private:
    void configureEditor();

    qtcode::core::StatusService *m_statusService = nullptr;
    KTextEditor::Document *m_document = nullptr;
    KTextEditor::View *m_view = nullptr;
    QString m_filePath;
    bool m_loadSucceeded = false;
    QString m_loadErrorMessage;
};

} // namespace qtcode::ui
