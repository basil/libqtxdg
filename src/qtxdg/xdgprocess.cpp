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

#include "meta_types.h"
#include "xdgprocess.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>

namespace {
    QString systemdEscape(const QString &input) {
        // Per https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html#Description
        // the "unit name prefix" must consist of one or more valid characters (ASCII letters,
        // digits, ":", "-", "_", ".", and "\").
        QString out = input;

        auto isInvalidChar = [](QChar c) {
            const ushort u = c.unicode();
            if ((u >= 'a' && u <= 'z') ||
                (u >= 'A' && u <= 'Z') ||
                (u >= '0' && u <= '9')) {
                return false;
            }

            if (c == QLatin1Char(':') ||
                c == QLatin1Char('-') ||
                c == QLatin1Char('_') ||
                c == QLatin1Char('.') ||
                c == QLatin1Char('\\')) {
                return false;
            }

            return true;
        };

        for (QChar &ch: out) {
            if (isInvalidChar(ch)) {
                ch = QLatin1Char('_');
            }
        }

        return out;
    }

    QString systemdUnitName(const QString &program, const QString &appId) {
        // Per https://systemd.io/DESKTOP_ENVIRONMENTS/#xdg-standardization-for-applications
        // application units should follow the scheme
        // app[-<launcher>]-<ApplicationID>[@<RANDOM>].service
        QString unitNamePrefix = QStringLiteral("app-");

        const QStringList xdgCurrentDesktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts);
        if (!xdgCurrentDesktops.isEmpty()) {
            unitNamePrefix += xdgCurrentDesktops.constFirst();
            unitNamePrefix += QLatin1Char('-');
        }

        if (appId.isEmpty()) {
            // Per https://systemd.io/DESKTOP_ENVIRONMENTS/#xdg-standardization-for-applications if
            // no application ID is available, the launcher should generate a reasonable name when
            // possible (e.g. using basename(argv[0])). This name must not contain a - character.
            unitNamePrefix += QFileInfo(program).completeBaseName();
        } else {
            unitNamePrefix += appId;
        }
        unitNamePrefix += QLatin1Char('-');

        unitNamePrefix = systemdEscape(unitNamePrefix);

        // Per https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html#Description
        // the total length of the unit name including the suffix must not exceed
        // 255 characters.
        constexpr qsizetype maxUnitNamePrefixLen = 220;
        if (unitNamePrefix.size() > maxUnitNamePrefixLen) {
            unitNamePrefix.truncate(maxUnitNamePrefixLen);
        }

        const quint64 r = QRandomGenerator::global()->generate64();
        const QString randHex = QString::number(r, 16).rightJustified(16, QLatin1Char('0'));

        return QStringLiteral("%1@%2.service").arg(unitNamePrefix, randHex);
    }
} // namespace

bool XdgProcess::startDetached(const QString &program,
                               const QStringList &arguments,
                               const QString &workingDirectory,
                               const QString &slice)
{
    return XdgProcess::startDetached(program,
                                     arguments,
                                     workingDirectory,
                                     slice,
                                     QString(),
                                     QString());
}

bool XdgProcess::startDetached(const QString &program,
                               const QStringList &arguments,
                               const QString &workingDirectory,
                               const QString &slice,
                               const QString &appId,
                               const QString &description)
{
    if (slice.isEmpty()) {
        return QProcess::startDetached(program, arguments, workingDirectory);
    }

    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return QProcess::startDetached(program, arguments, workingDirectory);
    }

    QDBusConnectionInterface *iface = bus.interface();
    if (!iface || !iface->isServiceRegistered(QStringLiteral("org.freedesktop.systemd1"))) {
        return QProcess::startDetached(program, arguments, workingDirectory);
    }



    SystemdDBusPropertyList properties;

    if (description.isEmpty()) {
        properties.append({QStringLiteral("Description"), QFileInfo(program).fileName()});
    } else {
        properties.append({QStringLiteral("Description"), description});
    }
    properties.append({QStringLiteral("CollectMode"), QStringLiteral("inactive-or-failed")});
    if (!slice.isEmpty()) {
        properties.append({QStringLiteral("Slice"), slice});
    }
    properties.append({QStringLiteral("Type"), QStringLiteral("exec")});
    properties.append({QStringLiteral("ExitType"), QStringLiteral("cgroup")});
    if (!workingDirectory.isEmpty()) {
        properties.append({QStringLiteral("WorkingDirectory"), workingDirectory});
    }

    SystemdDBusExecCommand execCmd;
    execCmd.path = program;
    execCmd.args = QStringList{program} + arguments;
    execCmd.ignoreFailure = false;

    SystemdDBusExecCommandList commands;
    commands.append(execCmd);
    properties.append({
        QStringLiteral("ExecStart"),
        QVariant::fromValue(commands)
    });

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.systemd1.Manager"),
        QStringLiteral("StartTransientUnit"));
    msg << systemdUnitName(program, appId) << QStringLiteral("fail");
    msg << QVariant::fromValue(properties);

    SystemdDBusAuxUnitList auxUnits;
    msg << QVariant::fromValue(auxUnits);

    QDBusReply<QDBusObjectPath> reply = bus.call(msg);
    return reply.isValid();
}
