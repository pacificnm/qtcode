#include "ui/panels/EditorTab.h"
#include "ui/panels/WorkspaceTabs.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KTextEditor/Application>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QLabel>
#include <QFile>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QObject g_ktextEditorHost;
std::unique_ptr<KTextEditor::Application> g_ktextEditorApplication;

void ensureKTextEditorApplication()
{
    if (g_ktextEditorApplication != nullptr) {
        return;
    }

    g_ktextEditorApplication = std::make_unique<KTextEditor::Application>(&g_ktextEditorHost);
    KTextEditor::Editor::instance()->setApplication(g_ktextEditorApplication.get());
}

KTextEditor::Document *documentForEditorTab(qtcode::ui::EditorTab *editorTab)
{
    if (editorTab == nullptr) {
        return nullptr;
    }

    const auto *view = editorTab->findChild<KTextEditor::View *>();
    return view != nullptr ? view->document() : nullptr;
}

} // namespace

class WorkspaceTabsTest final : public QObject
{
    Q_OBJECT

private slots:
    void permanentAiChatTabHasNoCloseButton();
    void openFileAddsEditorTab();
    void reopeningSameFileDoesNotDuplicateTab();
    void modifiedEditorTabReportsDirtyState();
    void saveCurrentEditorTabWritesToDisk();
    void unmodifiedEditorTabPromptCloseReturnsDiscard();
};

void WorkspaceTabsTest::permanentAiChatTabHasNoCloseButton()
{
    qtcode::ui::WorkspaceTabs tabs(nullptr, nullptr);
    auto *chatLabel = new QLabel(QStringLiteral("AI Chat"), &tabs);
    tabs.setPermanentAiChatTab(chatLabel);

    QCOMPARE(tabs.aiChatTabIndex(), 0);
    QCOMPARE(tabs.tabCount(), 1);

    const auto *tabWidget = tabs.findChild<QTabWidget *>();
    QVERIFY(tabWidget != nullptr);
    QVERIFY(tabWidget->tabBar()->tabButton(0, QTabBar::RightSide) == nullptr);
}

void WorkspaceTabsTest::openFileAddsEditorTab()
{
    ensureKTextEditorApplication();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("sample.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("hello") > 0);
    file.close();

    qtcode::ui::WorkspaceTabs tabs(nullptr, nullptr);
    auto *chatLabel = new QLabel(QStringLiteral("AI Chat"), &tabs);
    tabs.setPermanentAiChatTab(chatLabel);

    tabs.requestOpenFile(filePath);
    QCOMPARE(tabs.tabCount(), 2);
    QVERIFY(tabs.hasActiveEditorTab());
}

void WorkspaceTabsTest::reopeningSameFileDoesNotDuplicateTab()
{
    ensureKTextEditorApplication();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("sample.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("hello") > 0);
    file.close();

    qtcode::ui::WorkspaceTabs tabs(nullptr, nullptr);
    auto *chatLabel = new QLabel(QStringLiteral("AI Chat"), &tabs);
    tabs.setPermanentAiChatTab(chatLabel);

    tabs.requestOpenFile(filePath);
    tabs.requestOpenFile(filePath);
    QCOMPARE(tabs.tabCount(), 2);
}

void WorkspaceTabsTest::modifiedEditorTabReportsDirtyState()
{
    ensureKTextEditorApplication();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("edit-me.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("before") > 0);
    file.close();

    qtcode::ui::WorkspaceTabs tabs(nullptr, nullptr);
    auto *chatLabel = new QLabel(QStringLiteral("AI Chat"), &tabs);
    tabs.setPermanentAiChatTab(chatLabel);
    tabs.requestOpenFile(filePath);

    QVERIFY(tabs.hasActiveEditorTab());
    QVERIFY(!tabs.currentEditorTabIsModified());

    auto *editorTab = tabs.findChild<qtcode::ui::EditorTab *>();
    QVERIFY(editorTab != nullptr);

    KTextEditor::Document *document = documentForEditorTab(editorTab);
    QVERIFY(document != nullptr);
    document->setText(QStringLiteral("before changed"));

    QVERIFY(editorTab->isModified());
    QVERIFY(tabs.currentEditorTabIsModified());
}

void WorkspaceTabsTest::saveCurrentEditorTabWritesToDisk()
{
    ensureKTextEditorApplication();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("save-me.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("before") > 0);
    file.close();

    qtcode::ui::WorkspaceTabs tabs(nullptr, nullptr);
    auto *chatLabel = new QLabel(QStringLiteral("AI Chat"), &tabs);
    tabs.setPermanentAiChatTab(chatLabel);
    tabs.requestOpenFile(filePath);

    auto *editorTab = tabs.findChild<qtcode::ui::EditorTab *>();
    QVERIFY(editorTab != nullptr);

    KTextEditor::Document *document = documentForEditorTab(editorTab);
    QVERIFY(document != nullptr);
    document->setText(QStringLiteral("saved content"));

    QVERIFY(tabs.saveCurrentEditorTab());
    QVERIFY(!editorTab->isModified());

    QFile savedFile(filePath);
    QVERIFY(savedFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(savedFile.readAll()).trimmed(), QStringLiteral("saved content"));
}

void WorkspaceTabsTest::unmodifiedEditorTabPromptCloseReturnsDiscard()
{
    ensureKTextEditorApplication();
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("close-me.txt"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("stable") > 0);
    file.close();

    qtcode::ui::EditorTab editorTab(filePath, nullptr);
    QVERIFY(editorTab.isLoadSuccessful());
    QVERIFY(!editorTab.isModified());
    QCOMPARE(editorTab.promptClose(nullptr), qtcode::ui::EditorCloseChoice::Discard);
}

QObject *buildWorkspaceTabsTest()
{
    return new WorkspaceTabsTest();
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

    return QTest::qExec(buildWorkspaceTabsTest(), argc, argv);
}

#include "WorkspaceTabsTest.moc"
