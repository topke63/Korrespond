// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

#include "composehtml.h"

using namespace Qt::StringLiterals;

class ComposeHtmlTest : public QObject
{
    Q_OBJECT

private:
    // Appends `text` to the document with the given character format.
    static void append(QTextCursor &cursor, const QString &text, const QTextCharFormat &format = {})
    {
        cursor.insertText(text, format);
    }

private Q_SLOTS:
    void testPlainTextHasNoFormatting()
    {
        QTextDocument document;
        document.setPlainText(u"Hello\nworld"_s);
        const ComposeHtml::Result result = ComposeHtml::fromDocument(document);
        QVERIFY(!result.hasFormatting);
        QCOMPARE(result.html, u"<div>Hello</div><div>world</div>"_s);
    }

    void testStyles()
    {
        QTextDocument document;
        QTextCursor cursor(&document);
        QTextCharFormat bold;
        bold.setFontWeight(QFont::Bold);
        QTextCharFormat italic;
        italic.setFontItalic(true);
        QTextCharFormat underline;
        underline.setFontUnderline(true);
        QTextCharFormat strike;
        strike.setFontStrikeOut(true);
        append(cursor, u"a "_s);
        append(cursor, u"b"_s, bold);
        append(cursor, u"i"_s, italic);
        append(cursor, u"u"_s, underline);
        append(cursor, u"s"_s, strike);
        const ComposeHtml::Result result = ComposeHtml::fromDocument(document);
        QVERIFY(result.hasFormatting);
        QCOMPARE(result.html, u"<div>a <b>b</b><i>i</i><u>u</u><s>s</s></div>"_s);
    }

    void testNestedStyles()
    {
        QTextDocument document;
        QTextCursor cursor(&document);
        QTextCharFormat both;
        both.setFontWeight(QFont::Bold);
        both.setFontItalic(true);
        append(cursor, u"x"_s, both);
        QCOMPARE(ComposeHtml::fromDocument(document).html, u"<div><i><b>x</b></i></div>"_s);
    }

    void testEscapingAndSpaces()
    {
        QTextDocument document;
        document.setPlainText(u"1 < 2 & \"3\" > 0\n  two  spaces"_s);
        const ComposeHtml::Result result = ComposeHtml::fromDocument(document);
        QVERIFY(!result.hasFormatting);
        QCOMPARE(result.html, u"<div>1 &lt; 2 &amp; &quot;3&quot; &gt; 0</div><div>&nbsp; two&nbsp; spaces</div>"_s);
    }

    void testEmptyParagraphAndLineBreak()
    {
        QTextDocument document;
        document.setPlainText(u"a\n\nb"_s);
        QCOMPARE(ComposeHtml::fromDocument(document).html, u"<div>a</div><div><br></div><div>b</div>"_s);
        QTextDocument broken;
        QTextCursor cursor(&broken);
        cursor.insertText(u"a"_s);
        cursor.insertText(QString(QChar::LineSeparator));
        cursor.insertText(u"b"_s);
        QCOMPARE(ComposeHtml::fromDocument(broken).html, u"<div>a<br>b</div>"_s);
    }

    void testPastedColourAndLinkAreDropped()
    {
        QTextDocument document;
        document.setHtml(
            u"<p>see <a href=\"http://x.test\">link</a> <span style=\"color:#ff0000; font-size:30px\">red</span> "
            "<b>bold</b></p>"_s);
        const ComposeHtml::Result result = ComposeHtml::fromDocument(document);
        QVERIFY(result.hasFormatting); // the <b>
        QVERIFY(!result.html.contains(u"href"_s));
        QVERIFY(!result.html.contains(u"color"_s));
        QVERIFY(!result.html.contains(u"style"_s));
        QVERIFY(!result.html.contains(u"<a"_s));
        QVERIFY(result.html.contains(u"<b>bold</b>"_s));
        QVERIFY(result.html.contains(u"link"_s) && result.html.contains(u"red"_s));
    }

    void testLinkOnlyIsNotFormatting()
    {
        QTextDocument document;
        document.setHtml(u"<p><a href=\"http://x.test\">link</a></p>"_s);
        // Qt marks anchor text underlined; a link alone must not turn the message into HTML.
        QVERIFY(!ComposeHtml::fromDocument(document).hasFormatting);
    }
};

QTEST_GUILESS_MAIN(ComposeHtmlTest)

#include "composehtmltest.moc"
