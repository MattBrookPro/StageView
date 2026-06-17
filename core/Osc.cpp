#include "Osc.h"

#include <QtEndian>
#include <cstring>

namespace osc {
namespace {

// OSC aligns every block to a 4-byte boundary. This returns how many padding
// bytes are needed after a block of length `len`.
int padTo4(int len)
{
    return (4 - (len % 4)) % 4;
}

// Append an OSC string: the bytes, at least one terminating null, then nulls to
// the next 4-byte boundary. ("a" -> 'a' \0 \0 \0).
void appendOscString(QByteArray &out, const QByteArray &str)
{
    out.append(str);
    out.append('\0');
    // After adding the mandatory null, pad the *whole* field (str + null) to 4.
    const int field = str.size() + 1;
    out.append(QByteArray(padTo4(field), '\0'));
}

// int32 and float32 go big-endian ("network byte order"). For the float we copy
// its bits into a uint, byte-swap that, then emit - the standard, portable way to
// serialise IEEE-754 without relying on float endianness helpers.
void appendInt32(QByteArray &out, qint32 v)
{
    qint32 be = qToBigEndian(v);
    out.append(reinterpret_cast<const char *>(&be), 4);
}

void appendFloat32(QByteArray &out, float v)
{
    quint32 bits;
    std::memcpy(&bits, &v, 4);
    quint32 be = qToBigEndian(bits);
    out.append(reinterpret_cast<const char *>(&be), 4);
}

// --- decode helpers: every read is bounds-checked against `data` ------------

bool readOscString(const QByteArray &data, int &pos, QString &out)
{
    const int start = pos;
    while (pos < data.size() && data[pos] != '\0')
        ++pos;
    if (pos >= data.size())
        return false; // no terminating null -> malformed
    out = QString::fromUtf8(data.constData() + start, pos - start);
    const int field = (pos - start) + 1; // include the null we stopped on
    pos = start + field + padTo4(field);
    return pos <= data.size();
}

bool readInt32(const QByteArray &data, int &pos, qint32 &out)
{
    if (pos + 4 > data.size())
        return false;
    qint32 be;
    std::memcpy(&be, data.constData() + pos, 4);
    out = qFromBigEndian(be);
    pos += 4;
    return true;
}

bool readFloat32(const QByteArray &data, int &pos, float &out)
{
    if (pos + 4 > data.size())
        return false;
    quint32 be;
    std::memcpy(&be, data.constData() + pos, 4);
    quint32 bits = qFromBigEndian(be);
    std::memcpy(&out, &bits, 4);
    pos += 4;
    return true;
}

} // namespace

QByteArray encode(const QString &address, const QList<QVariant> &args)
{
    QByteArray out;
    appendOscString(out, address.toUtf8());

    // Build the type-tag string first (",iff" etc.), then append the arg data in
    // the same order. Keeping the two loops separate makes the format obvious.
    QByteArray tags(",");
    for (const QVariant &a : args) {
        switch (a.metaType().id()) {
        case QMetaType::Int:
        case QMetaType::LongLong:
            tags.append('i');
            break;
        case QMetaType::Float:
        case QMetaType::Double:
            tags.append('f');
            break;
        case QMetaType::QString:
            tags.append('s');
            break;
        default:
            tags.append('\0'); // unsupported; tests assert we never hit this
            break;
        }
    }
    appendOscString(out, tags);

    for (const QVariant &a : args) {
        switch (a.metaType().id()) {
        case QMetaType::Int:
        case QMetaType::LongLong:
            appendInt32(out, a.toInt());
            break;
        case QMetaType::Float:
        case QMetaType::Double:
            appendFloat32(out, a.toFloat());
            break;
        case QMetaType::QString:
            appendOscString(out, a.toString().toUtf8());
            break;
        default:
            break;
        }
    }
    return out;
}

QByteArray encode(const Message &msg)
{
    return encode(msg.address, msg.args);
}

bool decode(const QByteArray &data, Message &out)
{
    int pos = 0;
    Message msg;

    if (!readOscString(data, pos, msg.address))
        return false;
    if (!msg.address.startsWith('/'))
        return false; // OSC addresses always begin with '/'

    QString tags;
    if (!readOscString(data, pos, tags))
        return false;
    if (!tags.startsWith(','))
        return false;

    for (int i = 1; i < tags.size(); ++i) {
        switch (tags.at(i).toLatin1()) {
        case 'i': {
            qint32 v;
            if (!readInt32(data, pos, v))
                return false;
            msg.args.append(QVariant(v));
            break;
        }
        case 'f': {
            float v;
            if (!readFloat32(data, pos, v))
                return false;
            msg.args.append(QVariant(v));
            break;
        }
        case 's': {
            QString v;
            if (!readOscString(data, pos, v))
                return false;
            msg.args.append(QVariant(v));
            break;
        }
        default:
            return false; // a tag we don't understand -> reject the whole message
        }
    }

    out = std::move(msg);
    return true;
}

} // namespace osc
