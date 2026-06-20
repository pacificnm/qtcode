#include "app/QtCodeApplication.h"

#include "shared/Logging.h"
#include "ui/MainWindow.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QSysInfo>

namespace {

constexpr auto kApplicationName = QT_TRANSLATE_NOOP("AppMetadata", "QTCode");
constexpr auto kApplicationDescription = QT_TRANSLATE_NOOP(
    "AppMetadata",
    "Native KDE/Linux developer cockpit for AI agents, terminals, and project memory.");
constexpr auto kCopyright = QT_TRANSLATE_NOOP("AppMetadata", "Copyright 2026 QTCode contributors");

} // namespace

QtCodeApplication::QtCodeApplication(int &argc, char **argv)
    : m_application(std::make_unique<QApplication>(argc, argv))
{
    configureMetadata();
    qtcode::shared::configureLogging();

    qCInfo(qtcodeApp) << "QTCode" << QCoreApplication::applicationVersion()
                      << "starting with Qt" << qVersion();
}

QtCodeApplication::~QtCodeApplication() = default;

int QtCodeApplication::run()
{
    qtcode::ui::MainWindow mainWindow;
    mainWindow.show();

    qCInfo(qtcodeApp) << "Main window shown";

    return QApplication::exec();
}

QApplication &QtCodeApplication::application() noexcept
{
    return *m_application;
}

void QtCodeApplication::configureMetadata()
{
    KLocalizedString::setApplicationDomain("qtcode");

    KAboutData aboutData(
        QStringLiteral("qtcode"),
        i18n(kApplicationName),
        QStringLiteral(QTcode_VERSION),
        i18n(kApplicationDescription),
        KAboutLicense::MIT,
        i18n(kCopyright),
        QString(),
        QStringLiteral("https://github.com/pacificnm/qtcode"));

    aboutData.setDesktopFileName(QStringLiteral("org.qtcode.QTCode"));
    KAboutData::setApplicationData(aboutData);

    QCoreApplication::setApplicationName(QStringLiteral("qtcode"));
    QGuiApplication::setApplicationDisplayName(i18n(kApplicationName));
    QCoreApplication::setApplicationVersion(QStringLiteral(QTcode_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("QTCode"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qtcode.dev"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qtcode.QTCode"));
}
