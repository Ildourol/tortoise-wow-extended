#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

using uint32 = unsigned;
constexpr int MAX_DBC_LOCALE = 8;
struct Field {
    std::string value;
    uint32 GetUInt32() const { return value.empty() ? 0 : std::stoul(value); }
    std::string GetCppString() const { return value; }
};
using Row = std::array<Field, 21>;
struct QueryResult {
    std::vector<Row> rows;
    size_t index = 0;
    Field* Fetch() { return rows[index].data(); }
    bool NextRow() { return ++index < rows.size(); }
};
struct Database {
    std::vector<Row> rows;
    QueryResult* Query(char const*) { return rows.empty() ? nullptr : new QueryResult{rows}; }
} WorldDatabase;
struct ChatChannelsEntry {
    uint32 id, flags, factionGroup, nameFlags, shortcutFlags;
    std::string name[MAX_DBC_LOCALE], shortcut[MAX_DBC_LOCALE];
};
struct ObjectMgr {
    std::map<uint32, ChatChannelsEntry> m_chatChannelsMap;
    void LoadChatChannels();
    ChatChannelsEntry const* GetChannelEntryFor(uint32);
    ChatChannelsEntry const* GetChannelEntryFor(std::string const&);
};
#include "ChatChannelsNative.inc"

void Check(bool ok, char const* why) { if (!ok) throw std::runtime_error(why); }
Row Channel(uint32 id, std::string name) {
    Row row{};
    row[0].value = std::to_string(id);
    row[1].value = std::to_string(id * 10);
    row[2].value = "3";
    row[3].value = name;
    row[11].value = "7";
    row[20].value = "9";
    return row;
}
int main() {
    auto general = Channel(1, "General - %s");
    general[5].value = "General [%s]";
    general[12].value = "General";
    auto trade = Channel(2, "Trade - %s");
    trade[19].value = "TradeShortcut";
    WorldDatabase.rows = {general, trade, Channel(22, "WorldDefense")};
    ObjectMgr mgr;
    mgr.LoadChatChannels();
    auto entry = mgr.GetChannelEntryFor(uint32(22));
    Check(entry && entry->id == 22 && entry->flags == 220, "DB channel ID/flags lost");
    Check(entry->factionGroup == 3 && entry->nameFlags == 7 && entry->shortcutFlags == 9, "DB metadata lost");
    auto matches = [&](std::string name, uint32 id) {
        auto value = mgr.GetChannelEntryFor(name);
        return value && value->id == id;
    };
    Check(matches("Trade - Stormwind City", 2), "zoned trade assigned wrong identity");
    Check(matches("WorldDefense", 22), "fixed built-in channel missing");
    Check(matches("General - Elwynn Forest", 1), "zone prefix missing");
    Check(matches("General [Elwynn Forest]", 1), "localized prefix/suffix missing");
    Check(matches("General", 1) && matches("TradeShortcut", 2), "shortcut locales missing");
    Check(matches("General - ", 1), "existing empty-zone prefix contract changed");
    for (auto const* name : {"", "World", "MyWorldDefense", "WorldDefenseClub",
            "MyGeneral - Elwynn", "General [Elwynn]Club", "General [Elwynn",
            "General [", "TradeShortcutClub", "SomeOtherChannel"})
        Check(!mgr.GetChannelEntryFor(std::string(name)), "custom/invalid channel treated as built-in");
    Check(!mgr.GetChannelEntryFor(uint32(99)), "missing numeric channel found");
    WorldDatabase.rows = {Channel(55, "Replacement")};
    mgr.LoadChatChannels();
    Check(!mgr.GetChannelEntryFor(uint32(22)) && matches("Replacement", 55), "reload retained stale channels");
    WorldDatabase.rows.clear();
    mgr.LoadChatChannels();
    Check(mgr.m_chatChannelsMap.empty(), "empty DB reload retained stale channels");
    std::cout << "Native channel loading, identity, locales, custom names and reload passed\n";
}
