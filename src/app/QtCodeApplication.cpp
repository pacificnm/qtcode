#include "app/QtCodeApplication.h"

#include "agents/AgentManager.h"
#include "agents/AgentSession.h"
#include "core/ApplicationController.h"
#include "core/AppConfigService.h"
#include "shared/Logging.h"
#include "ui/MainWindow.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QGuiApplication>
#include <QObject>
#include <QSysInfo>
#include <QTimer>

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
    m_controller = std::make_unique<qtcode::core::ApplicationController>();

    QString initializationError;
    if (!m_controller->initialize(&initializationError)) {
        qCCritical(qtcodeApp) << "Failed to initialize application services:"
                                << initializationError;
        return 1;
    }

    int exitCode = 0;
    {
        qtcode::ui::MainWindow mainWindow(m_controller.get());
        if (m_controller->appConfigService() != nullptr
            && m_controller->appConfigService()->config().startMaximized) {
            mainWindow.showMaximized();
        } else {
            mainWindow.show();
        }

        qCInfo(qtcodeApp) << "Main window shown";

        QString smokeTestError;
        QString smokeTestSessionId;
        if (!m_controller->runSmokeTestAgentPromptIfRequested(&smokeTestError, &smokeTestSessionId)) {
            qCWarning(qtcodeApp) << "Smoke-test agent prompt failed:" << smokeTestError;
        }

        QString memorySmokeError;
        if (!m_controller->runSmokeTestMemorySearchIfRequested(&memorySmokeError)) {
            qCWarning(qtcodeApp) << "Smoke-test memory search failed:" << memorySmokeError;
        }

        if (qEnvironmentVariableIsSet("QTCODE_WORKSPACE_SMOKE")) {
            QString workspaceSmokeError;
            if (!mainWindow.runWorkspaceSmokeChecks(&workspaceSmokeError)) {
                qCCritical(qtcodeApp) << "Workspace smoke check failed:" << workspaceSmokeError;
                return 1;
            }
        }

        if (qEnvironmentVariableIsSet("QTCODE_AUTO_QUIT")) {
            const int fallbackDelayMs = smokeTestSessionId.isEmpty() ? 200 : 120000;
            QTimer::singleShot(fallbackDelayMs, &mainWindow, &QWidget::close);

            if (!smokeTestSessionId.isEmpty() && m_controller->agentManager() != nullptr) {
                QObject::connect(
                    m_controller->agentManager(),
                    &qtcode::agents::AgentManager::sessionUpdated,
                    &mainWindow,
                    [&mainWindow, smokeTestSessionId](qtcode::agents::AgentSession *session) {
                        if (session == nullptr || session->id() != smokeTestSessionId) {
                            return;
                        }

                        if (session->status() == qtcode::agents::AgentSessionStatus::Completed
                            || session->status() == qtcode::agents::AgentSessionStatus::Failed
                            || session->status() == qtcode::agents::AgentSessionStatus::Canceled) {
                            mainWindow.close();
                        }
                    });
            }
        }

        exitCode = QApplication::exec();
    }

    m_controller->shutdown();
    m_controller.reset();

    return exitCode;
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
