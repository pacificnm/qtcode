#include "commands/CommandLibraryService.h"
#include "commands/QtCommandYamlParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

namespace {

[[nodiscard]] QString sampleCommandYaml()
{
    return QStringLiteral(
        "name: create-delete-button\n"
        "title: Create Delete Button\n"
        "description: Creates a project-standard delete button with confirmation behavior.\n"
        "category: ui\n"
        "version: 1\n"
        "status: stable\n"
        "tags:\n"
        "  - ui\n"
        "  - button\n"
        "aliases:\n"
        "  - delete-button\n"
        "memoryHints:\n"
        "  - remove entity action\n"
        "inputs:\n"
        "  - name: entityName\n"
        "    label: Entity Name\n"
        "    type: string\n"
        "    required: true\n"
        "    example: Artist\n"
        "template: ../templates/delete-button.tsx.tpl\n"
        "rules:\n"
        "  - ../rules/project-patterns.md\n"
        "examples:\n"
        "  - ../examples/artist-delete-button.tsx\n"
        "validation:\n"
        "  requireJsdoc: true\n"
        "  preserveFunctionNames: true\n");
}

[[nodiscard]] bool writeTextFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(content.toUtf8()) == content.toUtf8().size();
}

} // namespace

class CommandLibraryServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesCanonicalCommandInFullMode();
    void parsesIndexFieldsWithoutInputsOrValidation();
    void usesLegacyIdWhenNameMissing();
    void reportsInvalidYamlDiagnostic();
    void discoversYamlAndYmlCommands();
    void searchMatchesTagsAliasesAndMemoryHints();
    void loadCommandParsesFullDefinitionLazily();
    void invalidCommandProducesDiagnostic();
};

void CommandLibraryServiceTest::parsesCanonicalCommandInFullMode()
{
    const auto result = qtcode::commands::QtCommandYamlParser::parseContent(
        sampleCommandYaml(),
        QStringLiteral("/tmp/create-delete-button.yaml"),
        qtcode::commands::QtCommandYamlParseMode::Full);

    QVERIFY(result.success);
    QCOMPARE(result.command.index.name, QStringLiteral("create-delete-button"));
    QCOMPARE(result.command.index.title, QStringLiteral("Create Delete Button"));
    QCOMPARE(result.command.index.category, QStringLiteral("ui"));
    QCOMPARE(result.command.index.tags, QStringList({QStringLiteral("ui"), QStringLiteral("button")}));
    QCOMPARE(result.command.index.aliases, QStringList(QStringLiteral("delete-button")));
    QCOMPARE(result.command.index.memoryHints, QStringList(QStringLiteral("remove entity action")));
    QCOMPARE(result.command.version, QStringLiteral("1"));
    QCOMPARE(result.command.templatePath, QStringLiteral("../templates/delete-button.tsx.tpl"));
    QCOMPARE(result.command.rulesPaths.size(), 1);
    QCOMPARE(result.command.examplesPaths.size(), 1);
    QCOMPARE(result.command.inputs.size(), 1);
    QCOMPARE(result.command.inputs.first().name, QStringLiteral("entityName"));
    QVERIFY(result.command.validation.settings.value(QStringLiteral("requireJsdoc")));
    QVERIFY(result.command.fullyLoaded);
}

void CommandLibraryServiceTest::parsesIndexFieldsWithoutInputsOrValidation()
{
    const auto result = qtcode::commands::QtCommandYamlParser::parseContent(
        sampleCommandYaml(),
        QStringLiteral("/tmp/create-delete-button.yaml"),
        qtcode::commands::QtCommandYamlParseMode::IndexOnly);

    QVERIFY(result.success);
    QCOMPARE(result.command.index.name, QStringLiteral("create-delete-button"));
    QCOMPARE(result.command.index.tags.size(), 2);
    QCOMPARE(result.command.index.rulesPaths.size(), 1);
    QCOMPARE(result.command.index.examplesPaths.size(), 1);
    QVERIFY(result.command.inputs.isEmpty());
    QVERIFY(result.command.validation.settings.isEmpty());
    QVERIFY(!result.command.fullyLoaded);
}

void CommandLibraryServiceTest::usesLegacyIdWhenNameMissing()
{
    const QString yaml = QStringLiteral(
        "id: legacy-command\n"
        "title: Legacy Command\n"
        "description: Uses id instead of name.\n"
        "category: docs\n");

    const auto result = qtcode::commands::QtCommandYamlParser::parseContent(
        yaml,
        QStringLiteral("/tmp/legacy-command.yaml"),
        qtcode::commands::QtCommandYamlParseMode::IndexOnly);

    QCOMPARE(result.command.index.name, QStringLiteral("legacy-command"));
}

void CommandLibraryServiceTest::reportsInvalidYamlDiagnostic()
{
    const QString yaml = QStringLiteral(
        ": missing-key\n"
        "title: Broken Command\n");

    const auto result = qtcode::commands::QtCommandYamlParser::parseContent(
        yaml,
        QStringLiteral("/tmp/broken.yaml"),
        qtcode::commands::QtCommandYamlParseMode::IndexOnly);

    QVERIFY(!result.success);
    QVERIFY(std::any_of(
        result.diagnostics.cbegin(),
        result.diagnostics.cend(),
        [](const qtcode::commands::QtCommandDiagnostic &diagnostic) {
            return diagnostic.code == QStringLiteral("invalid-yaml");
        }));
}

void CommandLibraryServiceTest::discoversYamlAndYmlCommands()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString commandsDir =
        qtcode::commands::CommandLibraryService::commandsDirectoryForRoot(tempDir.path());
    QVERIFY(writeTextFile(
        QDir(commandsDir).filePath(QStringLiteral("create-delete-button.yaml")),
        sampleCommandYaml()));
    QVERIFY(writeTextFile(
        QDir(commandsDir).filePath(QStringLiteral("add-crud-route.yml")),
        QStringLiteral(
            "name: add-crud-route\n"
            "title: Add CRUD Route\n"
            "description: Adds a CRUD route.\n"
            "category: api\n"
            "version: 1\n"
            "inputs: []\n"
            "template: ../templates/crud-route.ts.tpl\n"
            "rules:\n"
            "  - ../rules/project-patterns.md\n"
            "examples:\n"
            "  - ../examples/crud-route.ts\n"
            "validation:\n"
            "  requireJsdoc: false\n")));

    qtcode::commands::CommandLibraryService service;
    service.setProjectRoot(tempDir.path());

    QCOMPARE(service.commands().size(), 2);
    QVERIFY(service.hasCommandsDirectory());
}

void CommandLibraryServiceTest::searchMatchesTagsAliasesAndMemoryHints()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString commandsDir =
        qtcode::commands::CommandLibraryService::commandsDirectoryForRoot(tempDir.path());
    QVERIFY(writeTextFile(
        QDir(commandsDir).filePath(QStringLiteral("create-delete-button.yaml")),
        sampleCommandYaml()));

    qtcode::commands::CommandLibraryService service;
    service.setProjectRoot(tempDir.path());

    QCOMPARE(service.search(QStringLiteral("delete-button")).size(), 1);
    QCOMPARE(service.search(QStringLiteral("button")).size(), 1);
    QCOMPARE(service.search(QStringLiteral("remove entity")).size(), 1);
    QCOMPARE(service.search(QStringLiteral("project-patterns")).size(), 1);
    QCOMPARE(service.search(QStringLiteral("artist-delete-button")).size(), 1);
    QVERIFY(service.search(QStringLiteral("missing-term")).isEmpty());
}

void CommandLibraryServiceTest::loadCommandParsesFullDefinitionLazily()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString commandsDir =
        qtcode::commands::CommandLibraryService::commandsDirectoryForRoot(tempDir.path());
    const QString commandPath =
        QDir(commandsDir).filePath(QStringLiteral("create-delete-button.yaml"));
    QVERIFY(writeTextFile(commandPath, sampleCommandYaml()));

    qtcode::commands::CommandLibraryService service;
    service.setProjectRoot(tempDir.path());

    const qtcode::commands::QtCommandIndexEntry indexEntry = service.commands().first();
    QVERIFY(indexEntry.diagnostics.isEmpty() || indexEntry.parseable);

    const std::optional<qtcode::commands::QtCommandDefinition> loaded =
        service.loadCommand(QStringLiteral("delete-button"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->inputs.size(), 1);
    QVERIFY(loaded->fullyLoaded);
    QVERIFY(!loaded->validation.settings.isEmpty());
}

void CommandLibraryServiceTest::invalidCommandProducesDiagnostic()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString commandsDir =
        qtcode::commands::CommandLibraryService::commandsDirectoryForRoot(tempDir.path());
    QVERIFY(writeTextFile(
        QDir(commandsDir).filePath(QStringLiteral("broken.yaml")),
        QStringLiteral(": missing-key\n")));

    qtcode::commands::CommandLibraryService service;
    service.setProjectRoot(tempDir.path());

    QCOMPARE(service.commands().size(), 1);
    QVERIFY(!service.diagnostics().isEmpty());
    QVERIFY(!service.commands().first().parseable);
}

QTEST_MAIN(CommandLibraryServiceTest)
#include "CommandLibraryServiceTest.moc"
