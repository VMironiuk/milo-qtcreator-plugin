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

#include "gitclient.h"

#include "gitsettings.h"

#include <QDir>

using namespace Utils;
using namespace VcsBase;

namespace Git {
namespace Internal {

GitClient::GitClient() : VcsBase::VcsBaseClientImpl(new GitSettings),
    m_disableEditor(false)
{
    m_gitQtcEditor = QString::fromLatin1("\"%1\" -client -block -pid %2")
            .arg(QCoreApplication::applicationFilePath())
            .arg(QCoreApplication::applicationPid());
}

VcsBaseEditorWidget *GitClient::annotate(
        const QString &workingDir, const QString &file, const QString &revision,
        int lineNumber, const QStringList &extraOptions)
{
    Q_UNUSED(workingDir)
    Q_UNUSED(file)
    Q_UNUSED(revision)
    Q_UNUSED(lineNumber)
    Q_UNUSED(extraOptions)

    return nullptr;
}

QProcessEnvironment GitClient::processEnvironment() const
{
    QProcessEnvironment environment = VcsBaseClientImpl::processEnvironment();
    QString gitPath = settings().stringValue(GitSettings::pathKey);
    if (!gitPath.isEmpty()) {
        gitPath += HostOsInfo::pathListSeparator();
        gitPath += environment.value("PATH");
        environment.insert("PATH", gitPath);
    }
    if (HostOsInfo::isWindowsHost()
            && settings().boolValue(GitSettings::winSetHomeEnvironmentKey)) {
        environment.insert("HOME", QDir::toNativeSeparators(QDir::homePath()));
    }
    environment.insert("GIT_EDITOR", m_disableEditor ? "true" : m_gitQtcEditor);
    return environment;
}

FileName GitClient::vcsBinary() const
{
    bool ok;
    Utils::FileName binary = static_cast<GitSettings &>(settings()).gitExecutable(&ok);
    if (!ok)
        return Utils::FileName();
    return binary;
}

} // namespace Internal
} // namespace Git
