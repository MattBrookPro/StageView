// Tests for the OSC codec (core/Osc).
//
// Deliberately framework-free: a CHECK macro that records failures and a non-zero
// exit code. For a unit this small that keeps the build dependency-light and the
// test trivially runnable in CI (ctest just runs the exe and reads its status).
// The interesting tests *pin the exact bytes* - that is what guarantees the C++
// surface and the Python engine agree on the wire, without a live round-trip.

#include "Osc.h"

#include <QByteArray>
#include <QVariant>
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Encode then decode should reproduce the address and every argument exactly.
// Floats survive bit-for-bit because encode/decode just byte-swap the same bits.
static void test_roundtrip_control()
{
    QByteArray bytes = osc::encode("/channel/3/level", {QVariant(0.75f)});
    osc::Message m;
    CHECK(osc::decode(bytes, m));
    CHECK(m.address == "/channel/3/level");
    CHECK(m.args.size() == 1);
    CHECK(m.args[0].toFloat() == 0.75f);
}

static void test_roundtrip_meters()
{
    QList<QVariant> args;
    for (int i = 0; i < 8; ++i)
        args.append(QVariant(static_cast<float>(i) / 8.0f));
    QByteArray bytes = osc::encode("/meters", args);
    osc::Message m;
    CHECK(osc::decode(bytes, m));
    CHECK(m.address == "/meters");
    CHECK(m.args.size() == 8);
    for (int i = 0; i < 8; ++i)
        CHECK(m.args[i].toFloat() == static_cast<float>(i) / 8.0f);
}

static void test_roundtrip_mixed()
{
    osc::Message m;
    QByteArray bytes = osc::encode("/say", {QVariant(7), QVariant(QString("hi")), QVariant(1.5f)});
    CHECK(osc::decode(bytes, m));
    CHECK(m.address == "/say");
    CHECK(m.args.size() == 3);
    CHECK(m.args[0].toInt() == 7);
    CHECK(m.args[1].toString() == "hi");
    CHECK(m.args[2].toFloat() == 1.5f);
}

// Pin the canonical bytes of an argument-less message. "/a" -> "/a\0\0", tag ","
// -> ",\0\0\0". Eight bytes, no surprises. If padding ever drifts, this fails.
static void test_pin_empty_message_bytes()
{
    QByteArray bytes = osc::encode("/a");
    const QByteArray expected = QByteArray::fromRawData("\x2F\x61\x00\x00\x2C\x00\x00\x00", 8);
    CHECK(bytes == expected);
}

// Pin float endianness: 440.0f is IEEE-754 0x43DC0000, big-endian on the wire.
// This is the exact check the Python codec mirrors, proving interop.
static void test_pin_float_bigendian()
{
    QByteArray bytes = osc::encode("/f", {QVariant(440.0f)});
    const QByteArray tail = bytes.right(4);
    const QByteArray expected = QByteArray::fromRawData("\x43\xDC\x00\x00", 4);
    CHECK(tail == expected);
}

// Networking code must reject short/garbage datagrams rather than over-read.
static void test_decode_rejects_garbage()
{
    osc::Message m;
    CHECK(!osc::decode(QByteArray("not-osc"), m));      // no leading '/', no nulls
    CHECK(!osc::decode(QByteArray(), m));               // empty
    QByteArray truncated = osc::encode("/x", {QVariant(1)});
    truncated.chop(2);                                  // lose part of the int arg
    CHECK(!osc::decode(truncated, m));
}

int main()
{
    test_roundtrip_control();
    test_roundtrip_meters();
    test_roundtrip_mixed();
    test_pin_empty_message_bytes();
    test_pin_float_bigendian();
    test_decode_rejects_garbage();

    if (g_failures == 0) {
        std::printf("osc_test: all checks passed\n");
        return 0;
    }
    std::printf("osc_test: %d check(s) failed\n", g_failures);
    return 1;
}
