/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "languagedirection.h"

#include <QList>
#include <QObject>
#include <QStringList>

#include <functional>

/// The outcome of a translation request.
struct DragomanReply {
    bool ok = false;
    QStringList translations; ///< one per input segment
    QString pivot; ///< the pivot language, when the daemon pivoted
    QString error; ///< a user-presentable, translated failure message
};

/**
 * A QtDBus client for the dragomand daemon, and the project's reference
 * for calling it from Qt code.
 *
 * The daemon follows the xdg-desktop-portal request pattern: slow methods
 * return a request object path immediately, and the outcome arrives exactly
 * once as a Response(u code, a{sv} results) signal on that object. The
 * client picks a handle_token, derives the request path itself, subscribes
 * to the request's signals before calling the method (the subscription and
 * the call travel on the same connection, so the bus sees them in order and
 * no signal can be missed), and then waits.
 *
 * Every call is asynchronous and every callback runs exactly once, on this
 * object's thread; destroying the client drops all pending callbacks.
 */
class DragomanClient : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(const DragomanReply &)>;
    using Pairs = QList<Dragoman::LanguagePair>;
    using PairsCallback = std::function<void(const Pairs &pairs, const QString &error)>;

    explicit DragomanClient(QObject *parent = nullptr);

    /**
     * Translates @p segments. A missing pair is installed first with
     * PreparePair (reported through progress()) and the call retried once.
     */
    void translate(const QString &source, const QString &target, const QStringList &segments, Callback callback);

    /**
     * Every pair the daemon knows (installed or available). An empty list
     * with a non-empty error means the daemon was unreachable.
     */
    void listPairs(PairsCallback callback);

    /**
     * The request object path for @p token on the connection named
     * @p uniqueName: every character of the unique name that is not an
     * ASCII letter or digit becomes '_' (so the leading ':' does too).
     */
    [[nodiscard]] static QString requestPath(QStringView uniqueName, QStringView token);

Q_SIGNALS:
    /// Download and load progress while a pair is being prepared, with
    /// @p fraction clamped to [0, 1].
    void progress(double fraction, const QString &stage);

private:
    void startTranslate(const QString &source, const QString &target, const QStringList &segments, Callback callback, bool installOnDemand);
    void preparePair(const QString &source, const QString &target, Callback callback);
};
