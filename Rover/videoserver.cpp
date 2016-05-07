#include "videoserver.h"

#define LOG_TAG _name + "(S)"

namespace Soro {
namespace Rover {

VideoServer::VideoServer(QString name, SocketAddress host, Logger *log, QObject *parent) : QObject(parent) {
    _name = name;
    _log = log;
    _host = host;

    LOG_I("Creating new video server");

    _controlChannel = new Channel(this, host.port, _name, Channel::TcpProtocol, host.host);
    connect(_controlChannel, SIGNAL(stateChanged(Channel*, Channel::State)),
            this, SLOT(controlChannelStateChanged(Channel*, Channel::State)));
    _controlChannel->open();

    _videoSocket = new QUdpSocket(this);

    _child.setProgram(QCoreApplication::applicationDirPath() + "/VideoStreamProcess");

    _state = IdleState;
}

VideoServer::~VideoServer() {
    stop();
}

void VideoServer::stop() {
    if (_state == IdleState) {
        LOG_W("stop() called: Server is already stopped");
        return;
    }
    if (_child.state() != QProcess::NotRunning) {
        LOG_I("stop() called: killing child process");
        _child.kill();
        _child.waitForFinished();
        LOG_I("Child process has been killed");
    }
    else {
        LOG_I("stop() called, however the child process is not running");
    }
    _deviceDescription = "";
    _format.Encoding = UnknownOrNoEncoding;
    if (_controlChannel->getState() == Channel::ConnectedState) {
        // notify the client that the server is stopping the stream
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << QString("eos");
        _controlChannel->sendMessage(message.constData(), message.size());
    }
    _videoSocket->abort();
    setState(IdleState);
}

void VideoServer::start(QString deviceName, StreamFormat format) {
    LOG_I("start() called");
    if (_state != IdleState) {
        LOG_I("Server is not idle, stopping operations");
        stop();
    }
    _deviceDescription = deviceName;
    _format = format;
    setState(WaitingState);
    startInternal();
}

void VideoServer::start(FlyCapture2::PGRGuid camera, StreamFormat format) {
    start("FlyCapture2:" + QString::number(camera.value[0]) + ":"
                        + QString::number(camera.value[1]) + ":"
                        + QString::number(camera.value[2]) + ":"
                        + QString::number(camera.value[3]),
                        format);
}

void VideoServer::startInternal() {
    if (_state != WaitingState) return;
    if (_controlChannel->getState() == Channel::ConnectedState) {
        _videoSocket->abort();
        if (!_videoSocket->bind(_host.host, _host.port)) {
            LOG_E("Cannot bind to video port: " + _videoSocket->errorString());
            QTimer::singleShot(500, this, SLOT(startInternal()));
            return;
        }
        connect(_videoSocket, SIGNAL(readyRead()), this, SLOT(videoSocketReadyRead()));
        _videoSocket->open(QIODevice::ReadWrite);
        // notify a connected client that there is about to be a stream change
        // and they should verify their UDP address
        LOG_I("Sending stream start message to client");
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << QString("start");
        _controlChannel->sendMessage(message.constData(), message.size());
        // client must respond within a certain time or the process will start again
        QTimer::singleShot(3000, this, SLOT(startInternal()));
    }
    else {
        LOG_I("Waiting for client to connect...");
        QTimer::singleShot(500, this, SLOT(startInternal()));
    }
}

void VideoServer::beginStream(SocketAddress address) {
    QStringList args;
    args << _deviceDescription;
    args << QString::number(reinterpret_cast<unsigned int&>(_format.Encoding));
    args << QString::number(_format.Width);
    args << QString::number(_format.Height);
    args << QString::number(_format.Framerate);
    switch (_format.Encoding) {
    case MjpegEncoding:
        args << QString::number(_format.Mjpeg_Quality);
        break;
    case Mpeg2Encoding:
        args << QString::number(_format.Mpeg2_Bitrate);
        break;
    default:
        break;
    }

    args << QHostAddress(address.host.toIPv4Address()).toString();
    args << QString::number(address.port);
    args << QHostAddress(_host.host.toIPv4Address()).toString();
    args << QString::number(_host.port);

    _child.setArguments(args);
    connect(&_child, SIGNAL(stateChanged(QProcess::ProcessState)),
               this, SLOT(childStateChanged(QProcess::ProcessState)));
    _child.start();
    setState(StreamingState);
}

void VideoServer::timerEvent(QTimerEvent *e) {
    QObject::timerEvent(e);
}

void VideoServer::videoSocketReadyRead() {
    if (!_videoSocket | (_state == StreamingState)) return;
    SocketAddress peer;
    char buffer[100];
    _videoSocket->readDatagram(&buffer[0], 100, &peer.host, &peer.port);
    if ((strcmp(buffer, _name.toLatin1().constData()) == 0) && (_format.Encoding != UnknownOrNoEncoding)) {
        LOG_I("Client has completed handshake on its UDP address");
        // send the client a message letting them know we are now streaming to their address,
        // and tell them the stream metadata
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << QString("streaming");
        stream << reinterpret_cast<unsigned int&>(_format.Encoding);
        stream << _format.Width;
        stream << _format.Height;
        stream << _format.Framerate;
        switch (_format.Encoding) {
        case MjpegEncoding:
            stream << _format.Mjpeg_Quality;
            break;
        case Mpeg2Encoding:
            stream << _format.Mpeg2_Bitrate;
            break;
        default:
            LOG_E("The format's encoding is set to Unknown, why am I starting a stream???");
            stop();
            return;
        }
        LOG_I("Sending stream configuration to client");
        _controlChannel->sendMessage(message.constData(), message.size());
        // Disconnect the video UDP socket so udpsink can bind to it
        disconnect(_videoSocket, SIGNAL(readyRead()), this, SLOT(videoSocketReadyRead()));
        _videoSocket->abort(); // MUST ABORT THE SOCKET!!!!
        beginStream(peer);
    }
}

void VideoServer::childStateChanged(QProcess::ProcessState state) {
    switch (state) {
    case QProcess::NotRunning:
        LOG_I("Child is no longer running (exit code " + QString::number(_child.exitCode()) + ")");

        disconnect(&_child, SIGNAL(stateChanged(QProcess::ProcessState)),
                   this, SLOT(childStateChanged(QProcess::ProcessState)));

        switch (_child.exitCode()) {
        case 0:
        case STREAMPROCESS_ERR_GSTREAMER_EOS:
            emit eos(this);
            break;
        case STREAMPROCESS_ERR_FLYCAP_ERROR:
            emit error(this, "Error in FlyCapture2 decoding");
            break;
        case STREAMPROCESS_ERR_GSTREAMER_ERROR:
            emit error(this, "Gstreamer error");
            break;
        case STREAMPROCESS_ERR_INVALID_ARGUMENT:
        case STREAMPROCESS_ERR_NOT_ENOUGH_ARGUMENTS:
        case STREAMPROCESS_ERR_UNKNOWN_CODEC:
        default:
            emit error(this, "Unknown error/segmentation fault");
        }

        setState(IdleState);
        break;
    case QProcess::Starting:
        LOG_I("Child is starting...");
        break;
    case QProcess::Running:
        LOG_I("Child has started successfully");
        break;
    }
}

void VideoServer::controlChannelStateChanged(Channel *channel, Channel::State state) {
    Q_UNUSED(channel);
    if (state != Channel::ConnectedState) {
        stop();
    }
}

VideoServer::State VideoServer::getState() {
    return _state;
}

QString VideoServer::getCameraName() {
    return _name;
}

const StreamFormat& VideoServer::getCurrentStreamFormat() const {
    return _format;
}

void VideoServer::setState(VideoServer::State state) {
    if (_state != state) {
        LOG_I("Changing to state " + QString::number(reinterpret_cast<unsigned int&>(state)));
        _state = state;
        emit stateChanged(this, state);
    }
}

} // namespace Rover
} // namespace Soro
