#include "mainwindow.h"

#include "src/player/tyy_log.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString log_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (log_path.isEmpty())
    {
        log_path = QDir::homePath() + "/AppData/Local/TyyPlayer";
    }
    QDir().mkpath(log_path);

    TyyPlayer::TyyLog::get_instance()->set_log_path(log_path.toStdString());
    TyyPlayer::TyyLog::get_instance()->set_log_level(TyyPlayer::TYY_LOG_LEVEL_TRACE);
    TyyPlayer::TyyLog::get_instance()->exec_log(TyyPlayer::DAILY_LOGGER);
    TYYINFO("TyyPlayer start, log path: {}", log_path.toStdString());

    MainWindow w;
    w.show();
    return a.exec();
}
