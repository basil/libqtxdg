/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * Copyright: 2025
 * Authors:
 *   Basil Crow <me@basilcrow.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef QTXDG_XDGPROCESS__INCLUDED
#define QTXDG_XDGPROCESS__INCLUDED

#include "xdgmacros.h"

#include <QString>
#include <QStringList>

/**
 * Helper for starting detached processes, optionally as transient systemd units tied to a given
 * slice.
 */
class QTXDG_API XdgProcess {
public:
    /**
     * Start a detached process.
     *
     * If @p slice is empty this is equivalent to QProcess::startDetached().
     *
     * If a slice is provided, this will attempt to launch the process as a transient systemd
     * service in the specified slice (subject to systemd availability). If systemd cannot be used,
     * it falls back to QProcess::startDetached().
     */
    static bool startDetached(const QString &program,
                              const QStringList &arguments = {},
                              const QString &workingDirectory = QString(),
                              const QString &slice = QString());

    /**
     * Start a detached process with an optional application ID and description for the transient
     * systemd unit.
     *
     * The semantics of @p slice are the same as for the overload above.
     */
    static bool startDetached(const QString &program,
                              const QStringList &arguments,
                              const QString &workingDirectory,
                              const QString &slice,
                              const QString &appId,
                              const QString &description);
};

#endif // QTXDG_XDGPROCESS__INCLUDED
