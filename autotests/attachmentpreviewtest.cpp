// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "attachmentpreview.h"

using namespace Qt::StringLiterals;
using AttachmentPreview::Kind;

class AttachmentPreviewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testKindFor_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("mime");
        QTest::addColumn<Kind>("kind");

        QTest::newRow("png") << u"a.png"_s << u"image/png"_s << Kind::Image;
        QTest::newRow("svg") << u"logo.svg"_s << u"image/svg+xml"_s << Kind::Image;
        QTest::newRow("pdf") << u"invoice.pdf"_s << u"application/pdf"_s << Kind::Pdf;
        QTest::newRow("text") << u"notes.txt"_s << u"text/plain"_s << Kind::Text;
        QTest::newRow("csv") << u"data.csv"_s << u"text/csv"_s << Kind::Text;
        QTest::newRow("json type") << u"x"_s << u"application/json"_s << Kind::Text;
        QTest::newRow("mp3") << u"song.mp3"_s << u"audio/mpeg"_s << Kind::Audio;
        QTest::newRow("mp4") << u"clip.mp4"_s << u"video/mp4"_s << Kind::Video;
        // The declared type is missing or generic: the suffix decides, in any case.
        QTest::newRow("octet-stream png") << u"Photo.PNG"_s << u"application/octet-stream"_s << Kind::Image;
        QTest::newRow("empty type pdf") << u"scan.Pdf"_s << QString() << Kind::Pdf;
        QTest::newRow("octet-stream log") << u"server.log"_s << u"application/octet-stream"_s << Kind::Text;
        QTest::newRow("octet-stream mkv") << u"film.mkv"_s << u"application/octet-stream"_s << Kind::Video;
        QTest::newRow("upper-case type") << u"a"_s << u"IMAGE/JPEG"_s << Kind::Image;
        // A type that says image beats a wrong suffix.
        QTest::newRow("type wins") << u"a.bin"_s << u"image/gif"_s << Kind::Image;
        // No preview.
        QTest::newRow("zip") << u"archive.zip"_s << u"application/zip"_s << Kind::None;
        QTest::newRow("docx") << u"letter.docx"_s << u"application/vnd.openxmlformats-officedocument.wordprocessingml.document"_s << Kind::None;
        QTest::newRow("unknown") << u"thing"_s << u"application/octet-stream"_s << Kind::None;
        QTest::newRow("trailing dot") << u"thing."_s << QString() << Kind::None;
        QTest::newRow("empty") << QString() << QString() << Kind::None;
    }

    void testKindFor()
    {
        QFETCH(QString, name);
        QFETCH(QString, mime);
        QFETCH(Kind, kind);
        QCOMPARE(AttachmentPreview::kindFor(name, mime), kind);
    }

    void testTextUtf8AndBom()
    {
        const auto result = AttachmentPreview::textFor(
            "\xEF\xBB\xBF"
            "Zdravo \xC4\x8D\xC4\x87\xC5\xA1");
        QVERIFY(!result.truncated);
        QCOMPARE(result.text, QString::fromUtf8("Zdravo \xC4\x8D\xC4\x87\xC5\xA1"));
    }

    void testTextFallsBackToLatin1()
    {
        // 0xE9 alone is not valid UTF-8.
        const auto result = AttachmentPreview::textFor("caf\xE9");
        QCOMPARE(result.text, QString::fromUtf8("caf\xC3\xA9"));
    }

    void testTextLineEndsAndControlCharacters()
    {
        const auto result = AttachmentPreview::textFor(QByteArray("a\r\nb\rc\td\0e\x01", 11));
        QCOMPARE(result.text, u"a\nb\nc\td\uFFFDe\uFFFD"_s);
    }

    void testTextTruncatesOnACharacterBoundary()
    {
        // "aaa" + e-acute (2 bytes) + "b": cutting after 4 bytes would split the e-acute.
        const QByteArray data =
            "aaa\xC3\xA9"
            "b";
        auto result = AttachmentPreview::textFor(data, 4);
        QVERIFY(result.truncated);
        QCOMPARE(result.text, u"aaa"_s);
        result = AttachmentPreview::textFor(data, 5);
        QVERIFY(result.truncated);
        QCOMPARE(result.text, QString::fromUtf8("aaa\xC3\xA9"));
        // A four-byte character (emoji) cut in the middle.
        result = AttachmentPreview::textFor("a\xF0\x9F\x91\x8D", 3);
        QCOMPARE(result.text, u"a"_s);
        // Not over the limit: nothing is cut.
        result = AttachmentPreview::textFor(data, 6);
        QVERIFY(!result.truncated);
        QCOMPARE(result.text,
                 QString::fromUtf8("aaa\xC3\xA9"
                                   "b"));
    }

    void testTextEmpty()
    {
        const auto result = AttachmentPreview::textFor(QByteArray());
        QVERIFY(result.text.isEmpty());
        QVERIFY(!result.truncated);
    }

    void testKindName()
    {
        QCOMPARE(AttachmentPreview::kindName(Kind::Image), u"image"_s);
        QCOMPARE(AttachmentPreview::kindName(Kind::Pdf), u"pdf"_s);
        QCOMPARE(AttachmentPreview::kindName(Kind::Text), u"text"_s);
        QCOMPARE(AttachmentPreview::kindName(Kind::Audio), u"audio"_s);
        QCOMPARE(AttachmentPreview::kindName(Kind::Video), u"video"_s);
        QCOMPARE(AttachmentPreview::kindName(Kind::None), u"none"_s);
    }
};

QTEST_GUILESS_MAIN(AttachmentPreviewTest)

#include "attachmentpreviewtest.moc"
