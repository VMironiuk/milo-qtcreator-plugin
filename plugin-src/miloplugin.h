#pragma once

#include "milo_global.h"

#include <extensionsystem/iplugin.h>

namespace Git {
namespace Internal {
class GitClient;
} // namespace Internal
} // namespace Git

namespace Milo {
namespace Internal {

class MiloPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "Milo.json")

public:
    MiloPlugin();
    ~MiloPlugin() override;

    static MiloPlugin *instance();
    static Git::Internal::GitClient *gitClient();

    bool initialize(const QStringList &arguments, QString *errorString) override;
    void extensionsInitialized() override;
    ShutdownFlag aboutToShutdown() override;

private:
    void triggerAction();

private:
    Git::Internal::GitClient *m_gitClient = nullptr;
};

} // namespace Internal
} // namespace Milo
