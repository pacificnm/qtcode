#pragma once

#include "core/ContextManager.h"
#include "memory/ContextResult.h"

#include <QList>
#include <QWidget>

class QListWidget;
class QPushButton;
class QTextEdit;

namespace qtcode::ui {

class ContextResultsView final : public QWidget
{
    Q_OBJECT

public:
    explicit ContextResultsView(QWidget *parent = nullptr);

    void setRetrievalOutcome(const qtcode::core::ContextRetrievalOutcome &outcome);
    void clearResults();

    [[nodiscard]] QList<memory::ContextResult> attachedResults() const;
    [[nodiscard]] QStringList attachedContextExcerpts() const;

signals:
    void attachmentChanged();

private slots:
    void onResultSelectionChanged();
    void attachAllResults();
    void detachAllResults();
    void attachSelectedResult();
    void detachSelectedResult();

private:
    void configureLayout();
    void refreshDetailView();
    void setResultAttached(int row, bool attached);
    [[nodiscard]] static QString resultListLabel(const memory::ContextResult &result, bool attached);

    QList<memory::ContextResult> m_results;
    QList<bool> m_attachedStates;
    QListWidget *m_resultList = nullptr;
    QTextEdit *m_detailView = nullptr;
    QPushButton *m_attachAllButton = nullptr;
    QPushButton *m_detachAllButton = nullptr;
    QPushButton *m_attachSelectedButton = nullptr;
    QPushButton *m_detachSelectedButton = nullptr;
    QString m_statusMessage;
    bool m_memoryUnavailable = false;
};

} // namespace qtcode::ui
