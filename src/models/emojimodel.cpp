// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emojimodel.h"

using namespace Qt::StringLiterals;

namespace
{
struct Entry {
    const char *category;
    const char *emoji;
    const char *name;
};

// Names are English, lower case; the order is the order of the picker.
const Entry s_entries[] = {
    {"smileys", "😀", "grinning face"},
    {"smileys", "😃", "smiling face with big eyes"},
    {"smileys", "😄", "grinning face with smiling eyes"},
    {"smileys", "😁", "beaming face"},
    {"smileys", "😆", "laughing"},
    {"smileys", "😅", "grinning face with sweat"},
    {"smileys", "🤣", "rolling on the floor laughing"},
    {"smileys", "😂", "face with tears of joy"},
    {"smileys", "🙂", "slightly smiling face"},
    {"smileys", "🙃", "upside-down face"},
    {"smileys", "😉", "winking face"},
    {"smileys", "😊", "smiling face with smiling eyes"},
    {"smileys", "😇", "smiling face with halo"},
    {"smileys", "🥰", "smiling face with hearts"},
    {"smileys", "😍", "heart eyes"},
    {"smileys", "🤩", "star struck"},
    {"smileys", "😘", "face blowing a kiss"},
    {"smileys", "😋", "yummy face"},
    {"smileys", "😛", "face with tongue"},
    {"smileys", "😜", "winking face with tongue"},
    {"smileys", "🤪", "zany face"},
    {"smileys", "🤔", "thinking face"},
    {"smileys", "🤨", "raised eyebrow"},
    {"smileys", "😐", "neutral face"},
    {"smileys", "😑", "expressionless face"},
    {"smileys", "😶", "face without mouth"},
    {"smileys", "🙄", "rolling eyes"},
    {"smileys", "😏", "smirking face"},
    {"smileys", "😌", "relieved face"},
    {"smileys", "😴", "sleeping face"},
    {"smileys", "😎", "smiling face with sunglasses"},
    {"smileys", "🤓", "nerd face"},
    {"smileys", "😕", "confused face"},
    {"smileys", "😟", "worried face"},
    {"smileys", "🙁", "slightly frowning face"},
    {"smileys", "😮", "face with open mouth"},
    {"smileys", "😲", "astonished face"},
    {"smileys", "😳", "flushed face"},
    {"smileys", "🥺", "pleading face"},
    {"smileys", "😢", "crying face"},
    {"smileys", "😭", "loudly crying face"},
    {"smileys", "😱", "face screaming in fear"},
    {"smileys", "😤", "face with steam from nose"},
    {"smileys", "😡", "pouting face"},
    {"smileys", "😠", "angry face"},
    {"smileys", "🤯", "exploding head"},
    {"smileys", "🥳", "partying face"},
    {"smileys", "🤒", "face with thermometer"},
    {"smileys", "🤕", "face with head bandage"},
    {"smileys", "🤢", "nauseated face"},
    {"smileys", "🥵", "hot face"},
    {"smileys", "🥶", "cold face"},
    {"smileys", "😷", "face with medical mask"},
    {"smileys", "🤗", "hugging face"},
    {"smileys", "🤫", "shushing face"},
    {"smileys", "🤭", "face with hand over mouth"},
    {"smileys", "😬", "grimacing face"},
    {"smileys", "🤐", "zipper mouth face"},
    {"people", "👍", "thumbs up"},
    {"people", "👎", "thumbs down"},
    {"people", "👌", "ok hand"},
    {"people", "✌️", "victory hand"},
    {"people", "🤞", "crossed fingers"},
    {"people", "🤟", "love you gesture"},
    {"people", "🤘", "sign of the horns"},
    {"people", "🤙", "call me hand"},
    {"people", "👈", "backhand index pointing left"},
    {"people", "👉", "backhand index pointing right"},
    {"people", "👆", "backhand index pointing up"},
    {"people", "👇", "backhand index pointing down"},
    {"people", "☝️", "index pointing up"},
    {"people", "✋", "raised hand"},
    {"people", "🖐️", "hand with fingers splayed"},
    {"people", "👋", "waving hand"},
    {"people", "👏", "clapping hands"},
    {"people", "🙌", "raising hands"},
    {"people", "🙏", "folded hands"},
    {"people", "🤝", "handshake"},
    {"people", "💪", "flexed biceps"},
    {"people", "✍️", "writing hand"},
    {"people", "👀", "eyes"},
    {"people", "🧠", "brain"},
    {"people", "👶", "baby"},
    {"people", "🧑", "person"},
    {"people", "👨", "man"},
    {"people", "👩", "woman"},
    {"people", "🧓", "older person"},
    {"people", "🤷", "person shrugging"},
    {"people", "🤦", "person facepalming"},
    {"people", "🙋", "person raising hand"},
    {"people", "💁", "person tipping hand"},
    {"people", "🙇", "person bowing"},
    {"people", "🧑‍💻", "technologist"},
    {"people", "🧑‍🍳", "cook"},
    {"people", "🏃", "person running"},
    {"people", "🚶", "person walking"},
    {"people", "💃", "woman dancing"},
    {"people", "🕺", "man dancing"},
    {"animals", "🐶", "dog face"},
    {"animals", "🐱", "cat face"},
    {"animals", "🐭", "mouse face"},
    {"animals", "🐹", "hamster"},
    {"animals", "🐰", "rabbit face"},
    {"animals", "🦊", "fox"},
    {"animals", "🐻", "bear"},
    {"animals", "🐼", "panda"},
    {"animals", "🐨", "koala"},
    {"animals", "🐯", "tiger face"},
    {"animals", "🦁", "lion"},
    {"animals", "🐮", "cow face"},
    {"animals", "🐷", "pig face"},
    {"animals", "🐸", "frog"},
    {"animals", "🐵", "monkey face"},
    {"animals", "🐔", "chicken"},
    {"animals", "🐧", "penguin"},
    {"animals", "🐦", "bird"},
    {"animals", "🦆", "duck"},
    {"animals", "🦉", "owl"},
    {"animals", "🐺", "wolf"},
    {"animals", "🐴", "horse face"},
    {"animals", "🦄", "unicorn"},
    {"animals", "🐝", "honeybee"},
    {"animals", "🦋", "butterfly"},
    {"animals", "🐌", "snail"},
    {"animals", "🐢", "turtle"},
    {"animals", "🐍", "snake"},
    {"animals", "🐙", "octopus"},
    {"animals", "🐠", "tropical fish"},
    {"animals", "🐬", "dolphin"},
    {"animals", "🐳", "whale"},
    {"animals", "🌸", "cherry blossom"},
    {"animals", "🌹", "rose"},
    {"animals", "🌻", "sunflower"},
    {"animals", "🌲", "evergreen tree"},
    {"animals", "🍀", "four leaf clover"},
    {"animals", "🍁", "maple leaf"},
    {"food", "🍎", "red apple"},
    {"food", "🍐", "pear"},
    {"food", "🍊", "tangerine"},
    {"food", "🍋", "lemon"},
    {"food", "🍌", "banana"},
    {"food", "🍉", "watermelon"},
    {"food", "🍇", "grapes"},
    {"food", "🍓", "strawberry"},
    {"food", "🍒", "cherries"},
    {"food", "🍑", "peach"},
    {"food", "🥑", "avocado"},
    {"food", "🍅", "tomato"},
    {"food", "🥕", "carrot"},
    {"food", "🌽", "ear of corn"},
    {"food", "🥔", "potato"},
    {"food", "🍞", "bread"},
    {"food", "🧀", "cheese wedge"},
    {"food", "🍳", "cooking egg"},
    {"food", "🥓", "bacon"},
    {"food", "🍔", "hamburger"},
    {"food", "🍟", "french fries"},
    {"food", "🍕", "pizza"},
    {"food", "🌭", "hot dog"},
    {"food", "🌮", "taco"},
    {"food", "🍝", "spaghetti"},
    {"food", "🍣", "sushi"},
    {"food", "🍦", "soft ice cream"},
    {"food", "🍩", "doughnut"},
    {"food", "🍪", "cookie"},
    {"food", "🎂", "birthday cake"},
    {"food", "🍫", "chocolate bar"},
    {"food", "🍿", "popcorn"},
    {"food", "☕", "hot beverage"},
    {"food", "🍵", "teacup"},
    {"food", "🍺", "beer mug"},
    {"food", "🍷", "wine glass"},
    {"food", "🥂", "clinking glasses"},
    {"food", "🍾", "bottle with popping cork"},
    {"travel", "🚗", "car"},
    {"travel", "🚕", "taxi"},
    {"travel", "🚌", "bus"},
    {"travel", "🚑", "ambulance"},
    {"travel", "🚒", "fire engine"},
    {"travel", "🚲", "bicycle"},
    {"travel", "🏍️", "motorcycle"},
    {"travel", "🚆", "train"},
    {"travel", "✈️", "airplane"},
    {"travel", "🚀", "rocket"},
    {"travel", "🚁", "helicopter"},
    {"travel", "⛵", "sailboat"},
    {"travel", "🚢", "ship"},
    {"travel", "🏠", "house"},
    {"travel", "🏢", "office building"},
    {"travel", "🏥", "hospital"},
    {"travel", "🏫", "school"},
    {"travel", "🏰", "castle"},
    {"travel", "⛪", "church"},
    {"travel", "🗼", "tokyo tower"},
    {"travel", "🌍", "globe showing europe and africa"},
    {"travel", "🗺️", "world map"},
    {"travel", "🏖️", "beach with umbrella"},
    {"travel", "⛰️", "mountain"},
    {"travel", "🌋", "volcano"},
    {"travel", "🌅", "sunrise"},
    {"travel", "🌃", "night with stars"},
    {"travel", "🌈", "rainbow"},
    {"travel", "☀️", "sun"},
    {"travel", "🌙", "crescent moon"},
    {"travel", "⭐", "star"},
    {"travel", "☁️", "cloud"},
    {"travel", "🌧️", "cloud with rain"},
    {"travel", "⛈️", "cloud with lightning and rain"},
    {"travel", "❄️", "snowflake"},
    {"travel", "🔥", "fire"},
    {"travel", "💧", "droplet"},
    {"travel", "🌊", "water wave"},
    {"activities", "⚽", "soccer ball"},
    {"activities", "🏀", "basketball"},
    {"activities", "🏈", "american football"},
    {"activities", "⚾", "baseball"},
    {"activities", "🎾", "tennis"},
    {"activities", "🏐", "volleyball"},
    {"activities", "🏓", "ping pong"},
    {"activities", "🏸", "badminton"},
    {"activities", "🥊", "boxing glove"},
    {"activities", "🎯", "bullseye"},
    {"activities", "🎱", "pool 8 ball"},
    {"activities", "🎳", "bowling"},
    {"activities", "⛳", "flag in hole"},
    {"activities", "🏆", "trophy"},
    {"activities", "🥇", "first place medal"},
    {"activities", "🥈", "second place medal"},
    {"activities", "🥉", "third place medal"},
    {"activities", "🎮", "video game"},
    {"activities", "🎲", "game die"},
    {"activities", "🧩", "puzzle piece"},
    {"activities", "🎸", "guitar"},
    {"activities", "🎹", "musical keyboard"},
    {"activities", "🎤", "microphone"},
    {"activities", "🎧", "headphone"},
    {"activities", "🎬", "clapper board"},
    {"activities", "🎨", "artist palette"},
    {"activities", "🎉", "party popper"},
    {"activities", "🎊", "confetti ball"},
    {"activities", "🎈", "balloon"},
    {"activities", "🎁", "wrapped gift"},
    {"activities", "🎄", "christmas tree"},
    {"activities", "🎃", "jack o lantern"},
    {"objects", "📱", "mobile phone"},
    {"objects", "💻", "laptop"},
    {"objects", "🖥️", "desktop computer"},
    {"objects", "⌨️", "keyboard"},
    {"objects", "🖱️", "computer mouse"},
    {"objects", "🖨️", "printer"},
    {"objects", "📷", "camera"},
    {"objects", "📹", "video camera"},
    {"objects", "📞", "telephone receiver"},
    {"objects", "📧", "e-mail"},
    {"objects", "📨", "incoming envelope"},
    {"objects", "📬", "open mailbox with raised flag"},
    {"objects", "📎", "paperclip"},
    {"objects", "📌", "pushpin"},
    {"objects", "📝", "memo"},
    {"objects", "📄", "page facing up"},
    {"objects", "📁", "file folder"},
    {"objects", "📅", "calendar"},
    {"objects", "📊", "bar chart"},
    {"objects", "📈", "chart increasing"},
    {"objects", "📚", "books"},
    {"objects", "📖", "open book"},
    {"objects", "🔍", "magnifying glass"},
    {"objects", "🔒", "locked"},
    {"objects", "🔑", "key"},
    {"objects", "🔔", "bell"},
    {"objects", "💡", "light bulb"},
    {"objects", "🔋", "battery"},
    {"objects", "🔧", "wrench"},
    {"objects", "🔨", "hammer"},
    {"objects", "⚙️", "gear"},
    {"objects", "💰", "money bag"},
    {"objects", "💳", "credit card"},
    {"objects", "⏰", "alarm clock"},
    {"objects", "⌛", "hourglass"},
    {"objects", "🧳", "luggage"},
    {"objects", "🛒", "shopping cart"},
    {"objects", "🛠️", "hammer and wrench"},
    {"symbols", "❤️", "red heart"},
    {"symbols", "🧡", "orange heart"},
    {"symbols", "💛", "yellow heart"},
    {"symbols", "💚", "green heart"},
    {"symbols", "💙", "blue heart"},
    {"symbols", "💜", "purple heart"},
    {"symbols", "🖤", "black heart"},
    {"symbols", "💔", "broken heart"},
    {"symbols", "💕", "two hearts"},
    {"symbols", "💖", "sparkling heart"},
    {"symbols", "💯", "hundred points"},
    {"symbols", "💢", "anger symbol"},
    {"symbols", "💥", "collision"},
    {"symbols", "💫", "dizzy"},
    {"symbols", "💤", "zzz"},
    {"symbols", "✨", "sparkles"},
    {"symbols", "✅", "check mark button"},
    {"symbols", "✔️", "check mark"},
    {"symbols", "❌", "cross mark"},
    {"symbols", "❓", "question mark"},
    {"symbols", "❗", "exclamation mark"},
    {"symbols", "⚠️", "warning"},
    {"symbols", "🚫", "prohibited"},
    {"symbols", "⛔", "no entry"},
    {"symbols", "➕", "plus"},
    {"symbols", "➖", "minus"},
    {"symbols", "➡️", "right arrow"},
    {"symbols", "⬅️", "left arrow"},
    {"symbols", "⬆️", "up arrow"},
    {"symbols", "⬇️", "down arrow"},
    {"symbols", "🔄", "counterclockwise arrows"},
    {"symbols", "♻️", "recycling symbol"},
    {"symbols", "©️", "copyright"},
    {"symbols", "™️", "trade mark"},
    {"symbols", "🆗", "ok button"},
    {"symbols", "🆕", "new button"},
    {"symbols", "🔴", "red circle"},
    {"symbols", "🟢", "green circle"},
    {"symbols", "🔵", "blue circle"},
    {"symbols", "⚪", "white circle"},
};
}

EmojiModel::EmojiModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refresh();
}

int EmojiModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant EmojiModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const Entry &entry = s_entries[m_rows.at(index.row())];
    switch (role) {
    case EmojiRole:
    case Qt::DisplayRole:
        return QString::fromUtf8(entry.emoji);
    case NameRole:
        return QString::fromUtf8(entry.name);
    case CategoryRole:
        return QString::fromUtf8(entry.category);
    default:
        return {};
    }
}

QHash<int, QByteArray> EmojiModel::roleNames() const
{
    return {{EmojiRole, "emoji"}, {NameRole, "name"}, {CategoryRole, "category"}};
}

QString EmojiModel::filter() const
{
    return m_filter;
}

void EmojiModel::setFilter(const QString &filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    refresh();
    Q_EMIT filterChanged();
}

QString EmojiModel::category() const
{
    return m_category;
}

void EmojiModel::setCategory(const QString &category)
{
    if (m_category == category) {
        return;
    }
    m_category = category;
    refresh();
    Q_EMIT categoryChanged();
}

QStringList EmojiModel::categories() const
{
    return {u"smileys"_s, u"people"_s, u"animals"_s, u"food"_s, u"travel"_s, u"activities"_s, u"objects"_s, u"symbols"_s};
}

QString EmojiModel::emojiAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? QString::fromUtf8(s_entries[m_rows.at(row)].emoji) : QString();
}

QString EmojiModel::categoryEmoji(const QString &category) const
{
    for (const Entry &entry : s_entries) {
        if (QString::fromUtf8(entry.category) == category) {
            return QString::fromUtf8(entry.emoji);
        }
    }
    return {};
}

void EmojiModel::refresh()
{
    beginResetModel();
    m_rows.clear();
    const QString needle = m_filter.trimmed();
    for (int i = 0; i < static_cast<int>(std::size(s_entries)); ++i) {
        const Entry &entry = s_entries[i];
        if (!m_category.isEmpty() && QString::fromUtf8(entry.category) != m_category) {
            continue;
        }
        if (!needle.isEmpty() && !QString::fromUtf8(entry.name).contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        m_rows.append(i);
    }
    endResetModel();
}
