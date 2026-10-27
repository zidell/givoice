#include "effect/effect.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "input_event.h"
#include "keyboard_input.h"
#include "workspace.h"
#include "window.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QSet>
#include "../src/shortcut_presets.h"
#include <linux/input-event-codes.h>

namespace KWin {
static const QString app = QStringLiteral("com.videostew.givoice");
static const QString path = QStringLiteral("/com/videostew/givoice/Escape");
static const QString iface = QStringLiteral("com.videostew.givoice.Escape");

class GivoiceEscape final : public Effect, public InputEventFilter {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.videostew.givoice.Keyboard")
public:
    GivoiceEscape() : InputEventFilter(InputFilterOrder::ScreenEdge) {
        auto bus = QDBusConnection::sessionBus();
        bus.registerObject("/com/videostew/givoice/Keyboard", this, QDBusConnection::ExportAllSlots);
        bus.connect(app, path, iface, "StateChanged", this, SLOT(stateChanged(bool)));
        bus.connect(app, path, iface, "TriggerChanged", this, SLOT(triggerChanged(QString)));
        auto watcher = new QDBusServiceWatcher(app, bus,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &owner) {
                ++m_generation;
                m_active = false;
                ++m_triggerGeneration;
                m_trigger = nullptr;
                if (!owner.isEmpty()) refresh();
            });
        refresh();
        // GTK's skip-taskbar hint only reaches X11; apply it natively on KDE Wayland.
        auto hideTask = [](Window *window) {
            const auto name = window->resourceClass().toLower();
            if (name == QStringLiteral("com.videostew.givoice") || name == QStringLiteral("givoice"))
                window->setSkipTaskbar(true);
        };
        connect(workspace(), &Workspace::windowAdded, this, hideTask);
        for (auto window : workspace()->windows()) hideTask(window);
        input()->installInputEventFilter(this);
    }
    ~GivoiceEscape() override {
        QDBusConnection::sessionBus().unregisterObject("/com/videostew/givoice/Keyboard");
    }
    bool isActive() const override { return false; }
    bool keyboardKey(KeyboardKeyEvent *event) override {
        // A latched scan code owns repeats and release even if modifiers change
        // or Settings disables/replaces the trigger while the key is held.
        if (m_recordScan && event->nativeScanCode == m_recordScan) {
            if (event->state == KeyboardKeyState::Released) {
                m_recordScan = 0;
                if (m_recordNotified) record(false);
                m_recordNotified = false;
            }
            return true;
        }
        if (!effects->isScreenLocked() && m_trigger && !m_recordScan &&
            event->state == KeyboardKeyState::Pressed && matches(event)) {
            m_recordScan = event->nativeScanCode;
            input()->keyboard()->addFilteredKey(event->nativeScanCode);
            m_recordNotified = true;
            record(true);
            return true;
        }
        // Latch the actual scan code, because Escape can be remapped to Caps.
        // Retain repeats and release even after cancellation makes the app idle.
        if (m_consumed.contains(event->nativeScanCode)) {
            if (event->state == KeyboardKeyState::Released)
                m_consumed.remove(event->nativeScanCode);
            return true;
        }
        if (!m_active || effects->isScreenLocked() || event->key != Qt::Key_Escape ||
            event->state != KeyboardKeyState::Pressed)
            return false;
        m_consumed.insert(event->nativeScanCode);
        input()->keyboard()->addFilteredKey(event->nativeScanCode);
        m_active = false;
        ++m_generation;
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(app, path, iface, "Cancel"), 1000);
        return true;
    }
public Q_SLOTS:
    QStringList SupportedTriggers() const {
        QStringList triggers;
        for (const auto &preset : shortcut_presets)
            if (!preset.modifiers) triggers << QString::fromUtf8(preset.trigger);
        return triggers;
    }
private Q_SLOTS:
    void triggerChanged(const QString &trigger) {
        ++m_triggerGeneration;
        // Finish delivery to the old binding before enabling a new one.
        if (m_recordNotified) record(false);
        m_recordNotified = false;
        auto name = trigger.toUtf8();
        m_trigger = shortcut_preset(name.constData());
        if (m_trigger && m_trigger->modifiers) m_trigger = nullptr;
    }
    void stateChanged(bool active) {
        ++m_generation;
        m_active = active;
    }
private:
    bool matches(KeyboardKeyEvent *event) const {
        unsigned mods = 0;
        auto relevant = event->modifiersRelevantForGlobalShortcuts;
        if (relevant & Qt::ControlModifier) mods |= 1;
        if (relevant & Qt::AltModifier) mods |= 2;
        if (relevant & Qt::ShiftModifier) mods |= 4;
        if (relevant & Qt::MetaModifier) mods |= 8;
        // Standalone modifiers include their own modifier bit in the event.
        if (!m_trigger->keysym) {
            mods &= ~(m_trigger->scan_code == KEY_RIGHTALT ? 2u : 1u);
            return event->nativeScanCode == m_trigger->scan_code && mods == 0;
        }
        return event->nativeVirtualKey == m_trigger->keysym && mods == m_trigger->modifiers;
    }
    void record(bool down) {
        auto message = QDBusMessage::createMethodCall(app, path, iface, "Record");
        message << down;
        QDBusConnection::sessionBus().asyncCall(message, 1000);
    }
    void refresh() {
        const auto triggerGeneration = ++m_triggerGeneration;
        auto trigger = QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(app, path, iface, "GetTrigger"), 1000);
        auto triggerWatcher = new QDBusPendingCallWatcher(trigger, this);
        connect(triggerWatcher, &QDBusPendingCallWatcher::finished, this,
            [this, triggerGeneration](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<QString> reply = *watcher;
                if (triggerGeneration == m_triggerGeneration && !reply.isError()) triggerChanged(reply.value());
                watcher->deleteLater();
            });
        const auto generation = ++m_generation;
        auto call = QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(app, path, iface, "GetState"), 1000);
        auto watcher = new QDBusPendingCallWatcher(call, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<bool> reply = *watcher;
                if (generation == m_generation)
                    m_active = !reply.isError() && reply.value();
                watcher->deleteLater();
            });
    }
    const ShortcutPreset *m_trigger = nullptr;
    uint32_t m_recordScan = 0;
    bool m_recordNotified = false;
    unsigned m_triggerGeneration = 0;
    bool m_active = false;
    unsigned m_generation = 0;
    QSet<uint32_t> m_consumed;
};
}
KWIN_EFFECT_FACTORY(KWin::GivoiceEscape, "metadata.json")
#include "escape.moc"
