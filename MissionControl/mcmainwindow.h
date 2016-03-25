#ifndef ARMMAINWINDOW_H
#define ARMMAINWINDOW_H

#include <QMainWindow>
#include <QTimerEvent>
#include <QKeyEvent>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QtWebEngineWidgets>
#include <QDebug>
#include <QFile>
#include <QSerialPort>
#include <QSerialPortInfo>

#include "channel.h"
#include "logger.h"
#include "iniparser.h"
#include "mcutil.h"
#include "soroutil.h"
#include "armmessage.h"
#include "serialinterop.h"
#include "armglfwmap.h"
#include "glfwmapdialog.h"
#include "videowindow.h"
#include "latlng.h"
#include "commonini.h"

#include "GLFW/glfw3.h"

#define NO_CONTROLLER -1

namespace Ui {
    class McMainWindow;
}

namespace Soro {
namespace MissionControl {

    class McMainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit McMainWindow(QWidget *parent = 0);
        ~McMainWindow();
        enum InputMode {
            GLFW, MasterArm
        };
        
        enum LayoutMode {
            ArmLayoutMode, DriveLayoutMode, GimbalLayoutMode, SpectatorLayoutMode
        };

    private:
        Ui::McMainWindow *ui;
        int _initTimerId = TIMER_INACTIVE;
        VideoWindow *_videoWindow = NULL;
        bool _fullscreen = false;
        Logger *_log = NULL;
        char _currentKey = '\0';
        InputMode _inputMode = GLFW;
        LayoutMode _mode = SpectatorLayoutMode;
        char _buffer[512];
        bool _glfwInitialized = false;

        //ports for video stream
        quint16 _armVideoPort, _driveVideoPort, _gimbalVideoPort;

        //internet communication channels
        Channel *_controlChannel = NULL;
        Channel *_sharedChannel = NULL;

        //for joystick control
        ArmGlfwMap _controlMap;
        //TODO drive and gimbal
        int _controllerId = NO_CONTROLLER;
        int _controlSendTimerId = TIMER_INACTIVE;

        //Arm specific stuff
        SerialChannel *_masterArmSerial;
        ArmMessage::MasterRanges _masterArmRanges;
        void loadMasterArmConfig();

    private slots:
        void sharedChannelMessageReceived(const QByteArray& message);
        void sharedChannelStateChanged(Channel::State state);
        void sharedChannelStatsUpdate(int rtt, quint64 messagesUp, quint64 messagesDown,
                                int rateUp, int rateDown);
        void controlChannelStateChanged(Channel::State state);
        void controlChannelStatsUpdate(int rtt, quint64 messagesUp, quint64 messagesDown,
                                int rateUp, int rateDown);
        void masterArmSerialMessageReceived(const char *message, int size);
        void masterArmSerialStateChanged(SerialChannel::State);
        void settingsClicked();

    protected:
        void timerEvent(QTimerEvent *e);
        void keyPressEvent(QKeyEvent *e);
        void keyReleaseEvent(QKeyEvent *e);
        void resizeEvent(QResizeEvent* event);
    };

}
}

#endif // ARMMAINWINDOW_H
