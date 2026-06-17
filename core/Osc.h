#pragma once

// A small, self-contained OSC 1.0 codec.
//
// WHY we implement OSC ourselves instead of pulling a library:
//  - OSC is the control protocol pro-audio surfaces speak, so owning the wire
//    format is the point of the exercise, not a chore.
//  - The format is tiny - addresses, a type-tag string, and 4-byte-aligned args -
//    so a hand-written codec is short, dependency-free, and fully explainable.
//  - It lives in `core` with no UI/QML dependency, so it is unit-testable in
//    isolation (see tests/osc_test.cpp). Decoupling the protocol from the GUI is
//    the same "make the core testable" discipline applied to networking.
//
// Wire format recap (OSC 1.0):
//   [address string]   null-terminated, padded with nulls to a multiple of 4
//   [type-tag string]  ',' then one tag per arg ('i','f','s'), null-term + padded
//   [arguments]        in order; int32 and float32 are big-endian
//
// We support the three argument types this app needs: int32 ('i'), float32 ('f')
// and string ('s'). That is enough for "/channel/3/level 0.75" style control and
// "/meters f f f ..." style feedback.

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariant>

namespace osc {

// A decoded (or to-be-encoded) OSC message: an address pattern and its typed
// arguments. Args carry int (->'i'), float/double (->'f') or QString (->'s').
struct Message {
    QString address;
    QList<QVariant> args;
};

// Encode an address + args into an OSC datagram. Unsupported QVariant types are
// skipped (and would show up as a mismatch between the tag string and the data,
// which the tests guard against), so callers should only pass int/float/string.
QByteArray encode(const QString &address, const QList<QVariant> &args = {});
QByteArray encode(const Message &msg);

// Decode a single OSC message from a datagram. Returns false (and leaves `out`
// untouched) if the data is malformed or runs short - networking code must never
// trust the bytes on the wire.
bool decode(const QByteArray &data, Message &out);

} // namespace osc
