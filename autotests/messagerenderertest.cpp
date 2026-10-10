// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "messagerenderer.h"

#include <KMime/Message>

#include <QTest>

using namespace Qt::StringLiterals;

// KMime needs LF line endings (see T7 notes), so raw test messages use "\n".
static QString
render(const QByteArray &raw, const QColor &text = Qt::black, const QColor &background = Qt::white, const QColor &link = QColor(0x12, 0x34, 0x56))
{
    KMime::Message msg;
    msg.setContent(raw);
    msg.parse();
    return messageBodyHtml(msg, text, background, link);
}

static QString cspMeta(bool allowRemoteImages = false)
{
    return u"<meta http-equiv=\"Content-Security-Policy\" content=\""_s + messageContentSecurityPolicy(allowRemoteImages) + u"\">"_s;
}

static QString preview(const QByteArray &raw, int maxLength = 150)
{
    KMime::Message msg;
    msg.setContent(raw);
    msg.parse();
    return messagePreview(msg, maxLength);
}

class MessageRendererTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testHtmlColours_data()
    {
        QTest::addColumn<QString>("html");
        QTest::addColumn<bool>("own");
        QTest::newRow("none") << u"<p>hi</p>"_s << false;
        QTest::newRow("style color") << u"<p style=\"color:#333\">hi</p>"_s << true;
        QTest::newRow("style uppercase") << u"<p STYLE=\"COLOR : #333\">hi</p>"_s << true;
        QTest::newRow("bgcolor") << u"<table bgcolor=\"#eee\"><tr><td>x</td></tr></table>"_s << true;
        QTest::newRow("font color") << u"<font color=\"red\">x</font>"_s << true;
        QTest::newRow("body text") << u"<body text=\"#000\">x</body>"_s << true;
        QTest::newRow("border-color only") << u"<p style=\"border-color:red\">x</p>"_s << false;
        QTest::newRow("text-decoration-color only") << u"<p style=\"text-decoration-color:red\">x</p>"_s << false;
        QTest::newRow("style block background") << u"<style>p{background:#fff}</style><p>x</p>"_s << true;
        QTest::newRow("background-color") << u"<p style=\"background-color:#fff\">x</p>"_s << true;
    }
    void testHtmlColours()
    {
        QFETCH(QString, html);
        QFETCH(bool, own);
        QCOMPARE(htmlHasOwnColours(html), own);
        const QString out = render("Subject: a\nContent-Type: text/html\n\n" + html.toUtf8() + "\n", QColor("#abcdef"), QColor("#102030"), QColor("#fedcba"));
        QVERIFY(out.startsWith(cspMeta() + u"<style>"_s));
        QVERIFY(out.contains(html));
        if (own) {
            QVERIFY(out.contains(u"<style>html{background:#fff;color:#000}</style>"_s));
            QVERIFY(!out.contains(u"#abcdef"_s));
            QVERIFY(!out.contains(u"#102030"_s));
        } else {
            QVERIFY(out.contains(u"#abcdef"_s));
            QVERIFY(out.contains(u"#102030"_s));
            QVERIFY(out.contains(u"a{color:#fedcba}"_s));
            QVERIFY(out.contains(u"blockquote{border-left:2px solid"_s));
        }
    }
    void testLinkContrast_data()
    {
        QTest::addColumn<QColor>("link");
        QTest::addColumn<QColor>("background");
        QTest::addColumn<bool>("unchanged");
        QTest::newRow("blue on white") << QColor("#0645ad") << QColor("#ffffff") << true;
        QTest::newRow("dark blue on dark") << QColor("#00008b") << QColor("#1b1e20") << false;
        QTest::newRow("pale on white") << QColor("#e0e0ff") << QColor("#ffffff") << false;
        QTest::newRow("same as bg dark") << QColor("#202020") << QColor("#202020") << false;
        QTest::newRow("same as bg light") << QColor("#f0f0f0") << QColor("#f0f0f0") << false;
    }
    void testLinkContrast()
    {
        QFETCH(QColor, link);
        QFETCH(QColor, background);
        QFETCH(bool, unchanged);
        const QColor result = readableLinkColour(link, background);
        QVERIFY(contrastRatio(result, background) >= 4.5);
        QCOMPARE(result == link, unchanged);
    }
    void testLinkFollowsBase()
    {
        // A dark base lightens a too-dark link, a light base darkens a too-pale one, both to 4.5:1.
        const QColor dark("#1b1e20");
        const QColor light("#fcfcfc");
        const QColor onDark = readableLinkColour(QColor("#1d3f8f"), dark);
        QVERIFY(contrastRatio(onDark, dark) >= 4.5);
        QVERIFY(onDark.lightness() > QColor("#1d3f8f").lightness());
        const QColor onLight = readableLinkColour(QColor("#a8c8ff"), light);
        QVERIFY(contrastRatio(onLight, light) >= 4.5);
        QVERIFY(onLight.lightness() < QColor("#a8c8ff").lightness());
    }
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

    void testDoctypeHoisting_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<bool>("hoisted");
        QTest::addColumn<QString>("after"); // must appear after the policy
        const QString xhtml = QString::fromUtf8(
            "\xEF\xBB\xBF  \n<!doctype HTML PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
            "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\"><html><p>x</p></html>");
        QTest::newRow("plain doctype") << QByteArray("<!DOCTYPE html><p>x</p>") << true << u"<p>x</p>"_s;
        QTest::newRow("bom, space, xhtml doctype") << xhtml.toUtf8() << true << u"<html>"_s;
        QTest::newRow("comment first") << QByteArray("<!-- c --><!DOCTYPE html><p>x</p>") << false << u"<!-- c -->"_s;
        QTest::newRow("script first") << QByteArray("<script>alert(1)</script><!DOCTYPE html>") << false << u"<script"_s;
        QTest::newRow("img first") << QByteArray("<img src=https://x.invalid/a.png><!DOCTYPE html>") << false << u"<img"_s;
        QTest::newRow("meta first") << QByteArray("<meta http-equiv=\"Content-Security-Policy\" content=\"default-src *\"><!DOCTYPE html>") << false
                                    << u"<meta http-equiv=\"Content-Security-Policy\" content=\"default-src *\">"_s;
        QTest::newRow("text first") << QByteArray("x<!DOCTYPE html>") << false << u"x<!DOCTYPE"_s;
        QTest::newRow("lt inside doctype") << QByteArray("<!DOCTYPE html<img src=x>>") << false << u"<img"_s;
        QTest::newRow("second doctype") << QByteArray("<!DOCTYPE html><!DOCTYPE html><img src=x>") << true << u"<img"_s;
        QTest::newRow("not html doctype") << QByteArray("<!DOCTYPE svg><p>x</p>") << false << u"<!DOCTYPE svg>"_s;
        QTest::newRow("nul first") << QByteArray("\0<!DOCTYPE html><img src=x>", 27) << false << u"<img"_s;
        QTest::newRow("quoted gt in doctype") << QByteArray("<!DOCTYPE html PUBLIC \"a>b\"><img src=x>") << true << u"b\"><img"_s;
        QTest::newRow("cr, ff, tab, space first") << QByteArray("\r\f\n\t <!DOCTYPE html><p>x</p>") << true << u"<p>x</p>"_s;
        QTest::newRow("nul inside doctype") << QByteArray("<!DOCTYPE html\0><img src=x>", 28) << true << u"<img"_s;
        QTest::newRow("unterminated doctype, long") << QByteArray("<!DOCTYPE html ") + QByteArray(1000000, 'x') + QByteArray("<p>x</p>") << false
                                                    << u"<p>x</p>"_s;
        QTest::newRow("long whitespace before doctype") << QByteArray(1000000, ' ') + QByteArray("<!DOCTYPE html><p>x</p>") << true << u"<p>x</p>"_s;
    }

    void testDoctypeHoisting()
    {
        QFETCH(QByteArray, body);
        QFETCH(bool, hoisted);
        QFETCH(QString, after);
        const QString prefix = u"<!DOCTYPE html>"_s;
        for (const bool remote : {false, true}) {
            KMime::Message msg;
            msg.setContent("Subject: a\nContent-Type: text/html; charset=utf-8\n\n" + body);
            msg.parse();
            const QString out = messageBodyHtml(msg, Qt::black, Qt::white, Qt::blue, remote);
            const QString csp = cspMeta(remote);
            const qsizetype cspAt = out.indexOf(csp);
            QVERIFY2(cspAt == (hoisted ? prefix.size() : 0), qPrintable(out));
            if (hoisted) {
                QVERIFY2(out.startsWith(prefix + csp), qPrintable(out));
                // The mail's own doctype text is not copied; only ours precedes the policy.
                QCOMPARE(out.left(cspAt), prefix);
            }
            QVERIFY2(out.indexOf(after, cspAt + csp.size()) >= 0, qPrintable(out));
            // Nothing active from the mail precedes the policy.
            const QString before = out.left(cspAt);
            QVERIFY(!before.contains(u"<script"_s) && !before.contains(u"<img"_s) && !before.contains(u"<meta"_s));
            QCOMPARE(out.count(csp), 1);
        }
    }

    void testCspBlocksRemoteContent()
    {
        const QString policy = messageContentSecurityPolicy();
        QVERIFY(policy.contains(u"default-src 'none'"_s));
        QVERIFY(!policy.contains(u"script-src"_s));
        QVERIFY(!policy.contains(u"http"_s));
    }

    void testCspPolicyStrings()
    {
        QCOMPARE(messageContentSecurityPolicy(), u"default-src 'none'; style-src 'unsafe-inline'; img-src data: cid:"_s);
        QCOMPARE(messageContentSecurityPolicy(false), messageContentSecurityPolicy());
        const QString on = messageContentSecurityPolicy(true);
        QCOMPARE(on, u"default-src 'none'; style-src 'unsafe-inline'; img-src data: cid: https:"_s);
        const QStringList tokens = on.split(QRegularExpression(u"[\\s;]+"_s), Qt::SkipEmptyParts);
        QVERIFY(tokens.contains(u"https:"_s));
        QVERIFY(!tokens.contains(u"http:"_s));
        QVERIFY(!tokens.contains(u"*"_s));
        for (const QString &directive : {u"script-src"_s, u"frame-src"_s, u"connect-src"_s, u"font-src"_s, u"media-src"_s, u"object-src"_s}) {
            QVERIFY(!on.contains(directive));
        }
        QVERIFY(on.contains(u"default-src 'none';"_s));
        QVERIFY(on.contains(u"style-src 'unsafe-inline';"_s));
        QVERIFY(!messageContentSecurityPolicy().contains(u"https:"_s));
    }

    void testRemoteImagesAllowedInBody()
    {
        const QByteArray html = "Subject: a\nContent-Type: text/html\n\n<img src=\"https://x.invalid/a.png\">\n";
        KMime::Message msg;
        msg.setContent(html);
        msg.parse();
        const QString on = messageBodyHtml(msg, Qt::black, Qt::white, Qt::blue, true);
        QVERIFY(on.startsWith(cspMeta(true)));
        QVERIFY(on.contains(u"img-src data: cid: https:\""_s));
        const QString off = messageBodyHtml(msg, Qt::black, Qt::white, Qt::blue);
        QVERIFY(off.startsWith(cspMeta()));
        QVERIFY(!off.contains(u"img-src data: cid: https:"_s));

        KMime::Message plain;
        plain.setContent("Subject: a\nContent-Type: text/plain\n\nhi\n");
        plain.parse();
        QCOMPARE(messageBodyHtml(plain, Qt::black, Qt::white, Qt::blue, true), messageBodyHtml(plain, Qt::black, Qt::white, Qt::blue));
    }

    void testRemoteImages_data()
    {
        QTest::addColumn<QString>("html");
        QTest::addColumn<bool>("expected");
        QTest::newRow("img https") << u"<img src=\"https://x.invalid/a.png\">"_s << true;
        QTest::newRow("img unquoted upper") << u"<IMG SRC=http://x.invalid/a.png>"_s << true;
        QTest::newRow("css url") << u"<div style=\"background:url('https://x.invalid/b.png')\">"_s << true;
        QTest::newRow("background attr") << u"<td background=\"https://x.invalid/c.png\">"_s << true;
        QTest::newRow("protocol relative") << u"<img src=\"//x.invalid/a.png\">"_s << true;
        QTest::newRow("srcset") << u"<img srcset=\"https://x.invalid/a.png 2x\">"_s << true;
        QTest::newRow("picture source") << u"<picture><source srcset=\"https://x.invalid/a.webp\"></picture>"_s << true;
        QTest::newRow("input image") << u"<input type=image src=https://x.invalid/a.png>"_s << true;
        QTest::newRow("cid") << u"<img src=\"cid:abc@x\">"_s << false;
        QTest::newRow("data") << u"<img src=\"data:image/png;base64,AAAA\">"_s << false;
        QTest::newRow("text only") << u"<p>see https://x.invalid</p>"_s << false;
        QTest::newRow("link") << u"<a href=\"https://x.invalid\">x</a>"_s << false;
    }

    void testRemoteImages()
    {
        QFETCH(QString, html);
        QFETCH(bool, expected);
        QCOMPARE(htmlHasRemoteImages(html), expected);
    }

    void testMessageHasRemoteImages_data()
    {
        QTest::addColumn<QByteArray>("raw");
        QTest::addColumn<bool>("expected");
        QTest::newRow("html") << QByteArray("Subject: a\nContent-Type: text/html\n\n<img src=\"https://x.invalid/a.png\">\n") << true;
        QTest::newRow("plain with url") << QByteArray("Subject: a\nContent-Type: text/plain\n\nsee https://x.invalid/a.png\n") << false;
        QTest::newRow("html no images") << QByteArray("Subject: a\nContent-Type: text/html\n\n<p>Hello</p>\n") << false;
        QTest::newRow("alternative") << QByteArray(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/alternative; boundary=B\n\n"
            "--B\nContent-Type: text/plain\n\nHello\n"
            "--B\nContent-Type: text/html\n\n<p>Hello</p><img src=\"https://x.invalid/pixel.png\" alt=\"x\">\n--B--\n")
                                     << true;
        QTest::newRow("related remote") << QByteArray(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/related; boundary=R\n\n"
            "--R\nContent-Type: text/html\n\n<img src=\"https://x.invalid/a.png\">\n"
            "--R\nContent-Type: image/png\nContent-ID: <a@x>\nContent-Transfer-Encoding: base64\n\nAAAA\n--R--\n")
                                        << true;
        QTest::newRow("related cid only") << QByteArray(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/related; boundary=R\n\n"
            "--R\nContent-Type: text/html\n\n<img src=\"cid:a@x\">\n"
            "--R\nContent-Type: image/png\nContent-ID: <a@x>\nContent-Transfer-Encoding: base64\n\nAAAA\n--R--\n")
                                          << false;
        QTest::newRow("html attachment") << QByteArray(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=M\n\n"
            "--M\nContent-Type: text/plain\n\nTHE BODY\n"
            "--M\nContent-Type: text/html\nContent-Disposition: attachment; filename=page.html\n\n<img src=\"https://x.invalid/a.png\">\n--M--\n")
                                         << false;
    }

    void testMessageHasRemoteImages()
    {
        QFETCH(QByteArray, raw);
        QFETCH(bool, expected);
        KMime::Message msg;
        msg.setContent(raw);
        msg.parse();
        QCOMPARE(messageHasRemoteImages(msg), expected);
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
        QTest::newRow("query with &") << QByteArray("https://e.com/a?b=1&c=2") << u"<a href=\"https://e.com/a?b=1&amp;c=2\">https://e.com/a?b=1&amp;c=2</a>"_s;
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
        const QString html = render(
            "Subject: a\nContent-Type: text/plain; charset=iso-8859-1\nContent-Transfer-Encoding: quoted-printable\n\n"
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

    void testAttachmentNames_data()
    {
        QTest::addColumn<QByteArray>("raw");
        QTest::addColumn<QStringList>("expected");

        const QByteArray head = "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=M\n\n--M\nContent-Type: text/plain\n\nbody\n";
        const QByteArray tail = "--M--\n";
        const QByteArray pdf = "Content-Transfer-Encoding: base64\n\nJVBERg==\n";

        QTest::newRow("none") << QByteArray("Subject: a\nContent-Type: text/plain\n\nbody\n") << QStringList();
        QTest::newRow("disposition filename") << head + "--M\nContent-Type: application/pdf\nContent-Disposition: attachment; filename=\"report.pdf\"\n" + pdf
                + tail << QStringList{u"report.pdf"_s};
        QTest::newRow("content-type name only") << head + "--M\nContent-Type: application/pdf; name=\"invoice.pdf\"\nContent-Disposition: attachment\n" + pdf
                + tail << QStringList{u"invoice.pdf"_s};
        QTest::newRow("filename wins over name") << head
                + "--M\nContent-Type: application/pdf; name=\"type.pdf\"\nContent-Disposition: attachment; filename=\"disp.pdf\"\n" + pdf
                + tail << QStringList{u"disp.pdf"_s};
        QTest::newRow("unnamed") << head + "--M\nContent-Type: application/octet-stream\nContent-Disposition: attachment\n" + pdf + tail
                                 << QStringList{u"Unnamed attachment"_s};
        QTest::newRow("named and unnamed, in order") << head + "--M\nContent-Type: application/pdf\nContent-Disposition: attachment; filename=a.pdf\n" + pdf
                + "--M\nContent-Type: application/octet-stream\nContent-Disposition: attachment\n" + pdf
                + tail << QStringList{u"a.pdf"_s, u"Unnamed attachment"_s};
        QTest::newRow("RFC 2231 filename") << head
                + "--M\nContent-Type: application/pdf\nContent-Disposition: attachment; filename*=UTF-8''%D0%B8%D0%B7%D0%B2%D0%B5%D1%88%D1%82%D0%B0%D1%98.pdf\n"
                + pdf + tail << QStringList{u"извештај.pdf"_s};
        QTest::newRow("RFC 2047 name") << head + "--M\nContent-Type: application/pdf; name=\"=?UTF-8?B?Y2Fmw6kucGRm?=\"\nContent-Disposition: attachment\n"
                + pdf + tail << QStringList{u"café.pdf"_s};
        QTest::newRow("alternative bodies are not attachments") << QByteArray(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/alternative; boundary=B\n\n"
            "--B\nContent-Type: text/plain\n\nplain\n--B\nContent-Type: text/html\n\n<p>html</p>\n--B--\n")
                                                                << QStringList();
    }

    void testAttachmentNames()
    {
        QFETCH(QByteArray, raw);
        QFETCH(QStringList, expected);
        KMime::Message msg;
        msg.setContent(raw);
        msg.parse();
        QCOMPARE(attachmentNames(msg), expected);
    }

    void testAttachmentParts_data()
    {
        QTest::addColumn<QByteArray>("raw");
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("mime");
        QTest::addColumn<QByteArray>("data");

        const QByteArray head = "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=M\n\n--M\nContent-Type: text/plain\n\nbody\n";
        const QByteArray tail = "--M--\n";
        const QByteArray b64 = "Content-Transfer-Encoding: base64\n\nSGVsbG8sIHdvcmxkIQ==\n";
        const QByteArray hello = "Hello, world!";
        auto row = [&](const char *tag, const QByteArray &partHead, const QString &name, const QString &mime) {
            QTest::newRow(tag) << head + "--M\n" + partHead + b64 + tail << name << mime << hello;
        };
        row("base64 text", "Content-Type: text/plain\nContent-Disposition: attachment; filename=\"hello.txt\"\n", u"hello.txt"_s, u"text/plain"_s);
        row("parent dirs", "Content-Type: text/plain\nContent-Disposition: attachment; filename=\"../../etc/passwd\"\n", u"passwd"_s, u"text/plain"_s);
        row("slash and backslash", "Content-Type: text/plain\nContent-Disposition: attachment; filename=\"a/b\\\\c.txt\"\n", u"c.txt"_s, u"text/plain"_s);
        row("control chars",
            "Content-Type: text/plain\nContent-Disposition: attachment; filename*=UTF-8''%20a%0Ab%09c%7F.txt\n",
            u"abc.txt"_s,
            u"text/plain"_s);
        row("only dots", "Content-Type: text/plain\nContent-Disposition: attachment; filename=\"..\"\n", u"attachment"_s, u"text/plain"_s);
        row("unnamed", "Content-Type: application/octet-stream\nContent-Disposition: attachment\n", u"attachment"_s, u"application/octet-stream"_s);
        row("no content type", "Content-Disposition: attachment; filename=x.bin\n", u"x.bin"_s, u"text/plain"_s);
    }

    void testAttachmentParts()
    {
        QFETCH(QByteArray, raw);
        QFETCH(QString, name);
        QFETCH(QString, mime);
        QFETCH(QByteArray, data);
        KMime::Message msg;
        msg.setContent(raw);
        msg.parse();
        const auto parts = attachmentParts(msg);
        QCOMPARE(parts.size(), 1);
        QCOMPARE(parts[0].name, name);
        QCOMPARE(parts[0].mimeType, mime);
        QCOMPARE(parts[0].data, data);
    }

    void testAttachmentPartsNone()
    {
        KMime::Message msg;
        msg.setContent("Subject: a\nContent-Type: text/plain\n\nbody\n");
        msg.parse();
        QVERIFY(attachmentParts(msg).isEmpty());
    }

    void testAttachmentPartsOrder()
    {
        const QByteArray raw =
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=M\n\n--M\nContent-Type: text/plain\n\nbody\n"
            "--M\nContent-Type: application/pdf\nContent-Disposition: attachment; filename=a.pdf\n\nAAA\n"
            "--M\nContent-Type: text/plain\nContent-Disposition: attachment; filename=b.txt\n\nBBB\n--M--\n";
        KMime::Message msg;
        msg.setContent(raw);
        msg.parse();
        const auto parts = attachmentParts(msg);
        const QStringList names = attachmentNames(msg);
        QCOMPARE(parts.size(), 2);
        QCOMPARE(names.size(), 2);
        for (int i = 0; i < 2; ++i) {
            QCOMPARE(parts[i].name, names[i]);
        }
        QCOMPARE(parts[0].mimeType, u"application/pdf"_s);
        QCOMPARE(parts[1].mimeType, u"text/plain"_s);
    }

    void testRiskyAttachment_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("mime");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("risky");
        // Risky by file suffix, whatever the declared type says.
        QTest::newRow("shell script") << u"run.sh"_s << u"application/octet-stream"_s << QByteArray("echo hi\n") << true;
        QTest::newRow("windows upper case") << u"SETUP.EXE"_s << u"application/octet-stream"_s << QByteArray::fromHex("4d5a") << true;
        QTest::newRow("executable behind a document suffix") << u"invoice.pdf.exe"_s << u"application/pdf"_s << QByteArray("%PDF-1.4\n") << true;
        QTest::newRow("desktop entry") << u"app.desktop"_s << u"application/octet-stream"_s << QByteArray("[Desktop Entry]\n") << true;
        QTest::newRow("AppImage") << u"x.AppImage"_s << u"application/octet-stream"_s << QByteArray::fromHex("7f454c46") << true;
        QTest::newRow("python script declared as text") << u"script.py"_s << u"text/plain"_s << QByteArray("print(1)\n") << true;
        // Risky by detected type: no suffix, but a "#!" line (checked by the sniffing below).
        QTest::newRow("shebang without suffix") << u"script"_s << u"application/octet-stream"_s << QByteArray("#!/bin/sh\necho hi\n") << true;
        // Risky by declared type alone (the content is plain text), under a harmless name.
        QTest::newRow("declared windows program") << u"file.dat"_s << u"application/x-msdownload"_s << QByteArray("not a program\n") << true;
        // Not risky.
        QTest::newRow("photo") << u"photo.jpg"_s << u"image/jpeg"_s << QByteArray::fromHex("ffd8ffe0") << false;
        QTest::newRow("pdf") << u"doc.pdf"_s << u"application/pdf"_s << QByteArray("%PDF-1.4\n") << false;
        QTest::newRow("text") << u"notes.txt"_s << u"text/plain"_s << QByteArray("hello\n") << false;
        QTest::newRow("zip") << u"archive.zip"_s << u"application/zip"_s << QByteArray::fromHex("504b0304") << false;
        // Only the last suffix counts: this is a PDF.
        QTest::newRow("exe inside a pdf name") << u"report.exe.pdf"_s << u"application/pdf"_s << QByteArray("%PDF-1.4\n") << false;
    }

    void testRiskyAttachment()
    {
        QFETCH(QString, name);
        QFETCH(QString, mime);
        QFETCH(QByteArray, data);
        QFETCH(bool, risky);
        QCOMPARE(isRiskyAttachment(AttachmentData{name, mime, data}), risky);
    }

    void testStaleAttachmentFolder()
    {
        const auto dead = [](qint64) {
            return false;
        };
        const auto alive = [](qint64) {
            return true;
        };
        QVERIFY(isStaleAttachmentFolder(u"plasma-mail-4242-aB3dE9"_s, 0, dead));
        QVERIFY(!isStaleAttachmentFolder(u"plasma-mail-4242-aB3dE9"_s, 999999, alive));
        QVERIFY(!isStaleAttachmentFolder(u"plasma-mail-0-aB3dE9"_s, 999999, dead));
        QVERIFY(!isStaleAttachmentFolder(u"plasma-mail-4242-aB3d"_s, 999999, dead)); // wrong length
        QVERIFY(!isStaleAttachmentFolder(u"plasma-mail-4242-aB3dE9x"_s, 999999, dead));
        QVERIFY(isStaleAttachmentFolder(u"plasma-mail-aB3dE9"_s, 2 * 24 * 3600, alive));
        QVERIFY(!isStaleAttachmentFolder(u"plasma-mail-aB3dE9"_s, 3600, dead)); // old style, too young
        QVERIFY(!isStaleAttachmentFolder(u"plasma-mail-aB3dE9-"_s, 999999, dead));
        QVERIFY(!isStaleAttachmentFolder(u"other-4242-aB3dE9"_s, 999999, dead));
    }

    void testPreviewPlain()
    {
        // Line breaks and runs of spaces become single spaces; quoted lines are skipped.
        QCOMPARE(preview("Subject: a\nContent-Type: text/plain\n\nHello  Bob,\n\n> old text\n>> older\nSee you\ttomorrow.\n"),
                 u"Hello Bob, See you tomorrow."_s);
    }

    void testPreviewPrefersPlainOverHtml()
    {
        const QByteArray raw(
            "Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/alternative; boundary=B\n\n"
            "--B\nContent-Type: text/plain\n\nplain words\n"
            "--B\nContent-Type: text/html\n\n<p>html words</p>\n--B--\n");
        QCOMPARE(preview(raw), u"plain words"_s);
    }

    void testPreviewFromHtml()
    {
        const QByteArray raw(
            "Subject: a\nContent-Type: text/html\n\n"
            "<html><head><style>p { color: red; }</style><title>T</title></head>"
            "<body><p>Big <b>sale</b> &amp; more</p><img src=\"https://example.org/x.png\"><p>Second</p></body></html>\n");
        const QString text = preview(raw);
        QVERIFY2(text.startsWith(u"Big sale & more"_s), qPrintable(text));
        QVERIFY2(text.contains(u"Second"_s), qPrintable(text));
        QVERIFY2(!text.contains(u"color"_s) && !text.contains(u'<'), qPrintable(text)); // no CSS, no tags
        QVERIFY2(!text.contains(QChar::ObjectReplacementCharacter), qPrintable(text)); // the image
    }

    void testPreviewDropsInvisibleCharacters()
    {
        // Zero-width non-joiner and soft hyphen padding, as newsletters put before the text.
        QCOMPARE(preview("Subject: a\nContent-Type: text/plain; charset=utf-8\n\n\u200c\u00ad\u200c\u00adSale today\n"), u"Sale today"_s);
    }

    void testPreviewTruncates()
    {
        const QString text = preview("Subject: a\nContent-Type: text/plain\n\n" + QByteArray(300, 'x') + "\n", 20);
        QCOMPARE(text.size(), 20);
        QVERIFY(text.endsWith(u'…'));
        QCOMPARE(preview("Subject: a\nContent-Type: text/plain\n\n" + QByteArray(20, 'y') + "\n", 20), QString(20, u'y')); // fits
    }

    void testPreviewEmpty()
    {
        QVERIFY(preview("Subject: a\nContent-Type: text/plain\n\n").isEmpty());
        QVERIFY(preview("Subject: a\nContent-Type: text/plain\n\n> only a quote\n").isEmpty());
        QVERIFY(preview("Subject: a\nMIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=B\n\n"
                        "--B\nContent-Type: application/pdf\nContent-Disposition: attachment; filename=a.pdf\n"
                        "Content-Transfer-Encoding: base64\n\nJVBERg==\n--B--\n")
                    .isEmpty());
    }
};

QTEST_GUILESS_MAIN(MessageRendererTest)
#include "messagerenderertest.moc"
