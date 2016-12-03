/*
 * Copyright 2016 The University of Oklahoma.
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

/*********************************************************
 * This code can be compiled on a Qt or mbed enviornment *
 *********************************************************/

#include "mbedchannel.h"
#include "util.h"

#define IDLE_CONNECTION_TIMEOUT 2000
#define BROADCAST_PACKET "MbedChannel"
#define _MBED_MSG_TYPE_NORMAL 1
#define _MBED_MSG_TYPE_LOG 2
#define _MBED_MSG_TYPE_BROADCAST 3
#define _MBED_MSG_TYPE_HEARTBEAT 4

namespace Soro {

#ifdef QT_CORE_LIB

void MbedChannel::setChannelState(MbedChannel::State state) {
    if (_state != state) {
        _state = state;
        emit stateChanged(this, state);
    }
}

void MbedChannel::socketError(QAbstractSocket::SocketError err) {
    Q_UNUSED(err);
    LOG_E(LOG_TAG, "Error: " + _socket->errorString());
    START_TIMER(_resetConnectionTimerId, 500);
}

void MbedChannel::socketReadyRead() {
    qint64 length;
    SocketAddress peer;
    while (_socket->hasPendingDatagrams()) {
        length = _socket->readDatagram(&_buffer[0], 512, &peer.host, &peer.port);
        if ((length < 6) | (length == 512)) continue;
        if (_buffer[0] == '\0') {
            continue;
        }
        if ((_buffer[0] != _mbedId) || (peer.port != _host.port)) {
            LOG_W(LOG_TAG, "Received invalid message (got Mbed ID) "
                  + QString::number(reinterpret_cast<unsigned char&>(_buffer[0]))
                  + " on port " + QString::number(peer.port));
            continue;
        }
        unsigned int sequence = Util::deserialize<unsigned int>(_buffer + 2);
        if (_state == ConnectingState) {
            LOG_I(LOG_TAG, "Connected to mbed client");
            setChannelState(ConnectedState);
        }
        else if (sequence < _lastReceiveId) continue;
        _lastReceiveId = sequence;
        _active = true;
        switch (reinterpret_cast<unsigned char&>(_buffer[1])) {
        case _MBED_MSG_TYPE_NORMAL:
            if (length > 6) {
                emit messageReceived(_buffer + 6, length - 6);
            }
            break;
        case _MBED_MSG_TYPE_LOG:
            LOG_I(LOG_TAG, "Mbed:" + QString(_buffer + 6));
            break;
        case _MBED_MSG_TYPE_BROADCAST:
            _socket->writeDatagram(BROADCAST_PACKET, strlen(BROADCAST_PACKET) + 1, QHostAddress::Broadcast, _host.port);
            break;
        case _MBED_MSG_TYPE_HEARTBEAT:
            break;
        default:
            LOG_E(LOG_TAG, "Got message with unknown type");
            break;
        }
    }
}

void MbedChannel::resetConnection() {
    LOG_I(LOG_TAG, "Connection is resetting...");
    setChannelState(ConnectingState);
    _lastReceiveId = 0;
    _active = false;
    _socket->abort();
    if (_socket->bind(_host.host, _host.port)) {
        LOG_I(LOG_TAG, "Listening on UDP port " + _host.toString());
        _socket->open(QIODevice::ReadWrite);
    }
    else {
        LOG_E(LOG_TAG, "Failed to bind to " + _host.toString());
    }
}

MbedChannel::MbedChannel(SocketAddress host, unsigned char mbedId, QObject *parent) : QObject(parent) {
    _host = host;
    _socket = new QUdpSocket(this);
    _mbedId = reinterpret_cast<char&>(mbedId);
    LOG_TAG = "Mbed(" + QString::number(mbedId) + ")";
    LOG_I(LOG_TAG, "Creating new mbed channel");
    connect(_socket, SIGNAL(readyRead()),
            this, SLOT(socketReadyRead()));
    connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(socketError(QAbstractSocket::SocketError)));
    resetConnection();
    START_TIMER(_watchdogTimerId, IDLE_CONNECTION_TIMEOUT);
}

MbedChannel::~MbedChannel() {
    _socket->abort();
    delete _socket;
}

void MbedChannel::sendMessage(const char *message, int length) {
    if ((_state == ConnectedState) && (length < 500)) {
        _buffer[0] = '\0';
        _buffer[1] = _mbedId;
        Util::serialize<unsigned int>(_buffer + 2, _nextSendId++);
        memcpy(_buffer + 6, message, length);
        _socket->writeDatagram(_buffer, length + 6, QHostAddress::Broadcast, _host.port);
    }
}

void MbedChannel::timerEvent(QTimerEvent *e) {
    QObject::timerEvent(e);
    if (e->timerId() == _watchdogTimerId) {
        if ((_state == ConnectedState) & !_active) {
            LOG_E(LOG_TAG, "Mbed client has timed out");
            setChannelState(ConnectingState);
        }
        _active = false;
    }
    else if (e->timerId() == _resetConnectionTimerId) {
        resetConnection();
        KILL_TIMER(_resetConnectionTimerId); //single shot
    }
}

MbedChannel::State MbedChannel::getState() const {
    return _state;
}

#endif
#ifdef TARGET_LPC1768

extern "C" void mbed_reset();

void MbedChannel::panic() {
    DigitalOut led1(LED1);
    DigitalOut led2(LED2);
    DigitalOut led3(LED3);
    DigitalOut led4(LED4);
    while (1) {
        led1 = 1;
        led2 = 0;
        led3 = 0;
        led4 = 1;
        wait_ms(150);
        led1 = 0;
        led2 = 1;
        led3 = 1;
        led4 = 0;
        wait_ms(150);
    }
}

void MbedChannel::reset() {
    if (_resetCallback != NULL) {
        _resetCallback();
    }
    mbed_reset();
}

/*void MbedChannel::loadConfig() {
    LocalFileSystem local("local");
    FILE *configFile = fopen("/local/server.txt", "r");
    if (configFile != NULL) {
        char *line = new char[64];
        fgets(line, 64, configFile);
        fclose(configFile);
        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == ':') {
                char *ip = new char[i + 1];
                strncpy(ip, line, i);
                ip[i] = '\0';
                unsigned int port = (unsigned int)atoi(line + i + 1);
                if ((port == 0) | (port > 65535)) break;
                _server.set_address(ip, port);
                return;
            }
        }
    }
    //an error occurred
    panic();
}*/

bool MbedChannel::setServerAddress() {
    LocalFileSystem local("local");
    FILE *configFile = fopen("/local/server.txt", "r");
    if (configFile != NULL) {
        char *line = new char[64];
        fgets(line, 64, configFile);
        fclose(configFile);
        unsigned int port = (unsigned int)atoi(line);
        if ((port == 0) | (port > 65535)) {
            return false;
        }
        char *ip = _eth->getIPAddress();
        char *broadcast = new char[strlen(ip) + 3]; //make sure we have enough room to add 2 more digits
        strcpy(broadcast, ip);
        strcpy(strrchr(broadcast, '.'), ".255");
        // set address as broadcast address and port from file
        _server.set_address(broadcast, port);
    }
    return true;
}

void MbedChannel::initConnection() {
    //initialize ethernet interface
    DigitalOut led1(LED1);
    DigitalOut led2(LED2);
    DigitalOut led3(LED3);
    DigitalOut led4(LED4);
    led1 = 1;
    if (_eth->init() != 0) {
        wait(0.5);
        reset();
    }
    // connect to ethernet and get address through DHCP
    led2 = 1;
    if (_eth->connect() != 0) {
        wait(0.5);
        reset();
    }
    led3 = 1;

    if (!setServerAddress()) {
        panic();
    }
    setTimeout(IDLE_CONNECTION_TIMEOUT / 3);
    //initialize socket
    while (_socket->bind(_server.get_port()) != 0) {
        wait(0.2);
        led3 = 0;
        wait(0.2);
        led3 = 1;
    }
    if (_socket->set_broadcasting(true) != 0) {
        panic();
    }

    Endpoint peer;
    int packet_len = strlen(BROADCAST_PACKET) + 1;
    char buffer[packet_len];
    while (1) {
        //send broadcast handshake
        sendMessage(BROADCAST_PACKET, packet_len, _MBED_MSG_TYPE_BROADCAST);
        while (1) {
            //recieve any responses
            int len = _socket->receiveFrom(peer, &buffer[0], packet_len);
            if (len <= 0) break;
            if ((len == packet_len) && (strcmp(&buffer[0], BROADCAST_PACKET) == 0))  {
                //received a response from the server
                led1 = 0;
                led2 = 0;
                led3 = 0;
                led4 = 0;
                return;
            }
        }
        wait(0.2);
        led4 = 0;
        wait(0.2);
        led4 = 1;
    }
}

MbedChannel::MbedChannel(unsigned char mbedId) {
    _resetCallback = NULL;
    _mbedId = reinterpret_cast<char&>(mbedId);
    _eth = new EthernetInterface;
    _socket = new UDPSocket;
    initConnection();
    _lastSendTime = time(NULL);
    _lastReceiveId = 0;
    _nextSendId = 0;
}

MbedChannel::~MbedChannel() {
    _socket->close();
    delete _socket;
    //this class is managing the ethernet
    _eth->disconnect();
    delete _eth;
}

void MbedChannel::setTimeout(unsigned int millis) {
    if (millis < IDLE_CONNECTION_TIMEOUT / 2)
        _socket->set_blocking(false, millis);
}

void MbedChannel::sendMessage(char *message, int length, unsigned char type) {
    if (!isEthernetActive()) {
        reset();
    }
    _buffer[0] = _mbedId;
    _buffer[1] = reinterpret_cast<char&>(type);
    Util::serialize<unsigned int>(_buffer + 2, _nextSendId++);
    memcpy(_buffer + 6, message, length);
    _socket->sendTo(_server, _buffer, length + 6);
    _lastSendTime = time(NULL);
}

void MbedChannel::setResetListener(void (*callback)(void)) {
    _resetCallback = callback;
}

int MbedChannel::read(char *outMessage, int maxLength) {
    if (!isEthernetActive()) {
        reset();
    }
    Endpoint peer;
    //Check if a heartbeat should be sent
    if (time(NULL) - _lastSendTime >= 1) {
        sendMessage(NULL, 0, _MBED_MSG_TYPE_HEARTBEAT);
    }
    int len = _socket->receiveFrom(peer, _buffer, maxLength);
    unsigned int sequence = Util::deserialize<unsigned int>(_buffer + 2);
    if ((len < 6)
            || (peer.get_port() != _server.get_port())
            || (_buffer[0] != '\0')
            || (_buffer[1] != _mbedId)) {
        return -1;
    }
    if ((sequence < _lastReceiveId) && (time(NULL) - _lastReceiveTime < 2)) {
        return -1;
    }
    _lastReceiveId = sequence;
    _lastReceiveTime = time(NULL);
    memcpy(outMessage, _buffer + 6, len - 6);
    return len - 6;
}
#endif

}