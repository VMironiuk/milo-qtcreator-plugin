/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "mjsonsummarypage.h"

#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/session.h>
#include <projectexplorer/projecttree.h>

#include <coreplugin/coreconstants.h>
#include <coreplugin/iversioncontrol.h>

#include <utils/algorithm.h>
#include <utils/qtcassert.h>

#include <QDir>
#include <QMessageBox>

using namespace Core;
using namespace ProjectExplorer;

static char KEY_SELECTED_PROJECT[] = "SelectedProject";
static char KEY_SELECTED_NODE[] = "SelectedFolderNode";
static char KEY_IS_SUBPROJECT[] = "IsSubproject";
static char KEY_VERSIONCONTROL[] = "VersionControl";

namespace Milo {
namespace Internal {

// --------------------------------------------------------------------
// Helper:
// --------------------------------------------------------------------

static QString generatedProjectFilePath(const QList<JsonWizard::GeneratorFile> &files)
{
    foreach (const JsonWizard::GeneratorFile &file, files)
        if (file.file.attributes() & GeneratedFile::OpenProjectAttribute)
            return file.file.path();
    return QString();
}

static IWizardFactory::WizardKind wizardKind(JsonWizard *wiz)
{
    IWizardFactory::WizardKind kind = IWizardFactory::ProjectWizard;
    const QString kindStr = wiz->stringValue(QLatin1String("kind"));
    if (kindStr == QLatin1String(Core::Constants::WIZARD_KIND_PROJECT))
        kind = IWizardFactory::ProjectWizard;
    else if (kindStr == QLatin1String(Core::Constants::WIZARD_KIND_FILE))
        kind = IWizardFactory::FileWizard;
    else
        QTC_CHECK(false);
    return kind;
}

// --------------------------------------------------------------------
// MSummaryPageFactory:
// --------------------------------------------------------------------

static const char KEY_HIDE_PROJECT_UI[] = "hideProjectUi";

MSummaryPageFactory::MSummaryPageFactory()
{
    setTypeIdsSuffix(QLatin1String("MiloSummary"));
}

Utils::WizardPage *MSummaryPageFactory::create(JsonWizard *wizard, Core::Id typeId, const QVariant &data)
{
    Q_UNUSED(wizard);
    Q_UNUSED(data);
    QTC_ASSERT(canCreate(typeId), return nullptr);

    auto page = new MJsonSummaryPage;
    QVariant hideProjectUi = data.toMap().value(QLatin1String(KEY_HIDE_PROJECT_UI));
    page->setHideProjectUiValue(hideProjectUi);
    return page;
}

bool MSummaryPageFactory::validateData(Core::Id typeId, const QVariant &data, QString *errorMessage)
{
    QTC_ASSERT(canCreate(typeId), return false);
    if (!data.isNull() && (data.type() != QVariant::Map)) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard",
            "\"data\" for a \"Summary\" page can be unset or needs to be an object.");
        return false;
    }

    return true;
}

// --------------------------------------------------------------------
// MJsonSummaryPage:
// --------------------------------------------------------------------

MJsonSummaryPage::MJsonSummaryPage(QWidget *parent) :
    MProjectWizardPage(parent),
    m_wizard(nullptr)
{
    connect(this, &MProjectWizardPage::projectNodeChanged,
            this, &MJsonSummaryPage::summarySettingsHaveChanged);
    connect(this, &MProjectWizardPage::versionControlChanged,
            this, &MJsonSummaryPage::summarySettingsHaveChanged);
}

void MJsonSummaryPage::setHideProjectUiValue(const QVariant &hideProjectUiValue)
{
    m_hideProjectUiValue = hideProjectUiValue;
}

void MJsonSummaryPage::initializePage()
{
    m_wizard = qobject_cast<JsonWizard *>(wizard());
    QTC_ASSERT(m_wizard, return);

    m_wizard->setValue(QLatin1String(KEY_SELECTED_PROJECT), QVariant());
    m_wizard->setValue(QLatin1String(KEY_SELECTED_NODE), QVariant());
    m_wizard->setValue(QLatin1String(KEY_IS_SUBPROJECT), false);
    m_wizard->setValue(QLatin1String(KEY_VERSIONCONTROL), QString());

    connect(m_wizard, &JsonWizard::filesReady, this, &MJsonSummaryPage::triggerCommit);
    connect(m_wizard, &JsonWizard::filesReady, this, &MJsonSummaryPage::addToProject);

    updateFileList();

    IWizardFactory::WizardKind kind = wizardKind(m_wizard);
    bool isProject = (kind == IWizardFactory::ProjectWizard);

    QStringList files;
    if (isProject) {
        JsonWizard::GeneratorFile f
                = Utils::findOrDefault(m_fileList, [](const JsonWizard::GeneratorFile &f) {
            return f.file.attributes() & GeneratedFile::OpenProjectAttribute;
        });
        files << f.file.path();
    } else {
        files = Utils::transform(m_fileList,
                                 [](const JsonWizard::GeneratorFile &f) {
                                    return f.file.path();
                                 });
    }

    // Use static cast from void * to avoid qobject_cast (which needs a valid object) in value()
    // in the following code:
    auto contextNode = findWizardContextNode(static_cast<Node *>(m_wizard->value(ProjectExplorer::Constants::PREFERRED_PROJECT_NODE).value<void *>()));
    const ProjectAction currentAction = isProject ? AddSubProject : AddNewFile;

    initializeProjectTree(contextNode, files, kind, currentAction);

    // Refresh combobox on project tree changes:
    connect(ProjectTree::instance(), &ProjectTree::treeChanged,
            this, [this, files, kind, currentAction]() {
        initializeProjectTree(findWizardContextNode(currentNode()), files, kind, currentAction);
    });


    bool hideProjectUi = JsonWizard::boolFromVariant(m_hideProjectUiValue, m_wizard->expander());
    setProjectUiVisible(!hideProjectUi);

    initializeVersionControls();

    // Do a new try at initialization, now that we have real values set up:
    summarySettingsHaveChanged();
}

bool MJsonSummaryPage::validatePage()
{
    m_wizard->commitToFileList(m_fileList);
    m_fileList.clear();
    return true;
}

void MJsonSummaryPage::cleanupPage()
{
    disconnect(m_wizard, &JsonWizard::filesReady, this, nullptr);
}

void MJsonSummaryPage::triggerCommit(const JsonWizard::GeneratorFiles &files)
{
    GeneratedFiles coreFiles
            = Utils::transform(files, [](const JsonWizard::GeneratorFile &f) -> GeneratedFile
                                      { return f.file; });

    QString errorMessage;
    if (!runVersionControl(coreFiles, &errorMessage)) {
        QMessageBox::critical(wizard(), tr("Failed to Commit to Version Control"),
                              tr("Error message from Version Control System: \"%1\".")
                              .arg(errorMessage));
    }
}

void MJsonSummaryPage::addToProject(const JsonWizard::GeneratorFiles &files)
{
    QTC_CHECK(m_fileList.isEmpty()); // Happens after this page is done
    QString generatedProject = generatedProjectFilePath(files);
    IWizardFactory::WizardKind kind = wizardKind(m_wizard);

    FolderNode *folder = currentNode();
    if (!folder)
        return;
    if (kind == IWizardFactory::ProjectWizard) {
        if (!static_cast<ProjectNode *>(folder)->addSubProject(generatedProject)) {
            QMessageBox::critical(m_wizard, tr("Failed to Add to Project"),
                                  tr("Failed to add subproject \"%1\"\nto project \"%2\".")
                                  .arg(QDir::toNativeSeparators(generatedProject))
                                  .arg(folder->filePath().toUserOutput()));
            return;
        }
        m_wizard->removeAttributeFromAllFiles(GeneratedFile::OpenProjectAttribute);
    } else {
        QStringList filePaths = Utils::transform(files, [](const JsonWizard::GeneratorFile &f) {
            return f.file.path();
        });
        if (!folder->addFiles(filePaths)) {
            QStringList nativeFilePaths = Utils::transform(filePaths, &QDir::toNativeSeparators);
            QMessageBox::critical(wizard(), tr("Failed to Add to Project"),
                                  tr("Failed to add one or more files to project\n\"%1\" (%2).")
                                  .arg(folder->filePath().toUserOutput(),
                                       nativeFilePaths.join(QLatin1String(", "))));
            return;
        }
    }
    return;
}

void MJsonSummaryPage::summarySettingsHaveChanged()
{
    IVersionControl *vc = currentVersionControl();
    m_wizard->setValue(QLatin1String(KEY_VERSIONCONTROL), vc ? vc->id().toString() : QString());

    updateProjectData(currentNode());
}

ProjectExplorer::Node *MJsonSummaryPage::findWizardContextNode(Node *contextNode) const
{
    if (contextNode && !ProjectTree::hasNode(contextNode)) {
        contextNode = nullptr;

        // Static cast from void * to avoid qobject_cast (which needs a valid object) in value().
        auto project = static_cast<Project *>(m_wizard->value(ProjectExplorer::Constants::PROJECT_POINTER).value<void *>());
        if (SessionManager::projects().contains(project) && project->rootProjectNode()) {
            const QString path = m_wizard->value(ProjectExplorer::Constants::PREFERRED_PROJECT_NODE_PATH).toString();
            contextNode = project->rootProjectNode()->findNode([path](const Node *n) {
                return path == n->filePath().toString();
            });
        }
    }
    return contextNode;
}

void MJsonSummaryPage::updateFileList()
{
    m_fileList = m_wizard->generateFileList();
    QStringList filePaths
            = Utils::transform(m_fileList, [](const JsonWizard::GeneratorFile &f) { return f.file.path(); });
    setFiles(filePaths);
}

void MJsonSummaryPage::updateProjectData(FolderNode *node)
{
    Project *project = SessionManager::projectForNode(node);

    m_wizard->setValue(QLatin1String(KEY_SELECTED_PROJECT), QVariant::fromValue(project));
    m_wizard->setValue(QLatin1String(KEY_SELECTED_NODE), QVariant::fromValue(node));
    m_wizard->setValue(QLatin1String(KEY_IS_SUBPROJECT), node ? true : false);

    updateFileList();
}

} // namespace Internal
} // namespace Milo
