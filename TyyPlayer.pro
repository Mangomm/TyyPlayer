QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += TYY_PLAYER_STATIC

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    src/player/tyy_ffmpeg_d3d11va.cpp \
    src/player/tyy_ffmpeg_hw.cpp \
    src/player/tyy_ffplay_core.cpp \
    src/player/tyy_log.cpp \
    src/player/tyy_player.cpp \
    src/player/tyy_player_api.cpp \
    src/player/tyy_properties.cpp \
    src/player/tyy_sonic.cpp \
    src/player/tyy_video_state.cpp \
    ui/media_info_dialog.cpp \
    ui/preview_widget.cpp \
    ui/video_frame_extractor.cpp

HEADERS += \
    mainwindow.h \
    src/player/tyy_ffmpeg_d3d11va.h \
    src/player/tyy_ffmpeg_hw.h \
    src/player/tyy_ffplay_core.h \
    src/player/tyy_log.h \
    src/player/tyy_player.h \
    src/player/tyy_player_api.h \
    src/player/tyy_properties.h \
    src/player/tyy_sonic.h \
    src/player/tyy_video_state.h \
    ui/media_info_dialog.h \
    ui/preview_widget.h \
    ui/video_frame_extractor.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    ui/qss/theme.qrc \
    ui/icons/app_icon.qrc

INCLUDEPATH += $$PWD/src/player
INCLUDEPATH += $$PWD/ui

win32 {
    RC_ICONS += ui/icons/app_icon.ico

    FFMPEG_ROOT = E:/me-lessons/code_test/ffplay_study/3rdlib/ffmepg-4.4-study
    SDL_ROOT = E:/me-lessons/code_test/ffplay_study/3rdlib/libsdl2
    INCLUDEPATH += $$FFMPEG_ROOT/include
    INCLUDEPATH += $$SDL_ROOT/include
    FFMPEG_RUNTIME_DLLS = \
        $$FFMPEG_ROOT/bin/avcodec-58.dll \
        $$FFMPEG_ROOT/bin/avdevice-58.dll \
        $$FFMPEG_ROOT/bin/avfilter-7.dll \
        $$FFMPEG_ROOT/bin/avformat-58.dll \
        $$FFMPEG_ROOT/bin/avutil-56.dll \
        $$FFMPEG_ROOT/bin/postproc-55.dll \
        $$FFMPEG_ROOT/bin/swresample-3.dll \
        $$FFMPEG_ROOT/bin/swscale-5.dll \
        $$FFMPEG_ROOT/bin/fdk-aac.dll \
        $$FFMPEG_ROOT/bin/libmp3lame.dll \
        $$FFMPEG_ROOT/bin/libx264.dll \
        $$FFMPEG_ROOT/bin/x265.dll \
        $$FFMPEG_ROOT/bin/SDL2.dll
    CONFIG(debug, debug|release):RUNTIME_DEST_DIR = $$OUT_PWD/debug
    CONFIG(release, debug|release):RUNTIME_DEST_DIR = $$OUT_PWD/release
    for(DLL_FILE, FFMPEG_RUNTIME_DLLS) {
        QMAKE_POST_LINK += cmd /c copy /Y $$system_path($$DLL_FILE) $$system_path($$RUNTIME_DEST_DIR) $$escape_expand(\\n\\t)
    }
    LIBS += -L$$FFMPEG_ROOT/lib \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lavcodec \
        -lpostproc \
        -lavutil \
        -lswscale \
        -lswresample \
        -lshell32 \
        -lole32 \
        -lwinmm \
        -ld3d11 \
        -ldxgi \
        -ldxguid
    LIBS += -L$$SDL_ROOT/lib \
        -lSDL2
}

unix:!macx:!android {
    CONFIG += link_pkgconfig
    PKGCONFIG += libavformat libavcodec libavutil libswscale libswresample
}

macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += libavformat libavcodec libavutil libswscale libswresample
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
