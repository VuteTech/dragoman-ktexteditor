/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "dragomanclient.h"
#include "languagedirection.h"

#include <KTextEditor/Message>
#include <KTextEditor/Plugin>
#include <KXMLGUIClient>

#include <QPointer>

namespace KTextEditor
{
class MainWindow;
class View;
}
class QAction;

/**
 * A KTextEditor plugin (Kate, KWrite, KDevelop) that translates the
 * selection in place through the dragomand daemon, speaking D-Bus directly
 * (see Dragoman::Client in libdragoman-qt).
 */
class DragomanPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit DragomanPlugin(QObject *parent, const QVariantList &args = {});

    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
};

/// The plugin's per-window part: actions, menu entries and messages.
class DragomanPluginView : public QObject, public KXMLGUIClient
{
    Q_OBJECT
public:
    DragomanPluginView(DragomanPlugin *plugin, KTextEditor::MainWindow *mainWindow);
    ~DragomanPluginView() override;

private:
    void translateSelection();
    void chooseAndTranslate();
    void showChooser(const QList<Dragoman::LanguagePair> &pairs);
    void swapDirection();
    void runTranslation(KTextEditor::View *view, const QString &source, const QString &target);

    [[nodiscard]] Dragoman::LanguagePair savedPair() const;
    void setSavedPair(const QString &source, const QString &target);
    void setBusy(bool busy);
    void notify(const QString &richText, KTextEditor::Message::MessageType type, int autoHideMs = 4000);
    void setProgressNote(const QString &richText);
    void clearProgressNote();

    KTextEditor::MainWindow *const m_mainWindow;
    Dragoman::Client *const m_client;
    QList<QAction *> m_actions;
    QPointer<KTextEditor::Message> m_progressNote;
    bool m_busy = false;
};
