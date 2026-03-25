QT += quick qml multimedia widgets serialbus
# QT += virtualkeyboard
CONFIG += c++11 debug
PKGCONFIG += gstreamer-1.0

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Refer to the documentation for the
# deprecated API to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += link_pkgconfig debug

SOURCES += \
        main.cpp \
        QrCodeReader.cpp \
        CanHandler.cpp \
        quirc/quirc.c \
        quirc/decode.c \
        quirc/identify.c \
        quirc/version_db.c

HEADERS += \
        QrCodeReader.h \
        CanHandler.h \
        quirc/quirc.h \
        quirc/quirc_internal.h

PKGCONFIG += libhal

RESOURCES += qml.qrc translations.qrc

TRANSLATIONS += translations/fr.ts

# Compile .ts to .qm before rcc processes translations.qrc
isEmpty(QMAKE_LRELEASE):QMAKE_LRELEASE = $$dirname(QMAKE_QMAKE)/lrelease

updateqm.input = TRANSLATIONS
updateqm.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /data/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
