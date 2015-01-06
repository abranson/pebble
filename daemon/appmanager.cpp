#include <QStandardPaths>
#include <QDir>
#include "appmanager.h"

AppManager::AppManager(QObject *parent)
    : QObject(parent), l(metaObject()->className()),
      _watcher(new QFileSystemWatcher(this))
{
    connect(_watcher, &QFileSystemWatcher::directoryChanged,
            this, &AppManager::rescan);

    QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    if (!dataDir.mkpath("apps")) {
        qCWarning(l) << "could not create apps dir" << dataDir.absoluteFilePath("apps");
    }
    qCDebug(l) << "install apps in" << dataDir.absoluteFilePath("apps");

    rescan();
}

QStringList AppManager::appPaths() const
{
    return QStandardPaths::locateAll(QStandardPaths::DataLocation,
                                     QLatin1String("apps"),
                                     QStandardPaths::LocateDirectory);
}

QList<QUuid> AppManager::appUuids() const
{
    return _apps.keys();
}

AppInfo AppManager::info(const QUuid &uuid) const
{
    return _apps.value(uuid);
}

AppInfo AppManager::info(const QString &name) const
{
    QUuid uuid = _names.value(name);
    if (!uuid.isNull()) {
        return info(uuid);
    } else {
        return AppInfo();
    }
}

void AppManager::rescan()
{
    QStringList watchedDirs = _watcher->directories();
    if (!watchedDirs.isEmpty()) _watcher->removePaths(watchedDirs);
    QStringList watchedFiles = _watcher->files();
    if (!watchedFiles.isEmpty()) _watcher->removePaths(watchedFiles);
    _apps.clear();
    _names.clear();

    Q_FOREACH(const QString &path, appPaths()) {
        QDir dir(path);
        _watcher->addPath(dir.absolutePath());
        qCDebug(l) << "scanning dir" << dir.absolutePath();
        QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable | QDir::Executable);
        qCDebug(l) << "scanning dir results" << entries;
        Q_FOREACH(const QString &path, entries) {
            QString appPath = dir.absoluteFilePath(path);
            _watcher->addPath(appPath);
            if (dir.exists(path + "/appinfo.json")) {
                _watcher->addPath(appPath + "/appinfo.json");
                scanApp(appPath);
            }
        }
    }

    qCDebug(l) << "now watching" << _watcher->directories() << _watcher->files();
    emit appsChanged();
}

void AppManager::insertAppInfo(const AppInfo &info)
{
    _apps.insert(info.uuid(), info);
    _names.insert(info.shortName(), info.uuid());

    const char *type = info.isWatchface() ? "watchface" : "app";
    const char *local = info.isLocal() ? "local" : "watch";
    qCDebug(l) << "found" << local << type << info.shortName() << info.versionCode() << "/" << info.versionLabel() << "with uuid" << info.uuid().toString();
}

void AppManager::scanApp(const QString &path)
{
    qCDebug(l) << "scanning app" << path;
    const AppInfo &info = AppInfo::fromPath(path);
    if (info.isLocal()) insertAppInfo(info);
}
