
#include "Category.h"
#include <memory>
#include "DetailedWorkDiagnostics.h"
#include "WorkSlice.h"
#include "ItemBag.h"
#include "ahbot/AhBot.h"
#include "World.h"
#include "Config/Config.h"
#include "Chat/Chat.h"
#include "AhBotConfig.h"
#include "AuctionHouse/AuctionHouseMgr.h"
#include "WorldSession.h"
#include "Objects/Player.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "AccountMgr.h"
#include "playerbot/playerbot.h"
#include "Mail/Mail.h"
#include "Util.h"
#include <cctype>
#include <cstdlib>
#include <limits>
#include <set>

#ifdef CMANGOS
#include <boost/thread/thread.hpp>
#endif

using namespace ahbot;

bool AhBot::HandleAhBotCommand(ChatHandler* handler, char const* args)
{
    return auctionbot.HandleCommand(handler, args ? args : "");
}

uint32 AhBot::auctionIds[MAX_AUCTIONS] = {1,6,7};
uint32 AhBot::auctioneers[MAX_AUCTIONS] = {79707,4656,23442};
std::map<uint32, uint32> AhBot::factions;

void AhBot::Init()
{
    sLog.outString("[AhBot] Initializing AhBot by ike3");

    if (!sAhBotConfig.Initialize())
    {
        sLog.outString("[AhBot] Disabled or failed to load ahbot.conf — AhBot will not run");
        return;
    }

    sLog.outString("[AhBot] Config: GUID=%llu, updateInterval=%ds, maxItemLevel=%d, maxRequiredLevel=%d, priceMultiplier=%.2f",
        (unsigned long long)sAhBotConfig.guid,
        sAhBotConfig.updateInterval,
        sAhBotConfig.maxItemLevel,
        sAhBotConfig.maxRequiredLevel,
        sAhBotConfig.priceMultiplier);

    factions[1] = 1;
    factions[2] = 1;
    factions[3] = 1;
    factions[4] = 2;
    factions[5] = 2;
    factions[6] = 2;
    factions[7] = 3;

    LoadItemOverrides();
    availableItems.Init();

    sLog.outString("[AhBot] Initialization complete. Incremental house checks every %u seconds.",
        std::max<uint32>(1, sAhBotConfig.updateInterval / MAX_AUCTIONS));
}

AhBot::~AhBot()
{
}

ObjectGuid AhBot::GetAHBplayerGUID()
{
    return ObjectGuid(sAhBotConfig.guid);
}

#ifdef MANGOS
class AhbotThread: public ACE_Task <ACE_MT_SYNCH>
{
public:
    int svc(void) { auctionbot.ForceUpdate(); return 0; }
};
#endif
#ifdef CMANGOS
void AhbotThread()
{
    auctionbot.ForceUpdate();
}
#endif

bool activateAhbotThread()
{
#ifdef MANGOS
    AhbotThread *thread = new AhbotThread();
    if (thread->activate() == -1)
    {
        delete thread;
        return false;
    }
#endif
#ifdef CMANGOS
    boost::thread t(AhbotThread);
    t.detach();
#endif
    return true;
}

bool AhBot::StartUpdate()
{
    if (!sAhBotConfig.enabled || pendingRebuild.load())
        return false;
    bool expected = false;
    if (!updating.compare_exchange_strong(expected, true))
        return false;
    // Reserve before launching: commands must also see a not-yet-started
    // worker as busy, or a rebuild/reload can race its initialization.
    try
    {
        if (activateAhbotThread())
            return true;
    }
    catch (std::exception const& error)
    {
        sLog.outError("[AhBot] Worker launch failed: %s", error.what());
    }
    updating = false;
    return false;
}

bool AhBot::QueueRebuild(bool includePlayerBids)
{
    // Repeated requests during refill must not expire the newly created stock.
    if (rebuildPassesRemaining.load())
        return false;
    uint32 const requested = includePlayerBids ? 2 : 1;
    uint32 previous = pendingRebuild.load();
    while (previous < requested &&
        !pendingRebuild.compare_exchange_weak(previous, requested)) {}
    return true;
}

bool AhBot::ProcessPendingRebuild()
{
    if (!pendingRebuild.load())
        return false;
    bool expected = false;
    if (!updating.compare_exchange_strong(expected, true))
        return true; // retain request until the active worker releases ownership
    struct ReleaseRebuild { std::atomic<bool>& busy; ~ReleaseRebuild() { busy = false; } } release{updating};
    uint32 const request = pendingRebuild.exchange(0);
    try
    {
    if (sAhBotConfig.enabled)
    {
        uint32 protectedBids = 0;
        Rebuild(request == 2, protectedBids);
    }
    else
        sLog.outString("[AhBot] Pending rebuild canceled: AHBot is disabled.");
    }
    catch (std::exception const& error)
    {
        pendingRebuild = request;
        sLog.outError("[AhBot] Rebuild failed: %s. Request retained.", error.what());
    }
    return true;
}

void AhBot::Update()
{
    if (sWorld.IsShutdowning())
        return;

    if (sWorld.IsStopped())
        return;

    // Carry out whatever the bot thread decided on last pass. This has to come
    // before the nextAICheckTime early-out below, or queued work would only run
    // once every check interval instead of on the next tick.
    if (ProcessPendingRebuild())
        return;
    RunQueuedWork();

    time_t now = time(0);

    if (now < nextAICheckTime)
        return;

    if (updating)
    {
        return;
    }

    bool const rebuilding = rebuildPassesRemaining.load() > 0;
    uint32 const sliceInterval = rebuilding ? 1 : std::max<uint32>(1, sAhBotConfig.updateInterval / MAX_AUCTIONS);
    sLog.outString("[AhBot] Scheduling incremental auction-house check (next in %u seconds)", sliceInterval);
    nextAICheckTime = time(0) + sliceInterval;
    StartUpdate();
    CleanupPropositions();
}

void AhBot::ForceUpdate()
{
    // Includes early returns and exceptions: a failed pass must not leave
    // future updates/rebuilds permanently marked busy.
    struct ReleaseWorker { std::atomic<bool>& busy; ~ReleaseWorker() { busy = false; } } release{updating};
    try
    {
	if (!sAhBotConfig.enabled)
	{
		sLog.outString("[AhBot] ForceUpdate called but AhBot is disabled in ahbot.conf");
		return;
	}

	sLog.outString("[AhBot] === Auction check starting ===");

	if (!allBidders.size())
	{
		sLog.outString("[AhBot] No bidders loaded yet — calling LoadRandomBots");
		LoadRandomBots();
	}

	if (!allBidders.size())
	{
		sLog.outError("[AhBot] No bidders available — cannot post or answer auctions. Check that AhBot.GUID is set to a valid character GUID in ahbot.conf.");
		return;
	}

	sLog.outString("[AhBot] Bidders loaded: %zu total (A=%zu H=%zu N=%zu)",
		allBidders.size(), bidders[1].size(), bidders[2].size(), bidders[3].size());

    uint32 const i = nextHouseIndex++ % MAX_AUCTIONS;
    if (i == 0)
        CheckCategoryMultipliers();

    int answered = 0, added = 0;
    sLog.outString("[AhBot] --- Checking auction house id=%u (incremental %u/%u) ---",
        auctionIds[i], i + 1, MAX_AUCTIONS);
    InAuctionItemsBag inAuctionItems(auctionIds[i]);
    inAuctionItems.Init(true);

    int ahAnswered = 0, ahAdded = 0;
    bool const rebuilding = rebuildPassesRemaining.load() > 0;
    for (int j = 0; j < CategoryList::instance.size(); j++)
    {
        Category* category = CategoryList::instance[j];
        // Like CMaNGOS rebuild, refill sells only; do not immediately buy
        // from the market that is being reconstructed.
        if (!rebuilding)
            ahAnswered += Answer(i, category, &inAuctionItems);
        ahAdded += AddAuctions(i, category, &inAuctionItems);
    }

    sLog.outString("[AhBot] Auction house id=%u: answered=%d added=%d", auctionIds[i], ahAnswered, ahAdded);
    answered += ahAnswered;
    added += ahAdded;

	CleanupHistory();

    sLog.outString("[AhBot] === Incremental check complete: %d answered, %d added ===", answered, added);
    uint32 remaining = rebuildPassesRemaining.load();
    while (remaining > 0 && !rebuildPassesRemaining.compare_exchange_weak(remaining, remaining - 1))
    {
    }
    }
    catch (std::exception const& error)
    {
        sLog.outError("[AhBot] Auction check failed: %s. Pending rebuild/refill retained for retry.", error.what());
    }
}

struct SortByPricePredicate
{
    bool operator()(AuctionSnapshot const & a, AuctionSnapshot const & b) const
    {
        if (a.startbid == b.startbid)
            return a.buyout < b.buyout;

        return a.startbid < b.startbid;
    }
};

std::vector<AuctionSnapshot> AhBot::LoadAuctions(const std::vector<AuctionSnapshot>& auctionEntryMap,
        Category*& category, int& auction)
{
    std::vector<AuctionSnapshot> entries;
    for (std::vector<AuctionSnapshot>::const_iterator itr = auctionEntryMap.begin();
            itr != auctionEntryMap.end(); ++itr)
    {
        const AuctionSnapshot& entry = *itr;
        if (IsBotAuction(entry.owner) || IsBotAuction(entry.bidder))
            continue;

        Item *item = sAuctionMgr.GetAItem(entry.itemGuidLow);
        if (!item)
            continue;

        if (!category->Contains(item->GetProto()))
            continue;

        uint32 price = category->GetPricingStrategy()->GetBuyPrice(item->GetProto(), auctionIds[auction]);
        if (!price || !item->GetCount())
        {
            sLog.outDetail("%s (x%d) in auction %d: price cannot be determined",
                    item->GetProto()->Name1.c_str(), item->GetCount(), auctionIds[auction]);
            continue;
        }

        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), SortByPricePredicate());
    return entries;
}

void AhBot::FindMinPrice(const std::vector<AuctionSnapshot>& auctionEntryMap, const AuctionSnapshot& entry, Item*& item, uint32* minBid,
        uint32* minBuyout)
{
    *minBid = 0;
    *minBuyout = 0;
    for (std::vector<AuctionSnapshot>::const_iterator itr = auctionEntryMap.begin();
            itr != auctionEntryMap.end(); ++itr)
    {
        const AuctionSnapshot& other = *itr;
        if (other.owner == entry.owner)
            continue;

        Item *otherItem = sAuctionMgr.GetAItem(other.itemGuidLow);
        if (!otherItem || !otherItem->GetCount() || !otherItem->GetProto() || otherItem->GetProto()->ItemId != item->GetProto()->ItemId)
            continue;

        uint32 startbid = other.startbid / otherItem->GetCount() * item->GetCount();
        uint32 bid = other.bid / otherItem->GetCount() * item->GetCount();
        uint32 buyout = other.buyout / otherItem->GetCount() * item->GetCount();

        if (!bid && startbid && (!*minBid || *minBid > startbid))
            *minBid = startbid;

        if (bid && (*minBid || *minBid > bid))
            *minBid = bid;

        if (buyout && (!*minBuyout || *minBuyout > buyout))
            *minBuyout = buyout;
    }
}

static int8 InventoryTypeToEquipSlot(uint32 invType)
{
    switch (invType)
    {
        case INVTYPE_HEAD:           return EQUIPMENT_SLOT_HEAD;
        case INVTYPE_NECK:           return EQUIPMENT_SLOT_NECK;
        case INVTYPE_SHOULDERS:      return EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:           return EQUIPMENT_SLOT_CHEST;
        case INVTYPE_WAIST:          return EQUIPMENT_SLOT_WAIST;
        case INVTYPE_LEGS:           return EQUIPMENT_SLOT_LEGS;
        case INVTYPE_FEET:           return EQUIPMENT_SLOT_FEET;
        case INVTYPE_WRISTS:         return EQUIPMENT_SLOT_WRISTS;
        case INVTYPE_HANDS:          return EQUIPMENT_SLOT_HANDS;
        case INVTYPE_FINGER:         return EQUIPMENT_SLOT_FINGER1;
        case INVTYPE_TRINKET:        return EQUIPMENT_SLOT_TRINKET1;
        case INVTYPE_CLOAK:          return EQUIPMENT_SLOT_BACK;
        case INVTYPE_WEAPON:
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND: return EQUIPMENT_SLOT_MAINHAND;
        case INVTYPE_SHIELD:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:       return EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:         return EQUIPMENT_SLOT_RANGED;
        default:                     return -1;
    }
}

static uint32 GetEquippedItemLevel(uint32 botGuid, uint8 slot, uint32& outGuid)
{
    outGuid = 0;
    auto result = CharacterDatabase.PQuery(
        "SELECT ci.item, ii.itemEntry FROM character_inventory ci "
        "JOIN item_instance ii ON ci.item = ii.guid "
        "WHERE ci.guid = '%u' AND ci.bag = 0 AND ci.slot = '%u'",
        botGuid, slot);
    if (!result)
        return 0;

    Field* fields = result->Fetch();
    outGuid = fields[0].GetUInt32();
    uint32 itemEntry = fields[1].GetUInt32();
    delete result;

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemEntry);
    return proto ? proto->ItemLevel : 0;
}

bool AhBot::TryEquipItem(uint32 bidder, uint32 itemGuidLow, ItemPrototype const* proto)
{
    int8 primarySlot = InventoryTypeToEquipSlot(proto->InventoryType);
    if (primarySlot < 0)
        return false;

    auto charResult = CharacterDatabase.PQuery(
        "SELECT race, class, level FROM characters WHERE guid = '%u'", bidder);
    if (!charResult)
        return false;

    Field* charFields = charResult->Fetch();
    uint32 race  = charFields[0].GetUInt32();
    uint32 cls   = charFields[1].GetUInt32();
    uint32 level = charFields[2].GetUInt32();
    delete charResult;

    if (proto->RequiredLevel && level < proto->RequiredLevel)
        return false;
    if (proto->AllowableClass && !(proto->AllowableClass & (1 << (cls - 1))))
        return false;
    if (proto->AllowableRace && !(proto->AllowableRace & (1 << (race - 1))))
        return false;

    // For rings and trinkets pick the slot with the lower-level current item
    bool isDualSlot = (proto->InventoryType == INVTYPE_FINGER || proto->InventoryType == INVTYPE_TRINKET);
    uint8 slot = (uint8)primarySlot;
    uint32 currentGuid = 0;
    uint32 currentLevel = GetEquippedItemLevel(bidder, slot, currentGuid);

    if (isDualSlot)
    {
        uint8 altSlot = (proto->InventoryType == INVTYPE_FINGER) ? EQUIPMENT_SLOT_FINGER2 : EQUIPMENT_SLOT_TRINKET2;
        uint32 altGuid = 0;
        uint32 altLevel = GetEquippedItemLevel(bidder, altSlot, altGuid);
        if (altLevel < currentLevel)
        {
            slot = altSlot;
            currentGuid = altGuid;
            currentLevel = altLevel;
        }
    }

    if (proto->ItemLevel <= currentLevel)
        return false;

    sLog.outString("[AhBot] Equipping upgrade on bot guid=%u slot=%u: %s ilvl=%u (was ilvl=%u)",
            bidder, slot, proto->Name1.c_str(), proto->ItemLevel, currentLevel);

    CharacterDatabase.BeginTransaction();
    // Delete all item_instances for everything currently in this slot, then clear the slot.
    // A slot may have multiple rows if inventory was previously corrupted; clean them all.
    CharacterDatabase.PExecute(
        "DELETE ii FROM item_instance ii "
        "JOIN character_inventory ci ON ci.item = ii.guid "
        "WHERE ci.guid = '%u' AND ci.bag = 0 AND ci.slot = '%u'",
        bidder, slot);
    CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE guid='%u' AND bag=0 AND slot='%u'", bidder, slot);
    CharacterDatabase.PExecute("UPDATE item_instance SET owner_guid='%u' WHERE guid='%u'", bidder, itemGuidLow);
    CharacterDatabase.PExecute("INSERT INTO character_inventory (guid, bag, slot, item, item_template) VALUES ('%u', 0, '%u', '%u', '%u')",
            bidder, slot, itemGuidLow, proto->ItemId);
    CharacterDatabase.CommitTransaction();

    return true;
}

int AhBot::Answer(int auction, Category* category, ItemBag* inAuctionItems)
{
    const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
    if (!ahEntry)
        return 0;

    int answered = 0;
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    std::vector<AuctionSnapshot> auctionEntryMap = auctionHouse->GetAuctionsSnapshot();
    int64 availableMoney = GetAvailableMoney(auctionIds[auction]);

    std::vector<AuctionSnapshot> entries = LoadAuctions(auctionEntryMap, category, auction);
    sLog.outDetail("[AhBot] Answer AH %u category %s: scanning %zu entries, money=%ld",
            auctionIds[auction], category->GetName().c_str(), entries.size(), availableMoney);

    for (std::vector<AuctionSnapshot>::const_iterator itr = entries.begin(); itr != entries.end(); ++itr)
    {
        const AuctionSnapshot& snap = *itr;
        uint32 owner = snap.owner;
        if (owner == sAhBotConfig.guid)
            continue;

        uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(ObjectGuid(HIGHGUID_PLAYER, owner));
        if (!account)
        {
            sLog.outDetail("[AhBot] Skipping entry %u (owner guid=%u): account lookup failed — owner not in DB?",
                    snap.Id, owner);
            continue;
        }
        if (sPlayerbotAIConfig.IsInRandomAccountList(account))
        {
            sLog.outDetail("[AhBot] Skipping entry %u (owner guid=%u account=%u): owner is a bot account",
                    snap.Id, owner, account);
            continue;
        }

        Item *item = sAuctionMgr.GetAItem(snap.itemGuidLow);
        if (!item || !item->GetCount())
        {
            sLog.outString("[AhBot] Skipping entry %u from real player (guid=%u account=%u): item not found in aitem map",
                    snap.Id, owner, account);
            continue;
        }

        const ItemPrototype* proto = item->GetProto();
        sLog.outString("[AhBot] Evaluating %s (x%d) entry=%u from real player (guid=%u account=%u) AH=%u startbid=%u buyout=%u",
                proto->Name1.c_str(), item->GetCount(), snap.Id, owner, account, auctionIds[auction],
                snap.startbid, snap.buyout);

        std::vector<uint32> items = availableItems.Get(category);
        if (std::find(items.begin(), items.end(), proto->ItemId) == items.end())
        {
            sLog.outString("[AhBot] SKIP %s (x%d): not in bot's available item pool for category %s",
                    proto->Name1.c_str(), item->GetCount(), category->GetName().c_str());
            continue;
        }

        uint32 answerCount = GetAnswerCount(proto->ItemId, auctionIds[auction], sAhBotConfig.itemBuyMaxInterval);
        uint32 maxAnswerCount = category->GetMaxAllowedItemAuctionCount(proto);
        if (maxAnswerCount && answerCount > maxAnswerCount)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): already answered %u times (max=%u) within interval",
                    proto->Name1.c_str(), item->GetCount(), answerCount, maxAnswerCount);
            continue;
        }

        if (proto->RequiredLevel > sAhBotConfig.maxRequiredLevel || proto->ItemLevel > sAhBotConfig.maxItemLevel)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): reqLevel=%u itemLevel=%u exceeds max (reqLevel<=%u itemLevel<=%u)",
                    proto->Name1.c_str(), item->GetCount(),
                    proto->RequiredLevel, proto->ItemLevel,
                    sAhBotConfig.maxRequiredLevel, sAhBotConfig.maxItemLevel);
            continue;
        }

        std::ostringstream priceExplain;
        uint32 price = category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction], &priceExplain);
        if (!price)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): buy price is 0 (%s)",
                    proto->Name1.c_str(), item->GetCount(), priceExplain.str().c_str());
            continue;
        }

        uint32 bidPrice = item->GetCount() * price;
        uint32 buyoutPrice = item->GetCount() * urand(price, 4 * price / 3);

        uint32 curPrice = snap.bid;
        if (!curPrice) curPrice = snap.startbid;
        if (!curPrice) curPrice = snap.buyout;

        uint32 bidder = GetRandomBidder(auctionIds[auction]);
        if (!bidder)
        {
            sLog.outError("[AhBot] No bidders for auction %d", auctionIds[auction]);
            break;
        }

        if (curPrice > buyoutPrice)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): listing price %u > bot max price %u (price/unit=%u)",
                    proto->Name1.c_str(), item->GetCount(), curPrice, buyoutPrice, price);
            CheckSendMail(bidder, buyoutPrice, snap);
            continue;
        }

        if (availableMoney < (int64)curPrice)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): listing price %u > available money %ld",
                    proto->Name1.c_str(), item->GetCount(), curPrice, availableMoney);
            continue;
        }

        uint32 minBid = 0, minBuyout = 0;
        FindMinPrice(auctionEntryMap, snap, item, &minBid, &minBuyout);

        if (minBid && snap.bid && minBid < snap.bid)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): current bid %u > cheaper listing %u (minBid)",
                    proto->Name1.c_str(), item->GetCount(), snap.bid, minBid);
            continue;
        }

        if (minBid && snap.startbid && minBid < snap.startbid)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): startbid %u > cheaper listing %u (minBid)",
                    proto->Name1.c_str(), item->GetCount(), snap.startbid, minBid);
            CheckSendMail(bidder, minBid, snap);
            continue;
        }

        double priceLevel = (double)curPrice / (double)buyoutPrice;
        uint32 buytime = GetBuyTime(snap.Id, proto->ItemId, auctionIds[auction], category, priceLevel);
        if (time(0) < buytime)
        {
            sLog.outString("[AhBot] SKIP %s (x%d): buy delay not expired, will act in %ld seconds",
                    proto->Name1.c_str(), item->GetCount(), (long)(buytime - time(0)));
            continue;
        }

        // Do not complete the purchase here. This runs on the bot thread, and
        // paying the seller reaches into mail, their session and possibly their
        // live Player object. Record the decision; the world thread executes it
        // from AhBot::Update().
        PendingPurchase pending;
        pending.auctionId   = snap.Id;
        pending.bidder      = bidder;
        pending.bidAmount   = curPrice + urand(1, 1 + bidPrice / 10);
        pending.unitPrice   = price;
        pending.minBuyout   = minBuyout;
        pending.houseIndex  = auction;
        {
            std::lock_guard<std::mutex> g(queuedWorkMutex);
            queuedPurchases.push_back(pending);
        }

        availableMoney -= curPrice;

        sLog.outString("[AhBot] Queued buy: %dx %s on AH %u for %u (bidder guid=%u)",
                item->GetCount(), proto->Name1.c_str(), auctionIds[auction], pending.bidAmount, bidder);

        answered++;
    }

    return answered;
}

uint32 AhBot::GetTime(std::string category, uint32 id, uint32 auctionHouse, uint32 type)
{
    auto results = CharacterDatabase.PQuery("SELECT MAX(buytime) FROM ahbot_history WHERE item = '%u' AND won = '%u' AND auction_house = '%u' AND category = '%s'",
        id, type, factions[auctionHouse], category.c_str());
    std::unique_ptr<QueryResult> results_guard(results);

    if (!results)
        return 0;

    Field* fields = results->Fetch();
    uint32 result = fields[0].GetUInt32();

    return result;
}

void AhBot::SetTime(std::string category, uint32 id, uint32 auctionHouse, uint32 type, uint32 value)
{
    CharacterDatabase.PExecute("DELETE FROM ahbot_history WHERE item = '%u' AND won = '%u' AND auction_house = '%u' AND category = '%s'",
        id, type, factions[auctionHouse], category.c_str());

    CharacterDatabase.PExecute("INSERT INTO ahbot_history (buytime, item, bid, buyout, category, won, auction_house) "
        "VALUES ('%u', '%u', '%u', '%u', '%s', '%u', '%u')",
        value, id, 0, 0,
        category.c_str(), type, factions[auctionHouse]);
}

uint32 AhBot::GetBuyTime(uint32 entry, uint32 itemId, uint32 auctionHouse, Category*& category, double priceLevel)
{
    uint32 entryTime = GetTime("entry", entry, auctionHouse, AHBOT_WON_DELAY);
    if (entryTime > time(0))
        return entryTime;

    uint32 result = entryTime;

    std::string categoryName = category->GetName();
    uint32 categoryTime = GetTime(categoryName, 0, auctionHouse, AHBOT_WON_DELAY);
    uint32 itemTime = GetTime("item", itemId, auctionHouse, AHBOT_WON_DELAY);

    if (categoryTime < time(0)) categoryTime = time(0);
    if (itemTime < time(0)) itemTime = time(0);

    double rarity = category->GetPricingStrategy()->GetRarityPriceMultiplier(itemId);
    categoryTime += urand(sAhBotConfig.itemBuyMinInterval, sAhBotConfig.itemBuyMaxInterval) * priceLevel;
    itemTime += urand(sAhBotConfig.itemBuyMinInterval, sAhBotConfig.itemBuyMaxInterval) * priceLevel / rarity;
    entryTime = std::max(categoryTime, itemTime);

    SetTime(categoryName, 0, auctionHouse, AHBOT_WON_DELAY, categoryTime);
    SetTime("item", itemId, auctionHouse, AHBOT_WON_DELAY, itemTime);
    SetTime("entry", entry, auctionHouse, AHBOT_WON_DELAY, entryTime);

    return result ? result : entryTime;
}

uint32 AhBot::GetSellTime(uint32 itemId, uint32 auctionHouse, Category*& category)
{
    uint32 itemSellTime = GetTime("item", itemId, auctionHouse, AHBOT_SELL_DELAY);
    uint32 itemBuyTime = GetTime("item", itemId, auctionHouse, AHBOT_WON_DELAY);
    uint32 itemTime = std::max(itemSellTime, itemBuyTime);

    if (itemTime > time(0))
        return itemTime;

    uint32 result = itemTime;

    std::string categoryName = category->GetDisplayName();
    uint32 categorySellTime = GetTime(categoryName, 0, auctionHouse, AHBOT_SELL_DELAY);
    uint32 categoryBuyTime = GetTime(categoryName, 0, auctionHouse, AHBOT_WON_DELAY);
    uint32 categoryTime = std::max(categorySellTime, categoryBuyTime);

    if (categoryTime < time(0)) categoryTime = time(0);
    if (itemTime < time(0)) itemTime = time(0);

    double rarity = category->GetPricingStrategy()->GetRarityPriceMultiplier(itemId);
    categoryTime += urand(sAhBotConfig.itemSellMinInterval, sAhBotConfig.itemSellMaxInterval);
    itemTime += urand(sAhBotConfig.itemSellMinInterval, sAhBotConfig.itemSellMaxInterval) * rarity;
    itemTime = std::max(itemTime, categoryTime);

    SetTime(categoryName, 0, auctionHouse, AHBOT_SELL_DELAY, categoryTime);
    SetTime("item", itemId, auctionHouse, AHBOT_SELL_DELAY, itemTime);

    return result ? result : itemTime;
}

int AhBot::AddAuctions(int auction, Category* category, ItemBag* inAuctionItems)
{
    std::vector<uint32>& inAuction = inAuctionItems->Get(category);

    int32 maxAllowedAuctionCount = categoryMaxAuctionCount[category->GetDisplayName()];
    if (inAuctionItems->GetCount(category) >= maxAllowedAuctionCount)
    {
        sLog.outDetail("[AhBot] Category '%s' on AH %u: at cap (%d/%d), skipping",
            category->GetDisplayName().c_str(), auctionIds[auction],
            inAuctionItems->GetCount(category), maxAllowedAuctionCount);
        return 0;
    }

    int added = 0;
    int ladded = 0;
    bool const rebuilding = rebuildPassesRemaining.load() > 0;
    std::vector<uint32> available = availableItems.Get(category);
    // A rebuild fills immediately; ordinary passes retain their sale pacing.
    // Retire exhausted/rejected candidates so a few capped items cannot use
    // the entire refill attempt budget while other eligible items remain.
    size_t const attempts = rebuilding ? size_t(maxAllowedAuctionCount) + available.size() : size_t(maxAllowedAuctionCount) + 1;
    for (size_t i = 0; i < attempts && !available.empty() && inAuctionItems->GetCount(category) < maxAllowedAuctionCount; ++i)
    {
        uint32 index = urand(0, available.size() - 1);
        uint32 itemId = available[index];

        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            available.erase(available.begin() + index);
            continue;
        }

        ItemOverride overrideData;
        if (GetItemOverride(itemId, overrideData) && overrideData.addChance &&
            urand(1, 100) > overrideData.addChance)
            continue;

        int32 maxAllowedItems = category->GetMaxAllowedItemAuctionCount(proto);
        if (maxAllowedItems && inAuctionItems->GetCount(category, proto->ItemId) >= maxAllowedItems)
        {
            sLog.outDetail("%s in auction %d: has reached max %d/%d",
                proto->Name1.c_str(), auctionIds[auction], inAuctionItems->GetCount(category, proto->ItemId), maxAllowedItems);
            available.erase(available.begin() + index);
            continue;
        }

        uint32 sellTime = rebuilding ? uint32(time(0)) : GetSellTime(proto->ItemId, auctionIds[auction], category);
        if (time(0) - sellTime < 0)
        {
            ladded += 1;
            sLog.outDetail( "%s in auction %d: will add in %ld seconds",
                    proto->Name1.c_str(), auctionIds[auction], sellTime - time(0));
            continue;
        }
        else if (time(0) - sellTime > sAhBotConfig.maxSellInterval)
        {
            sLog.outDetail( "%s in auction %d: too old (%ld secs)",
                    proto->Name1.c_str(), auctionIds[auction], time(0) - sellTime);
            continue;
        }
        int const listed = AddAuction(auction, category, proto);
        if (listed)
        {
            inAuctionItems->Add(proto);
            added += listed;
        }
        else
            available.erase(available.begin() + index);
    }

    if (added > 0 || ladded > 0)
        sLog.outString("[AhBot] Category '%s' on AH %u: %d new listing(s), %d pending (sell delay not elapsed)",
            category->GetDisplayName().c_str(), auctionIds[auction], added, ladded);


    return added;
}

int AhBot::AddAuction(int auction, Category* category, ItemPrototype const* proto)
{
    uint32 owner = GetRandomBidder(auctionIds[auction]);
    if (!owner)
    {
        sLog.outError("[AhBot] No valid bidder found for auction house %u — cannot list %s", auctionIds[auction], proto->Name1.c_str());
        return 0;
    }

    std::string name;
    if (!sObjectMgr.GetPlayerNameByGUID(ObjectGuid(HIGHGUID_PLAYER, owner), name))
    {
        sLog.outError("[AhBot] Owner GUID %u has no character record — cannot list %s", owner, proto->Name1.c_str());
        return 0;
    }

    uint32 price = category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction]);

    updateMarketPrice(proto->ItemId, price, auctionIds[auction]);

    price = category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction]);

    uint32 stackCount = urand(1, category->GetStackCount(proto));
    ItemOverride overrideData;
    if (GetItemOverride(proto->ItemId, overrideData) && overrideData.minAmount)
        stackCount = urand(overrideData.minAmount, overrideData.maxAmount);
    if (!price || !stackCount)
        return 0;

    if (price > sAhBotConfig.stackReducePrice)
        stackCount /= (price / sAhBotConfig.stackReducePrice);

    if (!stackCount)
        stackCount = 1;

    if (urand(0, 100) <= sAhBotConfig.underPriceProbability * 100)
        price = price * 100 / urand(100, 200);

    uint32 bidPrice = PricingStrategy::RoundPrice(stackCount * price);
    uint32 buyoutPrice = PricingStrategy::RoundPrice(stackCount * urand(price, 4 * price / 3));

    Item* item = Item::CreateItem(proto->ItemId, stackCount);
    if (!item)
        return 0;

    uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(proto->ItemId);
    if (randomPropertyId)
        item->SetItemRandomProperties(randomPropertyId);
    item->ClearUpdateMask(false);

    AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
    if (!ahEntry)
        return 0;

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);

    uint32 auction_time = uint32(urand(8, 24) * HOUR * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME));

    AuctionEntry* auctionEntry = new AuctionEntry();
    auctionEntry->Id = sObjectMgr.GenerateAuctionID();
    auctionEntry->itemGuidLow = item->GetObjectGuid().GetCounter();
    auctionEntry->itemTemplate = item->GetEntry();
    auctionEntry->itemCount = item->GetCount();
    auctionEntry->itemRandomPropertyId = item->GetItemRandomPropertyId();
    auctionEntry->owner = owner;
    auctionEntry->ownerAccount = sObjectMgr.GetPlayerAccountIdByGUID(ObjectGuid(HIGHGUID_PLAYER, owner));
    auctionEntry->startbid = bidPrice;
    auctionEntry->bidder = 0;
    auctionEntry->bid = 0;
    auctionEntry->buyout = buyoutPrice;
    auctionEntry->expireTime = time(nullptr) + auction_time;
    //auctionEntry->moneyDeliveryTime = 0;
    auctionEntry->deposit = 0;
    auctionEntry->auctionHouseEntry = ahEntry;

    auctionHouse->AddAuction(auctionEntry);


    sAuctionMgr.AddAItem(item);

    item->SaveToDB();
    auctionEntry->SaveToDB();

    sLog.outString("[AhBot] Listed: %dx %s on AH %u for %ug%us..%ug%us (owner: %s guid=%u)",
        stackCount, proto->Name1.c_str(), auctionIds[auction],
        bidPrice / 10000, (bidPrice % 10000) / 100,
        buyoutPrice / 10000, (buyoutPrice % 10000) / 100,
        name.c_str(), owner);
    return 1;
}

bool AhBot::HandleCommand(ChatHandler* handler, std::string command)
{
    command.erase(0, command.find_first_not_of(" \t\r\n"));
    size_t const last = command.find_last_not_of(" \t\r\n");
    if (last == std::string::npos)
        command.clear();
    else
        command.erase(last + 1);

    std::istringstream input(command);
    std::string action;
    std::string option;
    std::string extra;
    input >> action >> option >> extra;
    std::transform(action.begin(), action.end(), action.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::transform(option.begin(), option.end(), option.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    if (action == "reload")
    {
        if (!option.empty())
        {
            handler->SendSysMessage("Usage: .ahbot reload");
            return false;
        }
        if (updating.load() || pendingRebuild.load() || rebuildPassesRemaining.load())
        {
            handler->SendSysMessage("AHBot reload refused: an update or rebuild is active/pending. Try again after .ahbot status reports idle.");
            return false;
        }
        if (!sAhBotConfig.Reload())
        {
            handler->SendSysMessage("AHBot reload failed: ahbot.conf could not be parsed. The previous runtime settings remain active.");
            return false;
        }

        rebuildPassesRemaining = 0;
        LoadItemOverrides();
        availableItems.Init(true);
        categoryMultipliers.clear();
        categoryMaxAuctionCount.clear();
        categoryMaxItemAuctionCount.clear();
        categoryMultiplierExpireTimes.clear();
        bidders.clear();
        allBidders.clear();
        nextHouseIndex = 0;
        nextAICheckTime = time(0);

        handler->PSendSysMessage("AHBot configuration reloaded: %s, interval %us, sell delay %u-%us, max item/required level %u/%u.",
            sAhBotConfig.enabled ? "enabled" : "disabled", sAhBotConfig.updateInterval,
            sAhBotConfig.itemSellMinInterval, sAhBotConfig.itemSellMaxInterval,
            sAhBotConfig.maxItemLevel, sAhBotConfig.maxRequiredLevel);
        return true;
    }

    if (action == "rebuild" || action == "expire")
    {
        bool const includePlayerBids = option == "all";
        if ((!option.empty() && !includePlayerBids) || !extra.empty())
        {
            handler->SendSysMessage("Usage: .ahbot rebuild [all]");
            return false;
        }
        if (!sAhBotConfig.enabled)
        {
            handler->SendSysMessage("AHBot rebuild refused: AhBot is disabled. Change ahbot.conf and run .ahbot reload first.");
            return false;
        }
        if (!QueueRebuild(includePlayerBids))
        {
            handler->SendSysMessage("AHBot rebuild already in progress; the existing refill will continue.");
            return true;
        }
        handler->PSendSysMessage("AHBot rebuild accepted: waiting for the current check to finish; player bids %s. See .ahbot status for progress.",
            includePlayerBids ? "included (all requested)" : "protected");
        sLog.outString("[AhBot] Rebuild request accepted (include player bids=%u, worker busy=%u).",
            includePlayerBids ? 1u : 0u, updating.load() ? 1u : 0u);
        handler->SendSysMessage("After the current check finishes, expiry takes about one minute, followed by three background refill passes. No need to submit rebuild again.");
        return true;
    }

    if (action == "status" || action == "stats")
    {
        if ((!option.empty() && option != "all") || !extra.empty())
        {
            handler->SendSysMessage("Usage: .ahbot status [all]");
            return false;
        }
        PrintStatus(handler, option == "all");
        return true;
    }

    if (action == "update")
    {
        if (!sAhBotConfig.enabled)
        {
            handler->SendSysMessage("AHBot is disabled.");
            return false;
        }
        if (updating.load())
        {
            handler->SendSysMessage("AHBot update already running.");
            return false;
        }
        if (rebuildPassesRemaining.load())
        {
            handler->SendSysMessage("AHBot update not needed: a rebuild refill is already queued.");
            return false;
        }
        if (!StartUpdate())
        {
            handler->SendSysMessage("AHBot update could not start: worker busy, rebuild pending or worker launch failed.");
            return false;
        }
        handler->SendSysMessage("AHBot update requested.");
        return true;
    }

    if (action == "dump")
    {
        Dump();
        handler->SendSysMessage("AHBot item-price dump written to the world-server log.");
        return true;
    }

    // CMaNGOS spells this `.ahbot item ...`. Keep the old Turtle `.ahbot <id>`
    // query spelling as a compatibility alias.
    if (action == "item")
    {
        size_t const separator = command.find_first_of(" \t");
        return HandleItemCommand(handler, separator == std::string::npos ? "" : command.substr(separator + 1));
    }

    uint32 itemId = atoi(action.c_str());
    if (!itemId)
    {
        handler->SendSysMessage("AHBot commands:");
        handler->SendSysMessage(".ahbot reload - reload ahbot.conf safely");
        handler->SendSysMessage(".ahbot rebuild [all] - expire bot auctions and refill; 'all' also expires those with player bids");
        handler->SendSysMessage(".ahbot status [all] - show auction counts and runtime state");
        handler->SendSysMessage(".ahbot update - request one incremental market pass");
        handler->SendSysMessage(".ahbot item <itemId> [value [chance [min [max]]]] [reset] - inspect or override an item");
        return false;
    }

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto)
    {
        handler->PSendSysMessage("AHBot: item %u does not exist.", itemId);
        return false;
    }

    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (category->Contains(proto))
        {
            std::vector<uint32> items = availableItems.Get(category);
            if (std::find(items.begin(), items.end(), proto->ItemId) == items.end())
                continue;

            std::ostringstream out;
            out << proto->Name1.c_str() << " (" << category->GetDisplayName() << "), "
                    << category->GetMaxAllowedAuctionCount() << "x" << category->GetMaxAllowedItemAuctionCount(proto)
                    << "x" << category->GetStackCount(proto) << " max"
                    << "\n";
            for (int auction = 0; auction < MAX_AUCTIONS; auction++)
            {
                const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
                out << "--- auction house " << auctionIds[auction] << "(faction: " << factions[auctionIds[auction]] << ", money: "
                    << GetAvailableMoney(auctionIds[auction])
                    << ") ---\n";

                std::ostringstream exp1;
                out << "sell: " << ChatHelper::formatMoney(category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction], true, &exp1));
                out << " ("  << exp1.str().c_str() << ")\n";

                std::ostringstream exp2;
                out << "buy: " << ChatHelper::formatMoney(category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction], &exp2));
                out << " ("  << exp2.str().c_str() << ")\n";

                out << "market: " << ChatHelper::formatMoney(category->GetPricingStrategy()->GetMarketPrice(proto->ItemId, auctionIds[auction]))
                    << "\n";
            }
            sLog.outString("%s",out.str().c_str());
            handler->PSendSysMessage("%s", out.str().c_str());
        }
    }
    return true;
}

void AhBot::LoadItemOverrides()
{
    itemOverrides.clear();
    std::unique_ptr<QueryResult> result(CharacterDatabase.Query(
        "SELECT item, value, add_chance, min_amount, max_amount FROM ahbot_items"));
    if (!result)
    {
        sLog.outString("[AhBot] No item overrides loaded (ahbot_items is empty or unavailable)");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        ItemOverride data;
        data.value = fields[1].GetUInt32();
        data.addChance = std::min<uint32>(100, fields[2].GetUInt32());
        data.minAmount = fields[3].GetUInt32();
        data.maxAmount = fields[4].GetUInt32();
        itemOverrides[fields[0].GetUInt32()] = data;
    }
    while (result->NextRow());

    sLog.outString("[AhBot] Loaded %u item override(s)", (uint32)itemOverrides.size());
}

bool AhBot::GetItemOverride(uint32 itemId, ItemOverride& data) const
{
    std::map<uint32, ItemOverride>::const_iterator itr = itemOverrides.find(itemId);
    if (itr == itemOverrides.end())
        return false;
    data = itr->second;
    return true;
}

bool AhBot::IsItemBanned(uint32 itemId) const
{
    ItemOverride data;
    return GetItemOverride(itemId, data) && data.value == 0;
}

bool AhBot::HandleItemCommand(ChatHandler* handler, std::string const& arguments)
{
    std::istringstream input(arguments);
    std::string itemToken;
    std::string valueToken;
    std::string chanceToken;
    std::string minToken;
    std::string maxToken;
    std::string extra;
    input >> itemToken >> valueToken >> chanceToken >> minToken >> maxToken >> extra;

    size_t const link = itemToken.find("Hitem:");
    char const* number = link == std::string::npos ? itemToken.c_str() : itemToken.c_str() + link + 6;
    uint32 const itemId = (uint32)std::strtoul(number, nullptr, 10);
    ItemPrototype const* proto = itemId ? sObjectMgr.GetItemPrototype(itemId) : nullptr;
    if (!proto)
    {
        handler->SendSysMessage("Usage: .ahbot item <itemId> [value [chance [min [max]]]] [reset]");
        return false;
    }

    auto parseUInt = [](std::string const& token, uint32& value) -> bool
    {
        if (token.empty() || token.find_first_not_of("0123456789") != std::string::npos)
            return false;
        unsigned long long const parsed = std::strtoull(token.c_str(), nullptr, 10);
        if (parsed > std::numeric_limits<uint32>::max())
            return false;
        value = (uint32)parsed;
        return true;
    };

    std::string loweredValue = valueToken;
    std::transform(loweredValue.begin(), loweredValue.end(), loweredValue.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (loweredValue == "reset")
    {
        if (!chanceToken.empty() || !minToken.empty() || !maxToken.empty() || !extra.empty())
        {
            handler->SendSysMessage("Usage: .ahbot item <itemId> reset");
            return false;
        }
        if (updating.load())
        {
            handler->SendSysMessage("AHBot item reset refused while an auction update is running.");
            return false;
        }
        CharacterDatabase.PExecute("DELETE FROM ahbot_items WHERE item = '%u'", itemId);
        itemOverrides.erase(itemId);
        availableItems.Init(true);
        handler->PSendSysMessage("AHBot override reset for %s (%u).", proto->Name1, itemId);
        return true;
    }

    if (!valueToken.empty())
    {
        if (!extra.empty())
        {
            handler->SendSysMessage("Usage: .ahbot item <itemId> [value [chance [min [max]]]] [reset]");
            return false;
        }

        uint32 value = 0;
        uint32 chance = 0;
        uint32 minAmount = 0;
        uint32 maxAmount = 0;
        if (!parseUInt(valueToken, value) ||
            (!chanceToken.empty() && !parseUInt(chanceToken, chance)) ||
            (!minToken.empty() && !parseUInt(minToken, minAmount)) ||
            (!maxToken.empty() && !parseUInt(maxToken, maxAmount)))
        {
            handler->SendSysMessage("AHBot item values must be non-negative whole numbers.");
            return false;
        }
        if (updating.load())
        {
            handler->SendSysMessage("AHBot item update refused while an auction update is running.");
            return false;
        }

        ItemOverride data;
        data.value = value;
        data.addChance = chanceToken.empty() ? 0 : std::min<uint32>(100, chance);
        uint32 const maxStack = std::max<uint32>(1, proto->Stackable);
        data.minAmount = minToken.empty() ? 1 : std::max<uint32>(1, minAmount);
        data.maxAmount = maxToken.empty() ? maxStack : std::max<uint32>(1, maxAmount);
        data.minAmount = std::min(data.minAmount, maxStack);
        data.maxAmount = std::min(data.maxAmount, maxStack);
        if (data.maxAmount < data.minAmount)
            std::swap(data.minAmount, data.maxAmount);

        CharacterDatabase.PExecute(
            "REPLACE INTO ahbot_items (item, value, add_chance, min_amount, max_amount) VALUES ('%u','%u','%u','%u','%u')",
            itemId, data.value, data.addChance, data.minAmount, data.maxAmount);
        itemOverrides[itemId] = data;
        availableItems.Init(true);
        handler->PSendSysMessage("AHBot override saved for %s (%u): value %u, chance %u%%, stack %u-%u.",
            proto->Name1, itemId, data.value, data.addChance, data.minAmount, data.maxAmount);
        if (!data.value)
            handler->SendSysMessage("Value 0 bans this item from AHBot buying and new listings.");
        return true;
    }

    ItemOverride data;
    if (GetItemOverride(itemId, data))
        handler->PSendSysMessage("AHBot item %s (%u): override value %u, chance %u%%, stack %u-%u.",
            proto->Name1, itemId, data.value, data.addChance, data.minAmount, data.maxAmount);
    else
        handler->PSendSysMessage("AHBot item %s (%u): no override; native Turtle pricing and category rules apply.", proto->Name1, itemId);

    // Preserve Turtle's useful per-category and market-price diagnostics.
    return HandleCommand(handler, itemToken);
}

bool AhBot::IsBotOwner(uint32 guid, uint32 accountId)
{
    return guid == (uint32)sAhBotConfig.guid || (accountId && sPlayerbotAIConfig.IsInRandomAccountList(accountId));
}

bool AhBot::IsBotCharacter(uint32 guid)
{
    if (!guid)
        return false;
    if (guid == (uint32)sAhBotConfig.guid || allBidders.find(guid) != allBidders.end())
        return true;

    uint32 const accountId = sObjectMgr.GetPlayerAccountIdByGUID(ObjectGuid(HIGHGUID_PLAYER, guid));
    return accountId && sPlayerbotAIConfig.IsInRandomAccountList(accountId);
}

uint32 AhBot::Rebuild(bool includePlayerBids, uint32& protectedPlayerBids)
{
    // No producer is active here. Decisions about old listings must not buy
    // or advertise them while the native expiry sweep removes them.
    {
        std::lock_guard<std::mutex> guard(queuedWorkMutex);
        queuedPurchases.clear();
        queuedPropositions.clear();
    }
    uint32 expired = 0;
    protectedPlayerBids = 0;
    std::set<AuctionHouseObject*> visited;
    time_t const now = sWorld.GetGameTime();

    for (int auction = 0; auction < MAX_AUCTIONS; ++auction)
    {
        AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
        if (!ahEntry)
            continue;
        AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
        if (!auctionHouse || !visited.insert(auctionHouse).second)
            continue;

        AuctionHouseObject::Guard g(auctionHouse->GetLock());
        AuctionHouseObject::AuctionEntryMapBounds bounds = auctionHouse->GetAuctionsBounds_locked();
        for (AuctionHouseObject::AuctionEntryMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            AuctionEntry* entry = itr->second;
            if (!entry || !IsBotOwner(entry->owner, entry->ownerAccount))
                continue;

            bool const hasPlayerBid = entry->bidder && !IsBotCharacter(entry->bidder);
            if (hasPlayerBid && !includePlayerBids)
            {
                ++protectedPlayerBids;
                continue;
            }

            entry->expireTime = now;
            ++expired;
        }
    }

    CharacterDatabase.PExecute("DELETE FROM ahbot_category");
    CharacterDatabase.PExecute("UPDATE ahbot_history SET buytime = buytime - 86400 WHERE won = '%u'", AHBOT_SELL_DELAY);
    categoryMultipliers.clear();
    categoryMaxAuctionCount.clear();
    categoryMaxItemAuctionCount.clear();
    categoryMultiplierExpireTimes.clear();
    nextHouseIndex = 0;
    rebuildPassesRemaining = MAX_AUCTIONS;
    // AuctionHouseMgr removes expired entries once per minute. Waiting a little
    // over that interval prevents the refill pass from seeing the old stock.
    nextAICheckTime = time(0) + 65;
    sLog.outString("[AhBot] Rebuild queued: %u bot auctions expired, %u player-bid auctions preserved", expired, protectedPlayerBids);
    return expired;
}

void AhBot::PrintStatus(ChatHandler* handler, bool detailed)
{
    uint32 queuedPurchasesCount = 0;
    uint32 queuedPropositionsCount = 0;
    {
        std::lock_guard<std::mutex> guard(queuedWorkMutex);
        queuedPurchasesCount = (uint32)queuedPurchases.size();
        queuedPropositionsCount = (uint32)queuedPropositions.size();
    }

    time_t const now = time(0);
    uint32 const nextSeconds = nextAICheckTime > now ? (uint32)(nextAICheckTime - now) : 0;
    if (pendingRebuild.load())
        handler->SendSysMessage("AHBot rebuild: REQUEST ACCEPTED, waiting for the current worker; auctions have not been expired yet.");
    handler->PSendSysMessage("AHBot: %s; worker %s; next check in %us; rebuild passes %u; queued purchases/propositions %u/%u.",
        sAhBotConfig.enabled ? "enabled" : "disabled", updating.load() ? "running" : "idle",
        nextSeconds, rebuildPassesRemaining.load(), queuedPurchasesCount, queuedPropositionsCount);

    std::set<AuctionHouseObject*> visited;
    uint32 total = 0;
    uint32 botOwned = 0;
    uint32 playerOwned = 0;
    uint32 protectedPlayerBids = 0;

    for (int auction = 0; auction < MAX_AUCTIONS; ++auction)
    {
        AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
        if (!ahEntry)
            continue;
        AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
        if (!auctionHouse || !visited.insert(auctionHouse).second)
            continue;

        std::vector<AuctionSnapshot> const entries = auctionHouse->GetAuctionsSnapshot();
        uint32 marketBotOwned = 0;
        uint32 marketPlayerOwned = 0;
        uint32 marketPlayerBids = 0;
        for (std::vector<AuctionSnapshot>::const_iterator itr = entries.begin(); itr != entries.end(); ++itr)
        {
            if (IsBotOwner(itr->owner, itr->ownerAccount))
            {
                ++marketBotOwned;
                if (itr->bidder && !IsBotCharacter(itr->bidder))
                    ++marketPlayerBids;
            }
            else
                ++marketPlayerOwned;
        }

        total += (uint32)entries.size();
        botOwned += marketBotOwned;
        playerOwned += marketPlayerOwned;
        protectedPlayerBids += marketPlayerBids;
        handler->PSendSysMessage("AH market containing house %u: %u total, %u bot, %u player, %u bot listings with player bids.",
            auctionIds[auction], (uint32)entries.size(), marketBotOwned, marketPlayerOwned, marketPlayerBids);
    }

    handler->PSendSysMessage("AHBot total across %u distinct market(s): %u auctions (%u bot / %u player); %u protected player bid(s).",
        (uint32)visited.size(), total, botOwned, playerOwned, protectedPlayerBids);

    if (detailed)
    {
        handler->PSendSysMessage("Config: GUID %u, full interval %us, buy delay %u-%us, sell delay %u-%us, price multiplier %.2f.",
            (uint32)sAhBotConfig.guid, sAhBotConfig.updateInterval,
            sAhBotConfig.itemBuyMinInterval, sAhBotConfig.itemBuyMaxInterval,
            sAhBotConfig.itemSellMinInterval, sAhBotConfig.itemSellMaxInterval,
            sAhBotConfig.priceMultiplier);
        handler->PSendSysMessage("Limits: max item level %u, max required level %u, history %u day(s), bidder characters cached %u.",
            sAhBotConfig.maxItemLevel, sAhBotConfig.maxRequiredLevel, sAhBotConfig.historyDays,
            updating.load() ? 0u : (uint32)allBidders.size());
    }
}

void AhBot::AddToHistory(AuctionEntry* entry, uint32 won)
{
    if (!sAhBotConfig.enabled || !entry)
        return;

    if (!IsBotAuction(entry->owner) && !IsBotAuction(entry->bidder))
        return;

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry->itemTemplate);
    if (!proto)
        return;

    std::string category = "";
    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        if (CategoryList::instance[i]->Contains(proto))
        {
            category = CategoryList::instance[i]->GetName();
            break;
        }
    }

    if (!won)
    {
        won = AHBOT_WON_PLAYER;
        if (IsBotAuction(entry->bidder))
            won = AHBOT_WON_SELF;
    }

    sLog.outDetail( "AddToHistory: market price adjust");
    int count = entry->itemCount ? entry->itemCount : 1;
    updateMarketPrice(proto->ItemId, entry->buyout / count, entry->auctionHouseEntry->houseId);

    uint32 now = time(0);
    CharacterDatabase.PExecute("INSERT INTO ahbot_history (buytime, item, bid, buyout, category, won, auction_house) "
        "VALUES ('%u', '%u', '%u', '%u', '%s', '%u', '%u')",
        now, entry->itemTemplate, entry->bid ? entry->bid : entry->startbid, entry->buyout,
        category.c_str(), won, factions[entry->auctionHouseEntry->houseId]);
}

uint32 AhBot::GetAnswerCount(uint32 itemId, uint32 auctionHouse, uint32 withinTime)
{
    uint32 count = 0;

    auto results = CharacterDatabase.PQuery("SELECT COUNT(*) FROM ahbot_history WHERE "
        "item = '%u' AND won in (2, 3) AND auction_house = '%u' AND buytime > '%lu'",
        itemId, factions[auctionHouse], time(0) - withinTime);
    std::unique_ptr<QueryResult> results_guard(results);
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            count = fields[0].GetUInt32();
        } while (results->NextRow());
    }

    return count;
}

void AhBot::CleanupHistory()
{
    uint32 when = time(0) - 3600 * 24 * sAhBotConfig.historyDays;
    CharacterDatabase.PExecute("DELETE FROM ahbot_history WHERE buytime < '%u'", when);
}

uint32 AhBot::GetAvailableMoney(uint32 auctionHouse)
{
    int64 result = sAhBotConfig.alwaysAvailableMoney;

    std::map<uint32, uint32> data;
    data[AHBOT_WON_PLAYER] = 0;
    data[AHBOT_WON_SELF] = 0;

    const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionHouse);
    auto results = CharacterDatabase.PQuery(
        "SELECT won, SUM(bid) FROM ahbot_history WHERE auction_house = '%u' GROUP BY won HAVING won > 0 ORDER BY won",
        factions[auctionHouse]);
    std::unique_ptr<QueryResult> results_guard(results);
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            data[fields[0].GetUInt32()] = fields[1].GetUInt32();

        } while (results->NextRow());
    }

    results = CharacterDatabase.PQuery(
        "SELECT max(buytime) FROM ahbot_history WHERE auction_house = '%u' AND won = '2'",
        factions[auctionHouse]);
    results_guard.reset(results);
    if (results)
    {
        Field* fields = results->Fetch();
        uint32 lastBuyTime = fields[0].GetUInt32();
        uint32 now = time(0);
        if (lastBuyTime && now > lastBuyTime)
        result += (now - lastBuyTime) / 3600 / 24 * sAhBotConfig.alwaysAvailableMoney;
    }

    std::vector<AuctionSnapshot> auctionEntryMap = sAuctionMgr.GetAuctionsMap(ahEntry)->GetAuctionsSnapshot();
    for (std::vector<AuctionSnapshot>::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
    {
        if (!IsBotAuction(itr->bidder))
            continue;

        result -= itr->bid;
    }

    result += (data[AHBOT_WON_PLAYER] - data[AHBOT_WON_SELF]);
    return result < 0 ? 0 : (uint32)result;
}

void AhBot::CheckCategoryMultipliers()
{
    auto results = CharacterDatabase.PQuery("SELECT category, multiplier, max_auction_count, expire_time FROM ahbot_category");
    std::unique_ptr<QueryResult> results_guard(results);
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            categoryMultipliers[fields[0].GetString()] = fields[1].GetFloat();
            categoryMaxAuctionCount[fields[0].GetString()] = fields[2].GetInt32();
            categoryMultiplierExpireTimes[fields[0].GetString()] = fields[3].GetUInt64();

        } while (results->NextRow());
    }

    CharacterDatabase.PExecute("DELETE FROM ahbot_category");

    std::set<std::string> tmp;
    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        std::string name = CategoryList::instance[i]->GetDisplayName();

        if (tmp.find(name) != tmp.end())
            continue;

        tmp.insert(name);
        if (categoryMultiplierExpireTimes[name] <= (uint64)time(0) || categoryMultipliers[name] <= 0)
        {
            uint32 k = urand(1, 100);
            double m = 1.0;
            double r = (double)urand(100, 200) / 100.0;
            if (k < 50) m = r; // 1..2
            else if (k < 80) m = 1 + r; // 2..3
            else if (k < 90) m = 2 + r; // 3..4
            else m = 3 + r; // 4..5
            categoryMultipliers[name] = m;
            categoryMultiplierExpireTimes[name] = time(0) + urand(4, 7) * 3600 * 24;
        }

        categoryMaxAuctionCount[name] = CategoryList::instance[i]->GetMaxAllowedAuctionCount();

        CharacterDatabase.PExecute("INSERT INTO ahbot_category (category, multiplier, max_auction_count, expire_time) "
                "VALUES ('%s', '%f', '%u', '%zu')",
                name.c_str(), categoryMultipliers[name], categoryMaxAuctionCount[name], categoryMultiplierExpireTimes[name]);
    }
}


void AhBot::updateMarketPrice(uint32 itemId, double price, uint32 auctionHouse)
{
    double marketPrice = 0;

    auto results = CharacterDatabase.PQuery("SELECT price FROM ahbot_price WHERE item = '%u' AND auction_house = '%u'", itemId, auctionHouse);
    std::unique_ptr<QueryResult> results_guard(results);
    if (results)
    {
        marketPrice = results->Fetch()[0].GetFloat();
    }

    if (marketPrice > 0)
        marketPrice = (marketPrice + price) / 2;
    else
        marketPrice = price;

    CharacterDatabase.PExecute("DELETE FROM ahbot_price WHERE item = '%u' AND auction_house = '%u'", itemId, auctionHouse);
    CharacterDatabase.PExecute("INSERT INTO ahbot_price (item, price, auction_house) VALUES ('%u', '%lf', '%u')", itemId, marketPrice, auctionHouse);
}

bool AhBot::IsBotAuction(uint32 bidder)
{
    return allBidders.find(bidder) != allBidders.end();
}

uint32 AhBot::GetRandomBidder(uint32 auctionHouse)
{
    uint32 faction = factions[auctionHouse];
    std::vector<uint32> const& guids = bidders[faction];
    if (guids.empty())
    {
        sLog.outError("[AhBot] GetRandomBidder: no bidders registered for AH %u (faction %u)", auctionHouse, faction);
        return 0;
    }

    // LoadRandomBots already validates these rows and builds the per-house
    // cache. The former code copied and revalidated all ~5,400 bidders for
    // every single auction operation, turning a random pick into O(N).
    // Probe a small random sample in case a character was deleted after the
    // cache was built, then fall back to the configured AH owner.
    uint32 const first = urand(0, static_cast<uint32>(guids.size() - 1));
    uint32 const probes = std::min<uint32>(8, static_cast<uint32>(guids.size()));
    for (uint32 probe = 0; probe < probes; ++probe)
    {
        uint32 guid = guids[(first + probe) % guids.size()];
        std::string name;
        if (sObjectMgr.GetPlayerNameByGUID(ObjectGuid(HIGHGUID_PLAYER, guid), name))
            return guid;
    }

    sLog.outError("[AhBot] GetRandomBidder: cached bidder sample invalid for AH %u (faction %u)", auctionHouse, faction);
    return sAhBotConfig.guid ? static_cast<uint32>(sAhBotConfig.guid) : 0;
}

void AhBot::LoadRandomBots()
{
    sLog.outString("[AhBot] LoadRandomBots: scanning %zu random bot account(s)", sPlayerbotAIConfig.randomBotAccounts.size());

    for (std::list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        if (!sAccountMgr.GetCharactersCount(accountId))
            continue;

        auto result = CharacterDatabase.PQuery("SELECT guid, race FROM characters WHERE account = '%u'", accountId);
        std::unique_ptr<QueryResult> result_guard(result);
        if (!result)
            continue;

        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint8 race = fields[1].GetUInt8();
            uint32 auctionHouse = PlayerbotAI::IsOpposing(race, RACE_HUMAN) ? 2 : 1;
            bidders[auctionHouse].push_back(guid);
            bidders[3].push_back(guid);
            allBidders.insert(guid);
        } while (result->NextRow());
    }

    if (allBidders.empty() && sAhBotConfig.guid)
    {
        sLog.outString("[AhBot] No bot-account bidders found — falling back to AhBot.GUID=%llu", (unsigned long long)sAhBotConfig.guid);
        uint32 guid = sAhBotConfig.guid;
        allBidders.insert(guid);
        for (int i = 1; i <= 3; i++)
        {
            bidders[i].push_back(guid);
        }
    }

    sLog.outString("[AhBot] Bidders ready: Alliance=%zu Horde=%zu Neutral=%zu (total unique=%zu)",
        bidders[1].size(), bidders[2].size(), bidders[3].size(), allBidders.size());
}

int32 AhBot::GetSellPrice(ItemPrototype const* proto)
{
    if (!sAhBotConfig.enabled)
        return 0;

    int32 maxPrice = 0;
    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (!category->Contains(proto))
            continue;

        std::vector<uint32> items = availableItems.Get(category);
        if (std::find(items.begin(), items.end(), proto->ItemId) == items.end())
            continue;

        for (int auction = 0; auction < MAX_AUCTIONS; auction++)
        {
            int32 price = (int32)category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction]);
            if (!price)
                price = (int32)category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]);

            if (price > maxPrice)
                maxPrice = price;
        }
    }

    return maxPrice;
}

int32 AhBot::GetBuyPrice(ItemPrototype const* proto)
{
    if (!sAhBotConfig.enabled)
        return 0;

    int32 maxPrice = 0;
    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (!category->Contains(proto))
            continue;

        std::vector<uint32> items = availableItems.Get(category);
        if (std::find(items.begin(), items.end(), proto->ItemId) == items.end())
            continue;

        for (int auction = 0; auction < MAX_AUCTIONS; auction++)
        {
            int32 price = (int32)category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]);
            if (!price)
                continue;

            if (price > maxPrice)
                maxPrice = price;
        }
    }

    return maxPrice;
}

double AhBot::GetRarityPriceMultiplier(const ItemPrototype* proto)
{
    if (!sAhBotConfig.enabled)
        return 1.0;

    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (!category->Contains(proto))
            continue;

        return category->GetPricingStrategy()->GetRarityPriceMultiplier(proto->ItemId);
    }

    return 1.0;

}

bool AhBot::IsUsedBySkill(const ItemPrototype* proto, uint32 skillId)
{
    if (!sAhBotConfig.enabled)
        return false;

    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (category->GetSkillId() == skillId && category->Contains(proto))
            return true;
    }

    return false;
}

void AhBot::CheckSendMail(uint32 bidder, uint32 price, const AuctionSnapshot& entry)
{
    if (!sAhBotConfig.sendmail)
        return;

    time_t entryTime = GetTime("entry", entry.Id, entry.houseId, AHBOT_SENDMAIL);
    if (entryTime > time(0))
        return;

    const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(entry.houseId);
    if (!ahEntry)
        return;

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    std::vector<AuctionSnapshot> auctionEntryMap = auctionHouse->GetAuctionsSnapshot();
    for (std::vector<AuctionSnapshot>::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
    {
        const AuctionSnapshot& otherEntry = *itr;
        if (otherEntry.owner == entry.owner && otherEntry.Id != entry.Id && otherEntry.itemTemplate == entry.itemTemplate)
        {
            time_t otherEntryTime = GetTime("entry", otherEntry.Id, entry.houseId, AHBOT_SENDMAIL);
            if (otherEntryTime > time(0))
                return;
        }
    }

    // Sending resolves the receiver through ObjectAccessor and can reach their
    // live Player, so hand it to the world thread rather than doing it here.
    PendingProposition proposition;
    proposition.auctionId   = entry.Id;
    proposition.owner       = entry.owner;
    proposition.itemGuidLow = entry.itemGuidLow;
    proposition.bidder      = bidder;
    proposition.price       = price;
    proposition.houseId     = entry.houseId;
    proposition.expireTime  = entry.expireTime;

    std::lock_guard<std::mutex> g(queuedWorkMutex);
    queuedPropositions.push_back(proposition);
}

void AhBot::RunQueuedWork()
{
    // Purchases can perform SQL, mail and inventory work. Draining the entire
    // producer queue in one world tick stalls all maps and incoming players.
    // Preserve each queue's FIFO order; alternate queues to avoid starvation.
    WorkSlice slice(TurtleDiagnostics::Micros(), 4, 5000);
    while (slice.Take(TurtleDiagnostics::Micros()))
    {
        PendingPurchase purchase{};
        PendingProposition proposition{};
        bool doProposition;
        {
            std::lock_guard<std::mutex> g(queuedWorkMutex);
            if (queuedPurchases.empty() && queuedPropositions.empty())
                return;
            doProposition = !queuedPropositions.empty() && (preferProposition || queuedPurchases.empty());
            if (doProposition)
            {
                proposition = queuedPropositions.front();
                queuedPropositions.pop_front();
            }
            else
            {
                purchase = queuedPurchases.front();
                queuedPurchases.pop_front();
            }
            preferProposition = !doProposition;
        }
        if (doProposition)
            ExecuteProposition(proposition);
        else
            ExecutePurchase(purchase);
    }
}

void AhBot::ExecutePurchase(const PendingPurchase& p)
{
    DetailedWork::Scope work(DetailedWork::AuctionPurchase, p.bidder);
    const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[p.houseIndex]);
    if (!ahEntry)
        return;

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    if (!auctionHouse)
        return;

    // Hold the auctions lock for the whole read-act-remove sequence below, not
    // just the initial lookup. GetAuction()/RemoveAuction() only lock for their
    // own instant (the mutex is recursive specifically so callers can wrap a
    // larger critical section around them, per the comment on m_auctionsLock) -
    // entry is a raw pointer into the live map, and everything from here
    // through RemoveAuction()+delete needs to be atomic with respect to the
    // world thread's own packet handlers and AhBot::Update()'s background scan,
    // both of which can also touch this same entry. This is what the class
    // comment already documents as the intended pattern ("re-resolve the id
    // under the lock and re-check that the entry is still there") - this
    // function just never actually did it, leaving entry->bidder/bid writes,
    // SendAuctionSuccessfulMail, RemoveAuction and delete entry all racing
    // against concurrent access to the same AuctionsMap/AuctionEntry.
    AuctionHouseObject::Guard g(auctionHouse->GetLock());

    // The seller may have cancelled, or the auction may have expired or sold,
    // between the bot deciding and us getting here.
    AuctionEntry* entry = auctionHouse->GetAuction(p.auctionId);
    if (!entry)
        return;

    Item* item = sAuctionMgr.GetAItem(entry->itemGuidLow);
    if (!item || !item->GetCount())
        return;

    ItemPrototype const* proto = item->GetProto();
    if (!proto)
        return;

    entry->bidder = p.bidder;
    entry->bid = p.bidAmount;

    if ((entry->buyout && (entry->bid >= entry->buyout || 100 * (entry->buyout - entry->bid) / std::max(p.unitPrice, 1u) < 25)) &&
            !(p.minBuyout && entry->buyout && p.minBuyout < entry->buyout))
    {
        entry->bid = entry->buyout;
        sLog.outString("[AhBot] Bought: %dx %s on AH %u for %u (bidder guid=%u)",
                item->GetCount(), proto->Name1.c_str(), auctionIds[p.houseIndex], entry->buyout, p.bidder);
    }
    else
    {
        sLog.outString("[AhBot] Bought (at bid): %dx %s on AH %u for %u (bidder guid=%u)",
                item->GetCount(), proto->Name1.c_str(), auctionIds[p.houseIndex], entry->bid, p.bidder);
    }

    updateMarketPrice(proto->ItemId, entry->buyout / item->GetCount(), auctionIds[p.houseIndex]);

    // Pay the seller immediately and finalize the auction.
    // If the item is an upgrade for the bidder bot, equip it directly in the DB.
    // Otherwise discard it - letting items go through the normal mail/login path
    // causes inventory corruption when bot inventories are full.
    uint32 itemGuidLow = entry->itemGuidLow;
    sAuctionMgr.SendAuctionSuccessfulMail(entry);
    if (!TryEquipItem(entry->bidder, itemGuidLow, proto))
        CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid='%u'", itemGuidLow);
    sAuctionMgr.RemoveAItem(itemGuidLow);
    delete item;
    AddToHistory(entry, AHBOT_WON_BID);
    entry->DeleteFromDB();
    auctionHouse->RemoveAuction(entry);
    delete entry;

    CharacterDatabase.PExecute("DELETE FROM ahbot_history WHERE item = '%u' AND won = 4 AND auction_house = '%u' ",
            proto->ItemId, factions[auctionIds[p.houseIndex]]);
}

void AhBot::ExecuteProposition(const PendingProposition& p)
{
    DetailedWork::Scope work(DetailedWork::AuctionProposition, p.bidder);
    Item* item = sAuctionMgr.GetAItem(p.itemGuidLow);
    if (!item || !item->GetProto())
        return;

    std::string name;
    if (!sObjectMgr.GetPlayerNameByGUID(ObjectGuid(HIGHGUID_PLAYER, p.bidder), name))
        return;

    std::ostringstream body;
    body << "Hello,\n";
    body << "\n";
    body << "I see you posted " << ChatHelper::formatItem(item, item->GetCount());
    body << " to the AH and I really need that at the moment. Could you lower your price at least to ";
    body << ChatHelper::formatMoney(PricingStrategy::RoundPrice(p.price)) << "? I'll buy it then.\n";
    body << "\n";
    body << "Regards,\n";
    body << name << "\n";

    std::ostringstream title; title << "AH Proposition: " << item->GetProto()->Name1.c_str();
    MailDraft draft(title.str(), body.str());
    ObjectGuid receiverGuid(HIGHGUID_PLAYER, p.owner);
    draft.SendMailTo(MailReceiver(receiverGuid), MailSender(MAIL_NORMAL, p.bidder));

    SetTime("entry", p.auctionId, p.houseId, AHBOT_SENDMAIL, p.expireTime);
}

void AhBot::Dump()
{
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
            continue;

        bool first = true;
        for (int i=0; i<CategoryList::instance.size(); i++)
        {
            Category* category = CategoryList::instance[i];
            if (category->Contains(proto))
            {
                std::vector<uint32> items = availableItems.Get(category);
                if (find(items.begin(), items.end(), proto->ItemId) == items.end())
                    continue;

                std::ostringstream out;
                if (first)
                {
                    out << proto->ItemId << " (" << proto->Name1.c_str() << ") x" << category->GetStackCount(proto) << " - ";
                    first = false;
                }

                int auction = 0;
                const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
                out << "SELL: "
                    << ChatHelper::formatMoney(category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction], true))
                    << ", BUY: "
                    << ChatHelper::formatMoney(category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]))
                    << " (" << category->GetDisplayName() << ")";
                sLog.outString("%s",out.str().c_str());
            }
        }
    }
}

void AhBot::CleanupPropositions()
{
    DetailedWork::Scope work(DetailedWork::AuctionCleanup);
    uint32 deliverTime = time(0) - 3600 * 24 * 2;
    auto result = CharacterDatabase.PQuery("select id, receiver from mail where subject like 'AH Proposition%%' and deliver_time <= '%u'", deliverTime);
    std::unique_ptr<QueryResult> result_guard(result);
    if (!result)
        return;

    int count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 id = fields[0].GetUInt32();
        uint32 receiver = fields[1].GetUInt32();
        Player *player = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, receiver));
        if (player) player->RemoveMail(id);
        count++;
    } while (result->NextRow());

    if (count > 0)
    {
        CharacterDatabase.PExecute("delete from mail where subject like 'AH Proposition%%' and deliver_time <= '%u'", deliverTime);
        sLog.outBasic("%d old AH propositions removed", count);
    }
}

void AhBot::DeleteMail(std::list<uint32> buffer)
{
    std::ostringstream sql;
    sql << "delete from mail where id in ( ";
    bool first = true;
    for (std::list<uint32>::iterator j = buffer.begin(); j != buffer.end(); ++j)
    {
        if (first) first = false; else sql << ",";
        sql << "'" << *j << "'";
    }
    sql << ")";
    CharacterDatabase.Execute(sql.str().c_str());
}

INSTANTIATE_SINGLETON_1( ahbot::AhBot );
