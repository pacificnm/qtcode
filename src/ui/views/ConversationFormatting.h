#pragma once

#include <QPalette>
#include <QString>

namespace qtcode::ui {

enum class ActivityKind
{
    Shell,
    File,
    Search,
    Generic,
};

struct ParsedActivity
{
    QString verb;
    QString detail;
    ActivityKind kind = ActivityKind::Generic;
};

[[nodiscard]] QString truncateForDisplay(const QString &text, int maxLength = 120);
[[nodiscard]] QString shortPath(const QString &path);
[[nodiscard]] ParsedActivity parseActivityText(const QString &activityText);
[[nodiscard]] ActivityKind activityKindForVerb(const QString &verb);
[[nodiscard]] QString activityCardObjectName(ActivityKind kind);
[[nodiscard]] QString activityCardHeaderLabel(const ParsedActivity &parsed);
[[nodiscard]] QString formatContentHtml(const QString &content, const QPalette &palette);
[[nodiscard]] QString messageSignature(const QString &id, const QString &role, const QString &content);

} // namespace qtcode::ui
