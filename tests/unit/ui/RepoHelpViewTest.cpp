#include "ui/views/RepoHelpView.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTextBrowser>
#include <QTemporaryDir>
#include <QtTest>

class RepoHelpViewTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadHelpEntryRendersMarkdown();
    void loadHelpEntryShowsMissingFileMessage();
    void loadHelpEntryUsesReadmeEntryFile();
    void relativeMarkdownLinkNavigatesWithinDocRoot();
    void mainReadmeLinkNavigatesBackToEntry();
};

void RepoHelpViewTest::loadHelpEntryRendersMarkdown()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir docDir(tempDir.path() + QStringLiteral("/doc"));
    QVERIFY(docDir.mkpath(QStringLiteral(".")));

    QFile indexFile(docDir.filePath(QStringLiteral("index.md")));
    QVERIFY(indexFile.open(QIODevice::WriteOnly | QIODevice::Text));
    indexFile.write("# Repo Help\n\n| Column |\n| --- |\n| Value |\n");
    indexFile.close();

    qtcode::ui::RepoHelpView view(nullptr);
    view.loadHelpEntry(docDir.filePath(QStringLiteral("index.md")));

    QCOMPARE(view.docRootPath(), docDir.absolutePath());
    QCOMPARE(view.currentFilePath(), docDir.filePath(QStringLiteral("index.md")));

    const auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser != nullptr);
    QVERIFY(!browser->toPlainText().trimmed().isEmpty());
    QVERIFY(browser->document()->toPlainText().contains(QStringLiteral("Repo Help")));
}

void RepoHelpViewTest::loadHelpEntryShowsMissingFileMessage()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir docDir(tempDir.path() + QStringLiteral("/doc"));
    QVERIFY(docDir.mkpath(QStringLiteral(".")));

    qtcode::ui::RepoHelpView view(nullptr);
    view.loadHelpEntry(docDir.filePath(QStringLiteral("index.md")));

    const auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser != nullptr);
    QVERIFY(browser->toPlainText().contains(QStringLiteral("index.md")));
    QVERIFY(view.currentFilePath().isEmpty());
}

void RepoHelpViewTest::loadHelpEntryUsesReadmeEntryFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir docDir(tempDir.path() + QStringLiteral("/docs"));
    QVERIFY(docDir.mkpath(QStringLiteral(".")));

    QFile readmeFile(docDir.filePath(QStringLiteral("README.md")));
    QVERIFY(readmeFile.open(QIODevice::WriteOnly | QIODevice::Text));
    readmeFile.write("# Project Docs\n\nWelcome.\n");
    readmeFile.close();

    qtcode::ui::RepoHelpView view(nullptr);
    view.loadHelpEntry(docDir.filePath(QStringLiteral("README.md")));

    QCOMPARE(view.currentFilePath(), docDir.filePath(QStringLiteral("README.md")));
    const auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser != nullptr);
    QVERIFY(browser->document()->toPlainText().contains(QStringLiteral("Project Docs")));
}

void RepoHelpViewTest::relativeMarkdownLinkNavigatesWithinDocRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir docDir(tempDir.path() + QStringLiteral("/doc"));
    QVERIFY(docDir.mkpath(QStringLiteral(".")));

    QFile indexFile(docDir.filePath(QStringLiteral("index.md")));
    QVERIFY(indexFile.open(QIODevice::WriteOnly | QIODevice::Text));
    indexFile.write("# Index\n\nSee [details](details.md).\n");
    indexFile.close();

    QFile detailsFile(docDir.filePath(QStringLiteral("details.md")));
    QVERIFY(detailsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    detailsFile.write("# Details\n\nMore help.\n");
    detailsFile.close();

    qtcode::ui::RepoHelpView view(nullptr);
    view.loadHelpEntry(docDir.filePath(QStringLiteral("index.md")));

    const auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser != nullptr);

    const QUrl detailsUrl = QUrl(QStringLiteral("details.md"));
    QMetaObject::invokeMethod(&view, "onAnchorClicked", Q_ARG(QUrl, detailsUrl));

    QCOMPARE(view.currentFilePath(), docDir.filePath(QStringLiteral("details.md")));
    QVERIFY(browser->document()->toPlainText().contains(QStringLiteral("Details")));
}

void RepoHelpViewTest::mainReadmeLinkNavigatesBackToEntry()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir docDir(tempDir.path() + QStringLiteral("/docs"));
    QVERIFY(docDir.mkpath(QStringLiteral("plans")));

    QFile readmeFile(docDir.filePath(QStringLiteral("README.md")));
    QVERIFY(readmeFile.open(QIODevice::WriteOnly | QIODevice::Text));
    readmeFile.write("# Project Docs\n\nWelcome.\n");
    readmeFile.close();

    QFile roadmapFile(docDir.filePath(QStringLiteral("plans/roadmap.md")));
    QVERIFY(roadmapFile.open(QIODevice::WriteOnly | QIODevice::Text));
    roadmapFile.write("# Roadmap\n\nFuture work.\n");
    roadmapFile.close();

    qtcode::ui::RepoHelpView view(nullptr);
    view.loadHelpEntry(docDir.filePath(QStringLiteral("README.md")));

    const auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser != nullptr);

    const QUrl roadmapUrl = QUrl(QStringLiteral("plans/roadmap.md"));
    QMetaObject::invokeMethod(&view, "onAnchorClicked", Q_ARG(QUrl, roadmapUrl));

    QCOMPARE(view.currentFilePath(), docDir.filePath(QStringLiteral("plans/roadmap.md")));
    QVERIFY(browser->document()->toPlainText().contains(QStringLiteral("Main Readme")));
    QVERIFY(browser->document()->toPlainText().contains(QStringLiteral("Roadmap")));

    const QUrl mainReadmeUrl = QUrl(QStringLiteral("../README.md"));
    QMetaObject::invokeMethod(&view, "onAnchorClicked", Q_ARG(QUrl, mainReadmeUrl));

    QCOMPARE(view.currentFilePath(), docDir.filePath(QStringLiteral("README.md")));
    QVERIFY(browser->document()->toPlainText().contains(QStringLiteral("Project Docs")));
    QVERIFY(!browser->document()->toPlainText().contains(QStringLiteral("Main Readme")));
}

QObject *buildRepoHelpViewTest()
{
    return new RepoHelpViewTest();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("qtcode");

    KAboutData aboutData(
        QStringLiteral("qtcode"),
        QStringLiteral("QTCode"),
        QStringLiteral("0.1.0"),
        QStringLiteral("Test"),
        KAboutLicense::MIT);
    KAboutData::setApplicationData(aboutData);

    return QTest::qExec(buildRepoHelpViewTest(), argc, argv);
}

#include "RepoHelpViewTest.moc"
