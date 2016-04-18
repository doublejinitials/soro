#include "iniparser.h"

//lol thanks stackoverflow
#define IPV4_REGEX "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
#define IPV6_REGEX "(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(:[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]{1,}|::(ffff(:0{1,4}){0,1}:){0,1}((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])|([0-9a-fA-F]{1,4}:){1,4}:((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9]))"

//This char/string separates tags from values in a line
#define DELIM '='
//This char/string denotes a line as a comment
#define COMMENT '#'

namespace Soro {

bool IniParser::load(QTextStream& stream) {
    _contents.clear();

    QString line;
    do {
        line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(COMMENT)) {
            continue;
        }
        int sepIndex = line.indexOf(DELIM);
        if (sepIndex < 0) return false;
        QString tag = line.mid(0, sepIndex).trimmed().toLower();
        QString value = line.mid(sepIndex + 1).trimmed();
        if (value.contains(DELIM)) return false;
        _contents.insert(tag, value);
    } while (!line.isNull());
    return true;
}

bool IniParser::load(QFile& file) {
    if (!file.exists()) return false;
    if (!file.isOpen()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    }
    QTextStream stream(&file);
    bool success = load(stream);
    file.close();
    return success;
}

bool IniParser::write(QFile& file) const {
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate)) return false;
    QTextStream stream(&file);
    stream << COMMENT << "This file was generated by a program, modify at your own risk" << endl;
    foreach (QString tag, _contents.keys()) {
        stream << tag << DELIM << _contents.value(tag) << endl;
    }
    file.close();
    return true;
}

bool IniParser::contains(const QString &tag) const {
    return _contents.contains(tag.toLower());
}

QString IniParser::value(const QString &tag) const {
    return _contents.value(tag.toLower(), "");
}

bool IniParser::valueAsInt(const QString &tag, int* value) const {
    bool success;
    *value = this->value(tag).toInt(&success);
    return success;
}

bool IniParser::valueAsIntRange(const QString &tag, int *values) const {
    bool success;
    QStringList numbers = value(tag).split("-");
    if (numbers.length() != 2) return false;
    values[0] = numbers[0].toInt(&success);
    if (!success) return false;
    values[1] = numbers[1].toInt(&success);
    return success;
}

bool IniParser::valueAsBool(const QString &tag, bool* value) const {
    QString rawValue = this->value(tag).toLower();
    if (rawValue == "true" || rawValue == "1") {
        *value = true;
        return true;
    }
    else if (rawValue == "false" || rawValue == "0") {
        *value = false;
        return true;
    }
    return false;
}

bool IniParser::valueAsIP(const QString &tag, QHostAddress* value, bool allowV6) const {
    QString rawValue = this->value(tag);
    if (QRegExp(IPV4_REGEX).exactMatch(rawValue) || (allowV6 && QRegExp(IPV6_REGEX).exactMatch(rawValue))) {
            value->setAddress(rawValue);
            return true;
    }
    return false;
}

int IniParser::count() const {
    return _contents.size();
}

bool IniParser::remove(const QString &tag) {
    return _contents.remove(tag.toLower()) > 0;
}

void IniParser::insert(const QString& tag, const QString& value) {
    _contents.insert(tag.toLower(), value);
}

QList<QString> IniParser::tags() const {
    return _contents.keys();
}

}
