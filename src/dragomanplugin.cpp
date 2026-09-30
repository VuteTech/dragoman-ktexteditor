/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dragomanplugin.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>
#include <KXMLGUIFactory>

#include <QAction>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>

#include <algorithm>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(DragomanPlugin, "dragomanplugin.json")

namespace
{

// Mirrors the daemon's per-request limits.
constexpr qsizetype maxSegments = 256;
constexpr qsizetype maxTotalBytes = 1 << 20;

const auto configGroup = u"Dragoman"_s;

[[nodiscard]] QIcon pluginIcon()
{
    return QIcon::fromTheme(u"dev.l10n_bg.dragomand"_s, QIcon::fromTheme(u"applications-education-language"_s));
}

} // namespace

DragomanPlugin::DragomanPlugin(QObject *parent, const QVariantList &args)
    : KTextEditor::Plugin(parent)
{
    Q_UNUSED(args)
}

QObject *DragomanPlugin::createView(KTextEditor::MainWindow *mainWindow)
{
    return new DragomanPluginView(this, mainWindow);
}

DragomanPluginView::DragomanPluginView(DragomanPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_mainWindow(mainWindow)
    , m_client(new DragomanClient(this))
{
    KXMLGUIClient::setComponentName(u"dragoman"_s, i18n("Dragoman"));
    setXMLFile(u"ui.rc"_s);

    auto *translate = actionCollection()->addAction(u"dragoman_translate"_s);
    translate->setText(i18n("&Translate Selection"));
    translate->setIcon(pluginIcon());
    KActionCollection::setDefaultShortcut(translate, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
    connect(translate, &QAction::triggered, this, &DragomanPluginView::translateSelection);

    auto *choose = actionCollection()->addAction(u"dragoman_choose"_s);
    choose->setText(i18n("Translate Selection (Choose &Languages)…"));
    connect(choose, &QAction::triggered, this, &DragomanPluginView::chooseAndTranslate);

    auto *swap = actionCollection()->addAction(u"dragoman_swap"_s);
    swap->setText(i18n("&Swap Translation Direction"));
    connect(swap, &QAction::triggered, this, &DragomanPluginView::swapDirection);

    m_actions = {translate, choose, swap};

    connect(m_client, &DragomanClient::progress, this, [this](double fraction, const QString &stage) {
        setProgressNote(i18n("Preparing the translation model: %1 (%2%)", stage.toHtmlEscaped(), qRound(fraction * 100)));
    });

    m_mainWindow->guiFactory()->addClient(this);
}

DragomanPluginView::~DragomanPluginView()
{
    clearProgressNote();
    m_mainWindow->guiFactory()->removeClient(this);
}

Dragoman::LanguagePair DragomanPluginView::savedPair() const
{
    const KConfigGroup group(KSharedConfig::openConfig(), configGroup);
    return {group.readEntry("Source", u"bg"_s), group.readEntry("Target", u"en"_s)};
}

void DragomanPluginView::setSavedPair(const QString &source, const QString &target)
{
    KConfigGroup group(KSharedConfig::openConfig(), configGroup);
    group.writeEntry("Source", source);
    group.writeEntry("Target", target);
    group.sync();
}

void DragomanPluginView::swapDirection()
{
    const auto [source, target] = savedPair();
    setSavedPair(target, source);
    notify(i18n("Now translating from %1 to %2.", target, source), KTextEditor::Message::Information);
}

void DragomanPluginView::translateSelection()
{
    auto *view = m_mainWindow->activeView();
    if (!view || !view->selection()) {
        notify(i18n("Select the text to translate first."), KTextEditor::Message::Warning);
        return;
    }
    const QString text = view->document()->text(view->selectionRange());
    const auto [source, target] = Dragoman::chooseDirection(savedPair(), text);
    runTranslation(view, source, target);
}

void DragomanPluginView::chooseAndTranslate()
{
    if (m_busy) {
        return;
    }
    m_client->listPairs([this](const DragomanClient::Pairs &pairs, const QString &error) {
        if (pairs.isEmpty()) {
            notify(error.isEmpty() ? i18n("The translation daemon reports no language pairs.") : error.toHtmlEscaped(), KTextEditor::Message::Error);
            return;
        }
        showChooser(pairs);
    });
}

void DragomanPluginView::showChooser(const DragomanClient::Pairs &pairs)
{
    QStringList sources;
    for (const auto &[source, target] : pairs) {
        if (!sources.contains(source)) {
            sources.append(source);
        }
    }
    sources.sort();

    auto *dialog = new QDialog(m_mainWindow->window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Translation Languages"));
    auto *sourceBox = new QComboBox(dialog);
    auto *targetBox = new QComboBox(dialog);
    for (const QString &code : std::as_const(sources)) {
        sourceBox->addItem(Dragoman::languageLabel(code), code);
    }
    const auto fillTargets = [pairs, targetBox](const QString &source, const QString &preferred) {
        targetBox->clear();
        QStringList targets;
        for (const auto &[from, to] : pairs) {
            if (from == source && !targets.contains(to)) {
                targets.append(to);
            }
        }
        targets.sort();
        for (const QString &code : std::as_const(targets)) {
            targetBox->addItem(Dragoman::languageLabel(code), code);
        }
        targetBox->setCurrentIndex(std::max(0, targetBox->findData(preferred)));
    };
    const auto [savedSource, savedTarget] = savedPair();
    sourceBox->setCurrentIndex(std::max(0, sourceBox->findData(savedSource)));
    fillTargets(sourceBox->currentData().toString(), savedTarget);
    connect(sourceBox, &QComboBox::currentIndexChanged, dialog, [sourceBox, fillTargets, savedTarget] {
        fillTargets(sourceBox->currentData().toString(), savedTarget);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto *layout = new QFormLayout(dialog);
    layout->addRow(i18nc("@label:listbox source language", "From:"), sourceBox);
    layout->addRow(i18nc("@label:listbox target language", "To:"), targetBox);
    layout->addRow(buttons);

    connect(dialog, &QDialog::accepted, this, [this, sourceBox, targetBox] {
        const QString source = sourceBox->currentData().toString();
        const QString target = targetBox->currentData().toString();
        setSavedPair(source, target);
        auto *view = m_mainWindow->activeView();
        if (!view || !view->selection()) {
            notify(i18n("Saved: translating from %1 to %2. Select text and translate.", source, target), KTextEditor::Message::Information);
            return;
        }
        runTranslation(view, source, target);
    });
    dialog->open();
}

void DragomanPluginView::runTranslation(KTextEditor::View *view, const QString &source, const QString &target)
{
    if (m_busy) {
        return;
    }
    if (view->blockSelection()) {
        notify(i18n("Block selections cannot be translated."), KTextEditor::Message::Warning);
        return;
    }

    const KTextEditor::Range range = view->selectionRange();
    const QString original = view->document()->text(range);
    const QStringList lines = original.split(u'\n');
    QStringList segments;
    QList<qsizetype> where;
    qsizetype totalBytes = 0;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (!lines.at(i).trimmed().isEmpty()) {
            segments.append(lines.at(i));
            where.append(i);
            totalBytes += lines.at(i).toUtf8().size();
        }
    }
    if (segments.isEmpty()) {
        notify(i18n("The selection contains no text."), KTextEditor::Message::Warning);
        return;
    }
    if (segments.size() > maxSegments || totalBytes > maxTotalBytes) {
        notify(i18n("The selection is too large for one request (at most %1 lines and 1 MiB).", maxSegments), KTextEditor::Message::Warning);
        return;
    }

    setBusy(true);
    setProgressNote(i18n("Translating from %1 to %2…", source, target));
    QPointer<KTextEditor::Document> document(view->document());
    m_client->translate(source, target, segments, [this, document, range, original, lines, where, source, target](const DragomanReply &reply) {
        setBusy(false);
        clearProgressNote();
        if (!reply.ok) {
            notify(i18n("Translation failed: %1", reply.error.toHtmlEscaped()), KTextEditor::Message::Error, 8000);
            return;
        }
        QStringList merged = lines;
        for (qsizetype k = 0; k < where.size(); ++k) {
            merged[where.at(k)] = reply.translations.value(k);
        }
        const QString translated = merged.join(u'\n');
        if (!document || document->text(range) != original) {
            QGuiApplication::clipboard()->setText(translated);
            notify(i18n("The text changed while translating; the result is on the clipboard."), KTextEditor::Message::Warning, 8000);
            return;
        }
        document->replaceText(range, translated);
        if (!reply.pivot.isEmpty()) {
            notify(i18n("Translated from %1 to %2 through %3.", source, target, reply.pivot.toHtmlEscaped()), KTextEditor::Message::Information);
        }
    });
}

void DragomanPluginView::setBusy(bool busy)
{
    m_busy = busy;
    for (auto *action : std::as_const(m_actions)) {
        action->setEnabled(!busy);
    }
}

void DragomanPluginView::notify(const QString &richText, KTextEditor::Message::MessageType type, int autoHideMs)
{
    auto *view = m_mainWindow->activeView();
    if (!view) {
        return;
    }
    auto *message = new KTextEditor::Message(richText, type);
    message->setIcon(pluginIcon());
    message->setAutoHide(autoHideMs);
    message->setAutoHideMode(KTextEditor::Message::Immediate);
    view->document()->postMessage(message);
}

void DragomanPluginView::setProgressNote(const QString &richText)
{
    if (m_progressNote) {
        m_progressNote->setText(richText);
        return;
    }
    auto *view = m_mainWindow->activeView();
    if (!view) {
        return;
    }
    auto *message = new KTextEditor::Message(richText, KTextEditor::Message::Information);
    message->setIcon(pluginIcon());
    message->setAutoHide(-1);
    m_progressNote = message;
    view->document()->postMessage(message);
}

void DragomanPluginView::clearProgressNote()
{
    delete m_progressNote;
}

#include "dragomanplugin.moc"
