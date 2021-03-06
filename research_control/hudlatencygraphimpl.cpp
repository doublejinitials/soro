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

#include "hudlatencygraphimpl.h"
#include "qmath.h"

namespace Soro {
namespace MissionControl {

HudLatencyGraphImpl::HudLatencyGraphImpl(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    _mode = "vertical";
    START_TIMER(_updateTimerId, 20);
}

QString HudLatencyGraphImpl::mode() const {
    return _mode;
}

void HudLatencyGraphImpl::setMode(QString mode) {
    _mode = mode;
}

int HudLatencyGraphImpl::latency() const {
    return _latency;
}

void HudLatencyGraphImpl::setLatency(int latency) {
    if (qAbs(_latency - latency) > _latencyThreshold) {
        _latency = latency;
    }
}

float HudLatencyGraphImpl::value() const {
    return _value;
}

void HudLatencyGraphImpl::setValue(float value) {
    _value = value;
}

int HudLatencyGraphImpl::latencyThreshold() const {
    return _latencyThreshold;
}

void HudLatencyGraphImpl::setLatencyThreshold(int threshold) {
    _latencyThreshold = threshold;
}

void HudLatencyGraphImpl::paint(QPainter *painter) {
    qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();

    QMap<qint64, float>::const_iterator i = _history.constBegin();

    if (_mode == "horizontal") {
        int blobSize = height() / 4;

        // Draw bottom blob
        int startBlobY = (height() / 2) + ((height() / 2 - blobSize / 2) * _value);
        painter->setBrush(QBrush(Qt::white));
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawEllipse(0,
                             startBlobY - (blobSize / 2),
                             blobSize,
                             blobSize);

        // Draw top blob
        float endValue = nearestValue(QDateTime::currentDateTime().toMSecsSinceEpoch());
        int endBlobY = (height() / 2) + ((height() / 2 - blobSize / 2) * endValue);
        painter->setBrush(QBrush(Qt::white));
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawEllipse(width() - blobSize,
                             endBlobY - (blobSize / 2),
                             blobSize,
                             blobSize);

        // Draw graph
        QPen pen;
        pen.setColor(QColor("#88ffffff"));
        pen.setWidth(blobSize / 5);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();

        QPainterPath path;
        path.moveTo(width() - blobSize / 2, endBlobY);
        while (i != _history.constEnd()) {
            path.lineTo(width() - (blobSize / 2) - ((width() - 2 * blobSize) * ((float)(i.key() - now) / (float)_latency)),
                        (height() / 2) + (height() / 2 - blobSize / 2) * i.value());
            i++;
        }
        path.lineTo(0, startBlobY);
        painter->drawPath(path);
    }
    else if (_mode == "vertical") {
        int blobSize = width() / 4;

        // Draw bottom blob
        int startBlobX = (width() / 2) + ((width() / 2 - blobSize / 2) * _value);
        painter->setBrush(QBrush(Qt::white));
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawEllipse(startBlobX - (blobSize / 2),
                             height() - blobSize,
                             blobSize,
                             blobSize);

        // Draw top blob
        float endValue = nearestValue(QDateTime::currentDateTime().toMSecsSinceEpoch());
        int endBlobX = (width() / 2) + ((width() / 2 - blobSize / 2) * endValue);
        painter->setBrush(QBrush(Qt::white));
        painter->setPen(Qt::NoPen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawEllipse(endBlobX - (blobSize / 2),
                             0,
                             blobSize,
                             blobSize);

        // Draw graph
        QPen pen;
        pen.setColor(QColor("#88ffffff"));
        pen.setWidth(blobSize / 5);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();

        QPainterPath path;
        path.moveTo(endBlobX, blobSize / 2);
        while (i != _history.constEnd()) {
            path.lineTo((width() / 2) + (width() / 2 - blobSize / 2) * i.value(),
                              (blobSize / 2) + ((height() - 2 * blobSize) * ((float)(i.key() - now) / (float)_latency)));
            i++;
        }
        path.lineTo(startBlobX, height() - blobSize / 2);
        painter->drawPath(path);
    }
}

float HudLatencyGraphImpl::nearestValue(qint64 time) {
    QList<qint64> keys = _history.keys();
    int left = 0, right = keys.length() - 1;
    if (right == -1) return 0;

    while (right - left > 1) {
        qint64 m = keys.at((left + right) / 2);
        if (m > time) {
            right = (left + right) / 2;
        }
        else if (m < time) {
            left = (left + right) / 2;
        }
        else return _history.value(m);
    }
    qint64 ml = keys.at(left);
    qint64 mr = keys.at(right);

    if (qAbs(ml - time) > qAbs(mr - time)) {
        return _history.value(mr);
    }
    return _history.value(ml);
}

void HudLatencyGraphImpl::timerEvent(QTimerEvent *e) {
    if (e->timerId() == _updateTimerId) {
        // Prune value map
        qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();

        // Remove values in the past
        QMap<qint64, float>::iterator i = _history.begin();
        while ((i != _history.end()) && (i.key() <= now)) {
            i = _history.erase(i);
        }

        // Remove values farther in the future than our latency
        qint64 lastKey;
        while (!_history.empty() && ((lastKey = _history.lastKey()) > now + _latency)) {
            _history.remove(lastKey);
        }

        _history.insert((now + _latency), _value);
        // Invalidate
        update();
    }
}

} // namespace MissionControl
} // namespace Soro
