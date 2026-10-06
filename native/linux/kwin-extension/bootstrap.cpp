#include <QQmlExtensionPlugin>
#include <qqml.h>
#include <QCoreApplication>
#include <QTimer>
#include <QDBusInterface>
class GivoiceBootstrap final : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override {
        qmlRegisterModule(uri, 1, 0);
        QCoreApplication::addLibraryPath(QString::fromUtf8(GIVOICE_KWIN_PLUGIN_DIR));
        QTimer::singleShot(0, QCoreApplication::instance(), [] {
            QDBusInterface effects("org.kde.KWin", "/Effects", "org.kde.kwin.Effects");
            effects.asyncCall("loadEffect", "givoice-escape");
        });
    }
};
#include "bootstrap.moc"
