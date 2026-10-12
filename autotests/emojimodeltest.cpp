// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include "emojimodel.h"

using namespace Qt::StringLiterals;

class EmojiModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testListIsNotEmptyAndHasNoDuplicates()
    {
        EmojiModel model;
        QVERIFY(model.rowCount() >= 200);
        QSet<QString> seen;
        for (int row = 0; row < model.rowCount(); ++row) {
            const QString emoji = model.index(row).data(EmojiModel::EmojiRole).toString();
            QVERIFY(!emoji.isEmpty());
            QVERIFY2(!seen.contains(emoji), qPrintable(emoji));
            seen.insert(emoji);
            QVERIFY(!model.index(row).data(EmojiModel::NameRole).toString().isEmpty());
        }
    }

    void testEveryCategoryHasEmoji()
    {
        EmojiModel model;
        QCOMPARE(model.categories().size(), 8);
        for (const QString &category : model.categories()) {
            QVERIFY2(!model.categoryEmoji(category).isEmpty(), qPrintable(category));
            model.setCategory(category);
            QVERIFY2(model.rowCount() > 0, qPrintable(category));
            for (int row = 0; row < model.rowCount(); ++row) {
                QCOMPARE(model.index(row).data(EmojiModel::CategoryRole).toString(), category);
            }
        }
    }

    void testFilterMatchesNames()
    {
        EmojiModel model;
        const int all = model.rowCount();
        QSignalSpy spy(&model, &EmojiModel::filterChanged);
        model.setFilter(u"SMIL"_s);
        QCOMPARE(spy.count(), 1);
        QVERIFY(model.rowCount() > 0);
        QVERIFY(model.rowCount() < all);
        for (int row = 0; row < model.rowCount(); ++row) {
            QVERIFY(model.index(row).data(EmojiModel::NameRole).toString().contains(u"smil"_s, Qt::CaseInsensitive));
        }
        model.setFilter(u"no such emoji name"_s);
        QCOMPARE(model.rowCount(), 0);
        model.setFilter(QString());
        QCOMPARE(model.rowCount(), all);
    }

    void testFilterAndCategoryTogether()
    {
        EmojiModel model;
        model.setCategory(u"food"_s);
        model.setFilter(u"pizza"_s);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.index(0).data(EmojiModel::EmojiRole).toString(), QString::fromUtf8("🍕"));
        model.setCategory(u"animals"_s);
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(EmojiModelTest)

#include "emojimodeltest.moc"
