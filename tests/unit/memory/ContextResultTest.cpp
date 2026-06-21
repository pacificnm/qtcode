#include "memory/ContextResult.h"

#include <QTemporaryDir>
#include <QtTest>

namespace {

[[nodiscard]] qtcode::memory::ContextResult makeResult(
    const QString &sourceUri,
    const QString &excerpt,
    double score)
{
    qtcode::memory::ContextResult result;
    result.sourceUri = sourceUri;
    result.title = sourceUri;
    result.excerpt = excerpt;
    result.score = score;
    return result;
}

} // namespace

class ContextResultTest final : public QObject
{
    Q_OBJECT

private slots:
    void dedupeMergesMatchingSourcesAndKeepsHighestScore();
    void dedupePreservesDistinctSources();
    void contextResultFromFilePathUsesProjectRelativeUri();
};

void ContextResultTest::dedupeMergesMatchingSourcesAndKeepsHighestScore()
{
    const QList<qtcode::memory::ContextResult> results = {
        makeResult(QStringLiteral("README.md"), QStringLiteral("First chunk"), 0.25),
        makeResult(QStringLiteral("README.md"), QStringLiteral("Second chunk"), 0.75),
        makeResult(QStringLiteral("docs/spec.md"), QStringLiteral("Spec chunk"), 0.5),
    };

    const QList<qtcode::memory::ContextResult> deduped =
        qtcode::memory::dedupeContextResultsBySource(results);

    QCOMPARE(deduped.size(), 2);

    const qtcode::memory::ContextResult *readmeResult = nullptr;
    for (const qtcode::memory::ContextResult &result : deduped) {
        if (result.sourceUri == QStringLiteral("README.md")) {
            readmeResult = &result;
            break;
        }
    }

    QVERIFY(readmeResult != nullptr);
    QCOMPARE(readmeResult->score, 0.75);
    QVERIFY(readmeResult->excerpt.contains(QStringLiteral("First chunk")));
    QVERIFY(readmeResult->excerpt.contains(QStringLiteral("Second chunk")));
}

void ContextResultTest::dedupePreservesDistinctSources()
{
    const QList<qtcode::memory::ContextResult> results = {
        makeResult(QStringLiteral("README.md"), QStringLiteral("Intro"), 0.4),
        makeResult(QStringLiteral("docs/README.md"), QStringLiteral("Docs intro"), 0.6),
    };

    const QList<qtcode::memory::ContextResult> deduped =
        qtcode::memory::dedupeContextResultsBySource(results);

    QCOMPARE(deduped.size(), 2);
}

void ContextResultTest::contextResultFromFilePathUsesProjectRelativeUri()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString projectRoot = tempDir.path();
    const QString filePath = projectRoot + QStringLiteral("/src/example.cpp");
    QVERIFY(QDir().mkpath(QFileInfo(filePath).absolutePath()));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("int main() { return 0; }\n");
    file.close();

    const qtcode::memory::ContextResult result =
        qtcode::memory::contextResultFromFilePath(filePath, projectRoot);
    QCOMPARE(result.sourceType, qtcode::memory::ContextSourceType::SourceFile);
    QCOMPARE(result.sourceUri, QStringLiteral("src/example.cpp"));
    QVERIFY(result.excerpt.contains(QStringLiteral("int main()")));
}

QTEST_APPLESS_MAIN(ContextResultTest)

#include "ContextResultTest.moc"
