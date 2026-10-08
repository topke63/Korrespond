// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagerenderer.h"

#include <KMime/Message>

#include <QTest>

using namespace Qt::StringLiterals;

// KMime needs LF line endings (see T7 notes), so raw test messages use "\n".
static QString render(const QByteArray &raw, const QColor &text = Qt::black, const QColor &background = Qt::white)
{
    KMime::Message msg;
    msg.setContent(raw);
    msg.parse();
    return messageBodyHtml(msg, text, background);
}

static QString cspMeta()
{
    return u"<meta http-equiv=\"Content-Security-Policy\" content=\""_s + QLatin1StringView(messageContentSecurityPolicy) + u"\">"_s;
}

class MessageRendererTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCspAlwaysPresent_data()
    {
        QTest::addColumn<QByteArray>("raw");
        QTest::newRow("plain") << QByteArray("Subject: a\nContent-Type: text/plain\n\nhello\n");
        QTest::newRow("html") << QByteArray("Subject: a\nContent-Type: text/html\n\n<p>hello</p>\n");
        QTest::newRow("no content-type") << QByteArray("Subject: a\n\nhello\n");
        QTest::newRow("empty body") << QByteArray("Subject: a\nContent-Type: text/plain\n\n");
        QTest::newRow("only attachment") << QByteArray(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=B\n\n"
            "--B\nContent-Type: application/pdf\nContent-Disposition: attachment; filename=a.pdf\n"
            "Content-Transfer-Encoding: base64\n\nJVBERg==\n--B--\n");
    }

    void testCspAlwaysPresent()
    {
        QFETCH(QByteArray, raw);
        const QString html = render(raw);
        QVERIFY2(html.contains(cspMeta()), qPrintable(html));
        // The policy must come before any content of the mail.
        QVERIFY(html.indexOf(cspMeta()) < html.indexOf(u"<body"_s) || !html.contains(u"<body"_s));
    }

    void testCspBlocksRemoteContent()
    {
        const QString policy = QLatin1StringView(messageContentSecurityPolicy);
        QVERIFY(policy.contains(u"default-src 'none'"_s));
        QVERIFY(!policy.contains(u"script-src"_s));
        QVERIFY(!policy.contains(u"http"_s));
    }

    void testPlainTextEscapesMarkup()
    {
        const QString html = render("Subject: a\nContent-Type: text/plain; charset=utf-8\n\n<script>alert(1)</script> & <b>bold</b>\n");
        QVERIFY(!html.contains(u"<script>"_s));
        QVERIFY(!html.contains(u"<b>"_s));
        QVERIFY(html.contains(u"&lt;script&gt;alert(1)&lt;/script&gt; &amp; &lt;b&gt;bold&lt;/b&gt;"_s));
    }

    void testPlainTextLinks_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QString>("expected");
        QTest::newRow("simple") << QByteArray("go to https://example.com now") << u"go to <a href=\"https://example.com\">https://example.com</a> now"_s;
        QTest::newRow("http") << QByteArray("http://example.org/x") << u"<a href=\"http://example.org/x\">http://example.org/x</a>"_s;
        QTest::newRow("query with &") << QByteArray("https://e.com/a?b=1&c=2")
                                      << u"<a href=\"https://e.com/a?b=1&amp;c=2\">https://e.com/a?b=1&amp;c=2</a>"_s;
        QTest::newRow("sentence dot") << QByteArray("see https://e.com/page.") << u"see <a href=\"https://e.com/page\">https://e.com/page</a>."_s;
        QTest::newRow("in parentheses") << QByteArray("(https://e.com/x)") << u"(<a href=\"https://e.com/x\">https://e.com/x</a>)"_s;
        QTest::newRow("in quotes") << QByteArray("\"https://e.com/q\"") << u"&quot;<a href=\"https://e.com/q\">https://e.com/q</a>&quot;"_s;
        QTest::newRow("in angle brackets") << QByteArray("<https://e.com/a>") << u"&lt;<a href=\"https://e.com/a\">https://e.com/a</a>&gt;"_s;
        QTest::newRow("two links") << QByteArray("https://a.com and https://b.com")
                                   << u"<a href=\"https://a.com\">https://a.com</a> and <a href=\"https://b.com\">https://b.com</a>"_s;
        QTest::newRow("no scheme") << QByteArray("www.example.com") << u"www.example.com"_s;
        QTest::newRow("javascript scheme") << QByteArray("javascript:alert(1)") << u"javascript:alert(1)"_s;
    }

    void testPlainTextLinks()
    {
        QFETCH(QByteArray, body);
        QFETCH(QString, expected);
        const QString html = render("Subject: a\nContent-Type: text/plain; charset=utf-8\n\n" + body + "\n");
        QVERIFY2(html.contains(expected), qPrintable(html));
    }

    void testPlainTextColors()
    {
        const QString html = render("Subject: a\nContent-Type: text/plain\n\nhi\n", QColor(0x11, 0x22, 0x33), QColor(0xaa, 0xbb, 0xcc));
        QVERIFY(html.contains(u"color:#112233"_s));
        QVERIFY(html.contains(u"background:#aabbcc"_s));
        QVERIFY(html.contains(u"white-space:pre-wrap"_s));
    }

    void testNoContentTypeIsPlainText()
    {
        const QString html = render("Subject: a\n\n<i>not html</i>\n");
        QVERIFY(html.contains(u"&lt;i&gt;not html&lt;/i&gt;"_s));
    }

    void testAlternativePicksHtml()
    {
        const QString html = render(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/alternative; boundary=B\n\n"
            "--B\nContent-Type: text/plain\n\nPLAIN VERSION\n"
            "--B\nContent-Type: text/html\n\n<p>HTML VERSION</p>\n--B--\n");
        QVERIFY(html.contains(u"<p>HTML VERSION</p>"_s));
        QVERIFY(!html.contains(u"PLAIN VERSION"_s));
        QVERIFY(html.startsWith(cspMeta()));
    }

    void testMixedWithAlternativePicksHtml()
    {
        const QString html = render(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=M\n\n"
            "--M\nContent-Type: multipart/alternative; boundary=A\n\n"
            "--A\nContent-Type: text/plain\n\nPLAIN VERSION\n"
            "--A\nContent-Type: text/html\n\n<p>HTML VERSION</p>\n--A--\n"
            "--M\nContent-Type: application/pdf\nContent-Disposition: attachment; filename=a.pdf\n"
            "Content-Transfer-Encoding: base64\n\nJVBERg==\n--M--\n");
        QVERIFY2(html.contains(u"<p>HTML VERSION</p>"_s), qPrintable(html));
    }

    void testHtmlAttachmentIsNotTheBody()
    {
        // An attached .html file must not replace the plain-text body.
        const QString html = render(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=M\n\n"
            "--M\nContent-Type: text/plain\n\nTHE BODY\n"
            "--M\nContent-Type: text/html\nContent-Disposition: attachment; filename=page.html\n\n<p>ATTACHED</p>\n--M--\n");
        QVERIFY2(html.contains(u"THE BODY"_s), qPrintable(html));
        QVERIFY(!html.contains(u"<p>ATTACHED</p>"_s));
    }

    void testQuotedPrintableLatin1()
    {
        const QString html = render("Subject: a\nContent-Type: text/plain; charset=iso-8859-1\nContent-Transfer-Encoding: quoted-printable\n\n"
                                    "caf=E9 na=EFve =\nsoft break\n");
        QVERIFY2(html.contains(u"café naïve soft break"_s), qPrintable(html));
    }

    void testBase64Windows1252Html()
    {
        // "<p>€ – ok</p>" in windows-1252, base64 encoded
        const QByteArray body = QByteArray("<p>\x80 \x96 ok</p>").toBase64();
        const QString html = render("Subject: a\nContent-Type: text/html; charset=windows-1252\nContent-Transfer-Encoding: base64\n\n" + body + "\n");
        QVERIFY2(html.contains(u"<p>€ – ok</p>"_s), qPrintable(html));
    }

    void testUtf8Plain()
    {
        const QString html = render("Subject: a\nContent-Type: text/plain; charset=utf-8\nContent-Transfer-Encoding: 8bit\n\nЗдраво, свете\n");
        QVERIFY(html.contains(u"Здраво, свете"_s));
    }
};

QTEST_GUILESS_MAIN(MessageRendererTest)
#include "messagerenderertest.moc"
