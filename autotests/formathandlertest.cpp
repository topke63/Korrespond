// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>

#include "formathandler.h"

using namespace Qt::StringLiterals;

class FormatHandlerTest : public QObject
{
    Q_OBJECT

private:
    // The character format of the character at `position` (0-based).
    static QTextCharFormat formatOf(QTextDocument &document, int position)
    {
        QTextCursor cursor(&document);
        cursor.setPosition(position + 1);
        return cursor.charFormat();
    }

private Q_SLOTS:
    void testNoDocumentIsHarmless()
    {
        FormatHandler handler;
        QVERIFY(!handler.bold());
        handler.setBold(true);
        handler.setPlainText(u"x"_s);
        QVERIFY(!handler.bold());
    }

    void testBoldOnSelectionAndToggleOff()
    {
        QTextDocument document;
        document.setPlainText(u"hello big world"_s);
        FormatHandler handler;
        handler.setDocument(&document);
        handler.setSelectionStart(6);
        handler.setSelectionEnd(9);
        QVERIFY(!handler.bold());

        QSignalSpy spy(&handler, &FormatHandler::formatChanged);
        handler.setBold(true);
        QVERIFY(spy.count() >= 1);
        QVERIFY(handler.bold());
        QCOMPARE(formatOf(document, 5).fontWeight(), static_cast<int>(QFont::Normal));
        QCOMPARE(formatOf(document, 6).fontWeight(), static_cast<int>(QFont::Bold));
        QCOMPARE(formatOf(document, 8).fontWeight(), static_cast<int>(QFont::Bold));
        QCOMPARE(formatOf(document, 9).fontWeight(), static_cast<int>(QFont::Normal));

        handler.setBold(false);
        QVERIFY(!handler.bold());
        QCOMPARE(formatOf(document, 7).fontWeight(), static_cast<int>(QFont::Normal));
    }

    void testOtherStyles()
    {
        QTextDocument document;
        document.setPlainText(u"abc"_s);
        FormatHandler handler;
        handler.setDocument(&document);
        handler.setSelectionStart(0);
        handler.setSelectionEnd(3);
        handler.setItalic(true);
        handler.setUnderline(true);
        handler.setStrikeOut(true);
        QVERIFY(handler.italic());
        QVERIFY(handler.underline());
        QVERIFY(handler.strikeOut());
        QVERIFY(!handler.bold());
        QVERIFY(formatOf(document, 1).fontItalic());
        QVERIFY(formatOf(document, 1).fontUnderline());
        QVERIFY(formatOf(document, 1).fontStrikeOut());
    }

    void testNoSelectionStylesTheWordUnderTheCursor()
    {
        QTextDocument document;
        document.setPlainText(u"one two three"_s);
        FormatHandler handler;
        handler.setDocument(&document);
        handler.setCursorPosition(5); // inside "two"
        handler.setBold(true);
        QCOMPARE(formatOf(document, 3).fontWeight(), static_cast<int>(QFont::Normal));
        QCOMPARE(formatOf(document, 4).fontWeight(), static_cast<int>(QFont::Bold));
        QCOMPARE(formatOf(document, 6).fontWeight(), static_cast<int>(QFont::Bold));
        QCOMPARE(formatOf(document, 8).fontWeight(), static_cast<int>(QFont::Normal));
    }

    void testPositionsOutsideTheDocumentAreClamped()
    {
        QTextDocument document;
        document.setPlainText(u"abc"_s);
        FormatHandler handler;
        handler.setDocument(&document);
        handler.setSelectionStart(-5);
        handler.setSelectionEnd(99);
        handler.setBold(true);
        QVERIFY(handler.bold());
        QVERIFY(formatOf(document, 2).fontWeight() >= QFont::Bold);
    }

    void testFormattedHtmlOnlyWhenFormatted()
    {
        QTextDocument document;
        document.setPlainText(u"say hi"_s);
        FormatHandler handler;
        QVERIFY(handler.formattedHtml().isEmpty()); // no document
        handler.setDocument(&document);
        QVERIFY(handler.formattedHtml().isEmpty());
        handler.setSelectionStart(4);
        handler.setSelectionEnd(6);
        handler.setBold(true);
        QCOMPARE(handler.formattedHtml(), u"<div>say <b>hi</b></div>"_s);
    }

    // What goes out as HTML comes back as the same text and the same HTML (undo send).
    void testHtmlRoundTrip()
    {
        QTextDocument document;
        document.setPlainText(u"\nHi  there <you> & co\n\nsecond  line\n> quoted"_s);
        FormatHandler handler;
        handler.setDocument(&document);
        handler.setSelectionStart(1);
        handler.setSelectionEnd(3);
        handler.setBold(true);
        handler.setSelectionStart(15);
        handler.setSelectionEnd(20);
        handler.setItalic(true);
        handler.setUnderline(true);
        const QString plain = document.toPlainText();
        const QString html = handler.formattedHtml();
        QVERIFY(!html.isEmpty());

        QTextDocument restored;
        FormatHandler other;
        other.setDocument(&restored);
        other.setHtml(html);
        QCOMPARE(restored.toPlainText(), plain);
        QCOMPARE(other.formattedHtml(), html);
    }

    void testSetPlainTextDropsFormatting()
    {
        QTextDocument document;
        document.setPlainText(u"abc"_s);
        FormatHandler handler;
        handler.setDocument(&document);
        handler.setSelectionStart(0);
        handler.setSelectionEnd(3);
        handler.setBold(true);
        handler.setPlainText(u"new text"_s);
        QCOMPARE(document.toPlainText(), u"new text"_s);
        QCOMPARE(formatOf(document, 1).fontWeight(), static_cast<int>(QFont::Normal));
    }
};

QTEST_GUILESS_MAIN(FormatHandlerTest)

#include "formathandlertest.moc"
