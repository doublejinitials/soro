/*
 * Copyright 2017 The University of Oklahoma.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "researchprocess.h"

#define LOG_TAG "Research Control"

#define DEFAULT_VIDEO_STEREO_MODE VideoFormat::StereoMode_SideBySide

namespace Soro {
namespace MissionControl {

ResearchControlProcess::ResearchControlProcess(QHostAddress roverAddress, GamepadManager *gamepad, QQmlEngine *qml, QObject *parent) : QObject(parent) {
    _gamepad = gamepad;
    _settings = SettingsModel::Default(roverAddress);
    _qml = qml;

    // Must initialize after event loop starts
    QTimer::singleShot(1, this, &ResearchControlProcess::init);
}

void ResearchControlProcess::init() {
    LOG_I(LOG_TAG, "-------------------------------------------------------");
    LOG_I(LOG_TAG, "-------------------------------------------------------");
    LOG_I(LOG_TAG, "-------------------------------------------------------");
    LOG_I(LOG_TAG, "Starting research control process...");
    LOG_I(LOG_TAG, "-------------------------------------------------------");
    LOG_I(LOG_TAG, "-------------------------------------------------------");
    LOG_I(LOG_TAG, "-------------------------------------------------------");

    connect(_gamepad, &GamepadManager::gamepadChanged, this, &ResearchControlProcess::gamepadChanged);

    LOG_I(LOG_TAG, "****************Initializing connections*******************");

    LOG_I(LOG_TAG, "Setting up rover shared connection");
    // Create the main shared channel to connect to the rover
    _roverChannel = Channel::createClient(this, SocketAddress(_settings.roverAddress, NETWORK_ALL_SHARED_CHANNEL_PORT), CHANNEL_NAME_SHARED,
            Channel::TcpProtocol, QHostAddress::Any);
    _roverChannel->open();
    connect(_roverChannel, &Channel::messageReceived, this, &ResearchControlProcess::roverSharedChannelMessageReceived);
    connect(_roverChannel, &Channel::stateChanged, this, &ResearchControlProcess::updateUiConnectionState);

    LOG_I(LOG_TAG, "Creating drive control system");
    _driveSystem = new DriveControlSystem(_settings.roverAddress, _gamepad, this);
    _driveSystem->setMode(DriveGamepadMode::SingleStickDrive);
    connect(_driveSystem, &DriveControlSystem::connectionStateChanged, this, &ResearchControlProcess::driveConnectionStateChanged);
    QString err;
    if (!_driveSystem->init(&err)) {
        LOG_E(LOG_TAG, "Drive system failed to init: " + err);
        QCoreApplication::exit(1);
    }
    _driveSystem->enable();

    LOG_I(LOG_TAG, "***************Initializing Video system******************");

    _stereoLVideoClient = new VideoClient(MEDIAID_RESEARCH_SL_CAMERA, SocketAddress(_settings.roverAddress, NETWORK_ALL_RESEARCH_SL_CAMERA_PORT), QHostAddress::Any, this);
    _stereoRVideoClient = new VideoClient(MEDIAID_RESEARCH_SR_CAMERA, SocketAddress(_settings.roverAddress, NETWORK_ALL_RESEARCH_SR_CAMERA_PORT), QHostAddress::Any, this);
    _aux1VideoClient = new VideoClient(MEDIAID_RESEARCH_A1_CAMERA, SocketAddress(_settings.roverAddress, NETWORK_ALL_RESEARCH_A1L_CAMERA_PORT), QHostAddress::Any, this);
    _monoVideoClient = new VideoClient(MEDIAID_RESEARCH_M_CAMERA, SocketAddress(_settings.roverAddress, NETWORK_ALL_RESEARCH_ML_CAMERA_PORT), QHostAddress::Any, this);

    connect(_stereoLVideoClient, &VideoClient::stateChanged, this, &ResearchControlProcess::videoClientStateChanged);
    connect(_stereoRVideoClient, &VideoClient::stateChanged, this, &ResearchControlProcess::videoClientStateChanged);
    connect(_aux1VideoClient, &VideoClient::stateChanged, this, &ResearchControlProcess::videoClientStateChanged);
    connect(_monoVideoClient, &VideoClient::stateChanged, this, &ResearchControlProcess::videoClientStateChanged);

    // add localhost bounce to the video stream so the in-app player can display it from a udpsrc
    _stereoLVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SL_CAMERA_PORT));
    _stereoRVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SR_CAMERA_PORT));
    _aux1VideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1L_CAMERA_PORT));
    _aux1VideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1R_CAMERA_PORT));
    _monoVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_ML_CAMERA_PORT));
    _monoVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_MR_CAMERA_PORT));

    // add localhost bounce to the video stream so they can be recorded to file
    _stereoLVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SL_CAMERA_PORT_R));
    _stereoRVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SR_CAMERA_PORT_R));
    _aux1VideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1_CAMERA_PORT_R));
    _monoVideoClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_M_CAMERA_PORT_R));

    // Create file recorders
    _stereoLGStreamerRecorder = new GStreamerRecorder(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SL_CAMERA_PORT_R), "StereoLeft", this);
    _stereoRGStreamerRecorder = new GStreamerRecorder(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SR_CAMERA_PORT_R), "StereoRight", this);
    _aux1GStreamerRecorder = new GStreamerRecorder(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1_CAMERA_PORT_R), "Aux1", this);
    _monoGStreamerRecorder = new GStreamerRecorder(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_M_CAMERA_PORT_R), "Mono", this);

    LOG_I(LOG_TAG, "***************Initializing Audio system******************");

    _audioClient = new AudioClient(MEDIAID_AUDIO, SocketAddress(_settings.roverAddress, NETWORK_ALL_AUDIO_PORT), QHostAddress::Any, this);
    // forward audio stream through localhost
    _audioClient->addForwardingAddress(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_AUDIO_PORT));
    connect(_audioClient, &AudioClient::stateChanged, this, &ResearchControlProcess::audioClientStateChanged);

    _audioPlayer = new Soro::Gst::AudioPlayer(this);
    _audioGStreamerRecorder = new GStreamerRecorder(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_AUDIO_PORT), "Audio", this);

    LOG_I(LOG_TAG, "***************Initializing Data Recording system******************");

    _sensorDataSeries = new SensorDataParser(this);
    _gpsDataSeries = new GpsCsvSeries(this);
    _connectionEventSeries = new ConnectionEventCsvSeries(_driveSystem->getChannel(), _roverChannel, this);
    _latencyDataSeries = new LatencyCsvSeries(this);
    _commentDataSeries = new CommentCsvSeries(this);

    _dataRecorder = new CsvRecorder(this);
    _dataRecorder->setUpdateInterval(50);

    _dataRecorder->addColumn(_sensorDataSeries->getWheelPowerASeries());
    _dataRecorder->addColumn(_sensorDataSeries->getWheelPowerBSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getWheelPowerCSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getWheelPowerDSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getWheelPowerESeries());
    _dataRecorder->addColumn(_sensorDataSeries->getWheelPowerFSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getImuRearYawSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getImuRearPitchSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getImuRearRollSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getImuFrontYawSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getImuFrontPitchSeries());
    _dataRecorder->addColumn(_sensorDataSeries->getImuFrontRollSeries());
    _dataRecorder->addColumn(_gpsDataSeries->getLatitudeSeries());
    _dataRecorder->addColumn(_gpsDataSeries->getLongitudeSeries());
    _dataRecorder->addColumn(_connectionEventSeries);
    _dataRecorder->addColumn(_latencyDataSeries->getRealLatencySeries());
    _dataRecorder->addColumn(_latencyDataSeries->getSimulatedLatencySeries());
    _dataRecorder->addColumn(_commentDataSeries);

    LOG_I(LOG_TAG, "***************Initializing UI******************");

    // Create UI for rover control
    _mainUi = new ResearchMainWindow(_gamepad, _driveSystem);
    _mainUi->getCameraWidget()->setStereoMode(VideoFormat::StereoMode_SideBySide);

    // Create UI for settings and control
    QQmlComponent qmlComponent(_qml, QUrl("qrc:/Main.qml"));
    _controlUi = qobject_cast<QQuickWindow*>(qmlComponent.create());
    if (!qmlComponent.errorString().isEmpty() || !_controlUi) {
        LOG_E(LOG_TAG, "Cannot create main QML: " + qmlComponent.errorString());
        QCoreApplication::exit(1);
        return;
    }

    // Create UI for comments
    QQmlComponent qmlComponent2(_qml, QUrl("qrc:/Comments.qml"));
    _commentsUi = qobject_cast<QQuickWindow*>(qmlComponent2.create());
    if (!qmlComponent2.errorString().isEmpty() || !_commentsUi) {
        LOG_E(LOG_TAG, "Cannot create comments QML: " + qmlComponent2.errorString());
        QCoreApplication::exit(1);
        return;
    }

    // Perform initial setup of control UI

    connect(_controlUi, SIGNAL(requestUiSync()), this, SLOT(ui_requestUiSync()));
    connect(_controlUi, SIGNAL(settingsApplied()), this, SLOT(ui_settingsApplied()));
    connect (_controlUi, SIGNAL(recordButtonClicked()), this, SLOT(ui_toggleDataRecordButtonClicked()));
    connect(_controlUi, SIGNAL(zeroOrientationButtonClicked()), _mainUi, SLOT(zeroHudOrientation()));
    connect (_commentsUi, SIGNAL(recordButtonClicked()), this, SLOT(ui_toggleDataRecordButtonClicked()));

    connect(_sensorDataSeries, &SensorDataParser::dataParsed, _mainUi, &ResearchMainWindow::sensorUpdate);
    connect(_commentsUi, SIGNAL(logCommentEntered(QString)), _commentDataSeries, SLOT(addComment(QString)));
    connect(_controlUi, SIGNAL(closed()), this, SLOT(onQmlUiClosed()));
    connect(_commentsUi, SIGNAL(closed()), this, SLOT(onQmlUiClosed()));

    // Show UI's

    _mainUi->show();
    _controlUi->setVisible(true);
    _commentsUi->setVisible(true);

    // Start timers

    START_TIMER(_bitrateUpdateTimerId, 1000);
    START_TIMER(_pingTimerId, 1000);
}

void ResearchControlProcess::roverDataRecordResponseWatchdog() {
    if (!_dataRecorder->isRecording()) {
        // Rover did not respond to our record request in time
        stopDataRecording();
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant,"error"),
                                  Q_ARG(QVariant, "Cannot Record Data"),
                                  Q_ARG(QVariant, "The rover has not responded to the request to start data recording"));
    }
}

void ResearchControlProcess::startDataRecording() {
    _recordStartTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    sendStartRecordCommandToRover();
    _controlUi->setProperty("recordingState", "waiting");
    _commentsUi->setProperty("recordingState", "waiting");
    QTimer::singleShot(5000, this, SLOT(roverDataRecordResponseWatchdog()));
}

void ResearchControlProcess::stopDataRecording() {
    _dataRecorder->stopLog();
    _controlUi->setProperty("recordingState", "idle");
    _commentsUi->setProperty("recordingState", "idle");

    // Send stop command to rover as well
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    SharedMessageType messageType = SharedMessage_Research_StopDataRecording;
    stream << static_cast<qint32>(messageType);

    _roverChannel->sendMessage(byteArray);
}

void ResearchControlProcess::gamepadChanged(bool connected, QString name) {
    _controlUi->setProperty("gamepad", name);
    if (connected) {
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant,"information"),
                                  Q_ARG(QVariant, "Input Device Connected"),
                                  Q_ARG(QVariant, name + " is connected and ready for use."));
    }
    else {
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant,"warning"),
                                  Q_ARG(QVariant, "No Input Device"),
                                  Q_ARG(QVariant, "Driving will be unavailable until a compatible controller is connected."));
    }
}

void ResearchControlProcess::onQmlUiClosed() {
    QCoreApplication::quit();
}

void ResearchControlProcess::ui_requestUiSync() {
    // Sync state
    updateUiConnectionState();
    // Sync settings
    _settings.syncUi(_controlUi);
}

void ResearchControlProcess::ui_toggleDataRecordButtonClicked() {
    if (_dataRecorder->isRecording()) {
        stopDataRecording();
    }
    else {
        startDataRecording();
    }
}

void ResearchControlProcess::ui_settingsApplied() {
    _settings.syncModel(_controlUi);

    if (_settings.enableVideo) {
        VideoFormat format = _settings.getSelectedVideoFormat();
        if (format.isUseable()) {
            if (_settings.selectedCamera == _settings.mainCameraIndex) {
                if (_settings.enableStereoUi) {
                    if (_settings.enableStereoVideo) {
                        // Stream main camera in stereo
                        format.setStereoMode(DEFAULT_VIDEO_STEREO_MODE);
                        startStereoCameraStream(format);
                    }
                    else {
                        // Stream main camera in mono on stereo UI
                        format.setStereoMode(DEFAULT_VIDEO_STEREO_MODE);
                        startMonoCameraStream(format);
                    }
                }
                else {
                    _settings.enableStereoVideo = false; // Just in case the UI let them
                    // Stream main camera in mono on mono UI
                    format.setStereoMode(VideoFormat::StereoMode_None);
                    startMonoCameraStream(format);
                }
            }
            else if (_settings.selectedCamera == _settings.aux1CameraIndex) {
                _settings.enableStereoVideo = false;
                if (_settings.enableStereoUi) {
                    // Stream aux1 camera in mono on stereo UI
                    format.setStereoMode(DEFAULT_VIDEO_STEREO_MODE);
                    startAux1CameraStream(format);
                }
                else {
                    // Stream aux1 camera in mono on mono UI
                    format.setStereoMode(VideoFormat::StereoMode_None);
                    startAux1CameraStream(format);
                }
            }
            else {
                LOG_E(LOG_TAG, "Unknown camera index selected in UI");
                _settings.enableVideo = false;
                _settings.selectedCamera = 0;
            }
        }
        else {
            LOG_E(LOG_TAG, "Unknown video format index selected in UI");
            _settings.enableVideo = false;
            _settings.selectedCamera = 0;
        }
    }
    else {
        stopAllRoverCameras();
    }
    _mainUi->setHudVisible(_settings.enableHud);
    _mainUi->setHudParallax(_settings.selectedHudParallax);
    _mainUi->setHudLatency(_settings.selectedHudLatency);
    if (_settings.enableAudio) {
        startAudioStream(_settings.defaultAudioFormat);
    }
    else {
        stopAudio();
    }
    _driveSystem->getChannel()->setSimulatedDelay(_settings.selectedLatency);
    _latencyDataSeries->updateSimulatedLatency(_settings.selectedLatency);
}

void ResearchControlProcess::videoClientStateChanged(MediaClient *client, MediaClient::State state) {
    // Stop all file recordings
    _stereoLGStreamerRecorder->stop();
    _stereoRGStreamerRecorder->stop();
    _monoGStreamerRecorder->stop();
    _aux1GStreamerRecorder->stop();

    if ((client == _stereoLVideoClient) || (client == _stereoRVideoClient)) {
        if ((_stereoLVideoClient->getState() == MediaClient::StreamingState) &&
                (_stereoRVideoClient->getState() == MediaClient::StreamingState)) {
            // Stereo L/R cameras are streaming

            VideoFormat stereoLFormat = _stereoLVideoClient->getVideoFormat();
            VideoFormat stereoRFormat = _stereoRVideoClient->getVideoFormat();
            _mainUi->getCameraWidget()->playStereo(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SL_CAMERA_PORT),
                                               stereoLFormat,
                                               SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_SR_CAMERA_PORT),
                                               stereoRFormat);
            // Record streams
            qint64 timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
            _stereoLGStreamerRecorder->begin(&stereoLFormat, timestamp);
            _stereoRGStreamerRecorder->begin(&stereoRFormat, timestamp);

            if (!_settings.enableStereoUi || !_settings.enableStereoVideo) {
                LOG_E(LOG_TAG, "Video clients are playing stereo, but UI is not in stereo mode");
                _settings.enableStereoUi = true;
                _settings.enableStereoVideo = true;
                _settings.syncUi(_controlUi);
            }
        }
    }
    else if ((client == _aux1VideoClient) && (_aux1VideoClient->getState() == MediaClient::StreamingState)) {
        // Aux1 camera is streaming

        VideoFormat aux1Format = _aux1VideoClient->getVideoFormat();
        if (_settings.enableStereoUi) {
            _mainUi->getCameraWidget()->playStereo(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1L_CAMERA_PORT),
                                             aux1Format,
                                             SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1R_CAMERA_PORT),
                                             aux1Format);
        }
        else {
            _mainUi->getCameraWidget()->playMono(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_A1L_CAMERA_PORT),
                                             aux1Format);
        }

        // Record stream
        _aux1GStreamerRecorder->begin(&aux1Format, QDateTime::currentDateTime().toMSecsSinceEpoch());
    }
    else if ((client == _monoVideoClient) && (_monoVideoClient->getState() == MediaClient::StreamingState)) {
        // Mono camera is streaming

        VideoFormat monoFormat = _monoVideoClient->getVideoFormat();
        if (_settings.enableStereoUi) {
            _mainUi->getCameraWidget()->playStereo(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_ML_CAMERA_PORT),
                                             monoFormat,
                                             SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_MR_CAMERA_PORT),
                                             monoFormat);
        }
        else {
            _mainUi->getCameraWidget()->playMono(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_RESEARCH_ML_CAMERA_PORT),
                                             monoFormat);
        }

        // Record stream
        _monoGStreamerRecorder->begin(&monoFormat, QDateTime::currentDateTime().toMSecsSinceEpoch());
    }

    if (state == MediaClient::StreamingState) {
        _settings.enableVideo = true;
        _settings.setSelectedCamera(client->getMediaId());
        _settings.syncUi(_controlUi);
    }
    else if ((_stereoLVideoClient->getState() != MediaClient::StreamingState) &&
             (_stereoRVideoClient->getState() != MediaClient::StreamingState) &&
             (_monoVideoClient->getState() != MediaClient::StreamingState) &&
             (_aux1VideoClient->getState() != MediaClient::StreamingState)) {
        // No cameras streaming
        _settings.enableVideo = false;
        _settings.syncUi(_controlUi);
    }
}

void ResearchControlProcess::audioClientStateChanged(MediaClient *client, MediaClient::State state) {
    Q_UNUSED(client);

    switch (state) {
    case AudioClient::StreamingState: {
        AudioFormat audioFormat = _audioClient->getAudioFormat();
        _audioPlayer->play(SocketAddress(QHostAddress::LocalHost, NETWORK_ALL_AUDIO_PORT),
                           audioFormat);
        //_audioGStreamerRecorder->begin(&audioFormat, QDateTime::currentDateTime().toMSecsSinceEpoch());
        _settings.enableAudio = true;
        _settings.syncUi(_controlUi);
        break;
    }
    case AudioClient::ConnectingState:
        _audioPlayer->stop();
        //_audioGStreamerRecorder->stop();
        _settings.enableAudio = false;
        _settings.syncUi(_controlUi);
        break;
    default:
        break;
    }
}

void ResearchControlProcess::updateUiConnectionState() {
    switch (_roverChannel->getState()) {
    case Channel::ErrorState:
        _controlUi->setProperty("connectionState", "error");
        _commentsUi->setProperty("connectionState", "error");
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant,"error"),
                                  Q_ARG(QVariant, "Control Channel Error"),
                                  Q_ARG(QVariant, "An unrecoverable netowork error occurred. Please exit and check the log."));
        break;
    case Channel::ConnectedState:
        _controlUi->setProperty("connectionState", "connected");
        _commentsUi->setProperty("connectionState", "connected");
        break;
    default:
        _controlUi->setProperty("connectionState", "connecting");
        _commentsUi->setProperty("connectionState", "connecting");
        break;
    }
}

void ResearchControlProcess::timerEvent(QTimerEvent *e) {
    if (e->timerId() == _pingTimerId) {
        /****************************************
         * This timer runs regularly to update the
         * ping statistic
         */
        QMetaObject::invokeMethod(_controlUi,
                                  "updatePing",
                                  Q_ARG(QVariant, _driveSystem->getChannel()->getLastRtt()));
        if (_roverChannel->getLastRtt() > 1000) {
            // The REAL ping is over 1 second
            QMetaObject::invokeMethod(_controlUi,
                                      "notify",
                                      Q_ARG(QVariant,"warning"),
                                      Q_ARG(QVariant, "Ping Warning"),
                                      Q_ARG(QVariant, "Actual (non-simulated) ping is over 1 second."));
        }
    }
    else if (e->timerId() == _bitrateUpdateTimerId) {
        /*****************************************
         * This timer regularly updates the total bitrate count,
         * and also broadcasts it to slave mission controls since they
         * cannot calculate video bitrate
         */
        quint64 bpsRoverDown = 0, bpsRoverUp = 0;
        bpsRoverUp += _monoVideoClient->getBitrate();
        bpsRoverUp += _stereoLVideoClient->getBitrate();
        bpsRoverUp += _stereoRVideoClient->getBitrate();
        bpsRoverUp += _aux1VideoClient->getBitrate();
        bpsRoverUp += _audioClient->getBitrate();
        bpsRoverUp += _roverChannel->getBitsPerSecondDown();
        bpsRoverDown += _roverChannel->getBitsPerSecondUp();
        bpsRoverUp += _driveSystem->getChannel()->getBitsPerSecondDown();
        bpsRoverDown += _driveSystem->getChannel()->getBitsPerSecondUp();

        QMetaObject::invokeMethod(_controlUi,
                                  "updateBitrate",
                                  Q_ARG(QVariant, bpsRoverUp),
                                  Q_ARG(QVariant, bpsRoverDown));
    }
    else {
        QObject::timerEvent(e);
    }
}

void ResearchControlProcess::sendStartRecordCommandToRover() {
    QByteArray message;
    QDataStream stream(&message, QIODevice::WriteOnly);
    SharedMessageType messageType = SharedMessage_Research_StartDataRecording;
    stream << static_cast<qint32>(messageType);
    stream << _recordStartTime;
    _roverChannel->sendMessage(message);
}

void ResearchControlProcess::sendStopRecordCommandToRover() {
    QByteArray message;
    QDataStream stream(&message, QIODevice::WriteOnly);
    SharedMessageType messageType = SharedMessage_Research_StopDataRecording;
    stream << static_cast<qint32>(messageType);
    _roverChannel->sendMessage(message);
}

void ResearchControlProcess::roverSharedChannelMessageReceived(const char *message, Channel::MessageSize size) {

    QByteArray byteArray = QByteArray::fromRawData(message, size);
    QDataStream stream(byteArray);
    SharedMessageType messageType;

    LOG_D(LOG_TAG, "Getting shared channel message");

    stream >> reinterpret_cast<qint32&>(messageType);
    switch (messageType) {
    case SharedMessage_RoverStatusUpdate: {
        bool mbedStatus;
        stream >> mbedStatus;
        if (!mbedStatus) {
            QMetaObject::invokeMethod(_controlUi,
                                      "notify",
                                      Q_ARG(QVariant, "error"),
                                      Q_ARG(QVariant, "Mbed Error"),
                                      Q_ARG(QVariant, "The rover has lost connection to the mbed. Driving and data collection will no longer work."));
            _controlUi->setProperty("mbedStatus", "Error");
        }
        else {
            _controlUi->setProperty("mbedStatus", "Operational");
        }
    }
        break;
    case SharedMessage_RoverMediaServerError: {
        qint32 mediaId;
        QString error;
        stream >> mediaId;
        stream >> error;

        if (mediaId == _audioClient->getMediaId()) {
            QMetaObject::invokeMethod(_controlUi,
                                      "notify",
                                      Q_ARG(QVariant, "warning"),
                                      Q_ARG(QVariant, "Audio Stream Error"),
                                      Q_ARG(QVariant, "The rover encountered an error trying to stream audio."));
            LOG_E(LOG_TAG, "Audio streaming error: " + error);
        }
        else {
            QMetaObject::invokeMethod(_controlUi,
                                      "notify",
                                      Q_ARG(QVariant, "warning"),
                                      Q_ARG(QVariant, "Video Stream Error"),
                                      Q_ARG(QVariant, "The rover encountered an error trying to stream this camera."));
            LOG_E(LOG_TAG, "Streaming error on camera " + QString::number(mediaId) + ": " + error);
        }
    }
        break;
    case SharedMessage_RoverGpsUpdate: {
        NmeaMessage location;
        stream >> location;
        // Forward to UI
        QMetaObject::invokeMethod(_controlUi,
                                  "updateGpsLocation",
                                  Q_ARG(QVariant, location.Latitude),
                                  Q_ARG(QVariant, location.Longitude),
                                  Q_ARG(QVariant, location.Heading));

        // Forward to logger
        _gpsDataSeries->addLocation(location);
    }
        break;
    case SharedMessage_Research_RoverDriveOverrideStart:
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant, "information"),
                                  Q_ARG(QVariant, "Network Driving Disabled"),
                                  Q_ARG(QVariant, "The rover is being driven by serial override. Network drive commands will not be accepted."));
        _controlUi->setProperty("driveMbedStatus", "Serial Override");
        break;
    case SharedMessage_Research_RoverDriveOverrideEnd:
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant, "information"),
                                  Q_ARG(QVariant, "Network Driving Enabled"),
                                  Q_ARG(QVariant, "The rover has resumed accepting network drive commands."));
        _controlUi->setProperty("driveMbedStatus", "Operational");
        break;
    case SharedMessage_Research_SensorUpdate: {
        QByteArray data;
        stream >> data;
        // This raw data should be sent to an MbedParser to be decoded
        _sensorDataSeries->newData(data.data(), data.length());
        break;
    }
    case SharedMessage_Research_StartDataRecording: {
        // Rover has responed that they are starting data recording, start ours
        if (!_dataRecorder->startLog(QDateTime::fromMSecsSinceEpoch(_recordStartTime))) {
            stopDataRecording();
            QMetaObject::invokeMethod(_controlUi,
                                      "notify",
                                      Q_ARG(QVariant,"error"),
                                      Q_ARG(QVariant, "Cannot Record Data"),
                                      Q_ARG(QVariant, "An error occurred attempting to start data logging."));

            // Try to tell the rover to stop their recording too
            sendStopRecordCommandToRover();
        }
        _controlUi->setProperty("recordingState", "recording");
        _commentsUi->setProperty("recordingState", "recording");
        break;
    }
    default:
        LOG_E(LOG_TAG, "Got unknown message header on shared channel");
        break;
    }
}

void ResearchControlProcess::driveConnectionStateChanged(Channel::State state) {
    switch (state) {
    case Channel::ErrorState:
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant,"error"),
                                  Q_ARG(QVariant, "Drive Channel Error"),
                                  Q_ARG(QVariant, "An unrecoverable netowork error occurred. Please exit and check the log."));
        _controlUi->setProperty("driveMbedStatus", "Network Error");
        break;
    case Channel::ConnectedState:
        QMetaObject::invokeMethod(_controlUi,
                                  "notify",
                                  Q_ARG(QVariant,"information"),
                                  Q_ARG(QVariant, "Drive Channel Connected"),
                                  Q_ARG(QVariant, "You are now connected to the rover's drive system."));
        _controlUi->setProperty("driveMbedStatus", "Operational");
        break;
    default:
        if (_driveSystem->getChannel()->wasConnected()) {
            QMetaObject::invokeMethod(_controlUi,
                                      "notify",
                                      Q_ARG(QVariant,"error"),
                                      Q_ARG(QVariant, "Drive Channel Disconnected"),
                                      Q_ARG(QVariant, "The network connection to the rover's drive system has been lost."));
            _controlUi->setProperty("driveMbedStatus", "Network Disconnected");
        }
        break;
    }
}

void ResearchControlProcess::stopAllRoverCameras() {
    QByteArray message;
    QDataStream stream(&message, QIODevice::WriteOnly);
    SharedMessageType messageType = SharedMessage_Research_StopAllCameraStreams;

    _mainUi->getCameraWidget()->stop(_settings.enableStereoUi);
    stream << static_cast<qint32>(messageType);
    _roverChannel->sendMessage(message);
}

void ResearchControlProcess::startMonoCameraStream(VideoFormat format) {
    stopAllRoverCameras();

    if (format.isUseable()) {
        // Start mono stream
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        SharedMessageType messageType = SharedMessage_Research_StartMonoCameraStream;
        stream << static_cast<qint32>(messageType);
        stream << format.serialize();

        _roverChannel->sendMessage(message);
    }
    else {
        LOG_E(LOG_TAG, "startMonoCameraStream(): This format is not useable. If you want to stop this camera, call stopAllRoverCameras() instead");
    }
}

void ResearchControlProcess::startStereoCameraStream(VideoFormat format) {
    stopAllRoverCameras();

    if (format.isUseable()) {
        // Start stereo stream
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        SharedMessageType messageType = SharedMessage_Research_StartStereoCameraStream;
        stream << static_cast<qint32>(messageType);
        stream << format.serialize();

        _roverChannel->sendMessage(message);
    }
    else {
        LOG_E(LOG_TAG, "startStereoCameraStream(): This format is not useable. If you want to stop this camera, call stopAllRoverCameras() instead");
    }
}

void ResearchControlProcess::startAux1CameraStream(VideoFormat format) {
    stopAllRoverCameras();

    if (format.isUseable()) {
        // Start stereo stream
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        SharedMessageType messageType = SharedMessage_Research_StartAux1CameraStream;
        stream << static_cast<qint32>(messageType);
        stream << format.serialize();

        _roverChannel->sendMessage(message);
    }
    else {
        LOG_E(LOG_TAG, "startAux1CameraStream(): This format is not useable. If you want to stop this camera, call stopAllRoverCameras() instead");
    }
}

void ResearchControlProcess::stopAudio() {
    QByteArray message;
    QDataStream stream(&message, QIODevice::WriteOnly);
    SharedMessageType messageType = SharedMessage_RequestDeactivateAudioStream;

    stream << static_cast<qint32>(messageType);
    _roverChannel->sendMessage(message);
}

void ResearchControlProcess::startAudioStream(AudioFormat format) {
    if (format.isUseable()) {
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        SharedMessageType messageType;
        messageType = SharedMessage_RequestActivateAudioStream;

        stream << static_cast<qint32>(messageType);
        stream << format.serialize();
        _roverChannel->sendMessage(message);
    }
    else {
        LOG_E(LOG_TAG, "startAudioStream(): This format is not useable. If you want to stop the audio stream, call stopAudio() instead");
    }
}

ResearchControlProcess::~ResearchControlProcess() {
    if (_mainUi) {
        delete _mainUi;
    }
}

} // namespace MissionControl
} // namespace Soro
