#pragma once

#include <QWidget>

class QLabel;
class QHBoxLayout;
class QToolButton;
class QVBoxLayout;

namespace qtcode::ui {

class CollapsibleSection final : public QWidget
{
    Q_OBJECT

public:
    explicit CollapsibleSection(
        const QString &title,
        bool expandedByDefault = false,
        QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setExpanded(bool expanded);
    [[nodiscard]] bool isExpanded() const;

    [[nodiscard]] QVBoxLayout *contentLayout() const;
    [[nodiscard]] QHBoxLayout *headerTrailingLayout() const;

signals:
    void expandedChanged(bool expanded);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void updateToggleIcon();
    void onHeaderActivated();

    QToolButton *m_toggleButton = nullptr;
    QLabel *m_titleLabel = nullptr;
    QWidget *m_headerWidget = nullptr;
    QWidget *m_contentWidget = nullptr;
    QHBoxLayout *m_headerTrailingLayout = nullptr;
    bool m_expanded = false;
};

} // namespace qtcode::ui
