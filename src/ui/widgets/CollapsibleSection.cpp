#include "ui/widgets/CollapsibleSection.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>

namespace qtcode::ui {

CollapsibleSection::CollapsibleSection(
    const QString &title,
    bool expandedByDefault,
    QWidget *parent)
    : QWidget(parent)
    , m_expanded(expandedByDefault)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(4);

    m_headerWidget = new QWidget(this);
    m_headerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_headerWidget->setCursor(Qt::PointingHandCursor);
    m_headerWidget->installEventFilter(this);

    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    m_toggleButton = new QToolButton(m_headerWidget);
    m_toggleButton->setAutoRaise(true);
    m_toggleButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    connect(m_toggleButton, &QToolButton::clicked, this, &CollapsibleSection::onHeaderActivated);

    m_titleLabel = new QLabel(title, m_headerWidget);
    QFont font = m_titleLabel->font();
    font.setBold(true);
    m_titleLabel->setFont(font);
    m_titleLabel->setCursor(Qt::PointingHandCursor);
    m_titleLabel->installEventFilter(this);

    headerLayout->addWidget(m_toggleButton);
    headerLayout->addWidget(m_titleLabel, 1);

    m_headerTrailingLayout = new QHBoxLayout();
    m_headerTrailingLayout->setContentsMargins(0, 0, 0, 0);
    m_headerTrailingLayout->setSpacing(4);
    headerLayout->addLayout(m_headerTrailingLayout);

    m_contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(16, 0, 0, 0);
    contentLayout->setSpacing(4);

    outerLayout->addWidget(m_headerWidget, 0);
    outerLayout->addWidget(m_contentWidget, 1);

    setExpanded(expandedByDefault);
}

void CollapsibleSection::setTitle(const QString &title)
{
    if (m_titleLabel != nullptr) {
        m_titleLabel->setText(title);
    }
}

void CollapsibleSection::setExpanded(bool expanded)
{
    const bool stateChanged = m_expanded != expanded;

    m_expanded = expanded;
    if (m_contentWidget != nullptr) {
        m_contentWidget->setVisible(expanded);
    }

    if (auto *outerLayout = qobject_cast<QVBoxLayout *>(layout())) {
        const int contentIndex = outerLayout->indexOf(m_contentWidget);
        if (contentIndex >= 0) {
            outerLayout->setStretch(contentIndex, expanded ? 1 : 0);
        }
    }

    const auto verticalPolicy = expanded ? QSizePolicy::Expanding : QSizePolicy::Maximum;
    setSizePolicy(QSizePolicy::Expanding, verticalPolicy);
    if (m_contentWidget != nullptr) {
        m_contentWidget->setSizePolicy(QSizePolicy::Expanding, verticalPolicy);
    }

    updateToggleIcon();

    if (stateChanged) {
        emit expandedChanged(expanded);
    }
}

bool CollapsibleSection::isExpanded() const
{
    return m_expanded;
}

QVBoxLayout *CollapsibleSection::contentLayout() const
{
    return m_contentWidget != nullptr
        ? qobject_cast<QVBoxLayout *>(m_contentWidget->layout())
        : nullptr;
}

QHBoxLayout *CollapsibleSection::headerTrailingLayout() const
{
    return m_headerTrailingLayout;
}

bool CollapsibleSection::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_headerWidget || watched == m_titleLabel)
        && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            onHeaderActivated();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CollapsibleSection::updateToggleIcon()
{
    if (m_toggleButton == nullptr) {
        return;
    }

    const char *iconName = m_expanded ? "go-down" : "go-next";
    m_toggleButton->setIcon(QIcon::fromTheme(iconName));
}

void CollapsibleSection::onHeaderActivated()
{
    setExpanded(!m_expanded);
}

} // namespace qtcode::ui
