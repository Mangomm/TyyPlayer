#include "mainwindow.h"

#include "src/player/tyy_log.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

static void apply_windows_app_user_model_id()
{
#ifdef Q_OS_WIN
    typedef HRESULT (WINAPI *SetCurrentProcessExplicitAppUserModelIDProc)(PCWSTR);

    HMODULE shell_module = LoadLibraryW(L"shell32.dll");
    if (shell_module == nullptr)
    {
        return;
    }

    SetCurrentProcessExplicitAppUserModelIDProc set_app_id =
        reinterpret_cast<SetCurrentProcessExplicitAppUserModelIDProc>(
            GetProcAddress(shell_module, "SetCurrentProcessExplicitAppUserModelID"));
    if (set_app_id != nullptr)
    {
        set_app_id(L"TyyPlayer.TyyPlayer");
    }

    FreeLibrary(shell_module);
#endif
}

int main(int argc, char *argv[])
{
    apply_windows_app_user_model_id();

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/app_icon.png"));

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
    w.setWindowIcon(QIcon(":/icons/app_icon.png"));
    w.show();
    return a.exec();
}
