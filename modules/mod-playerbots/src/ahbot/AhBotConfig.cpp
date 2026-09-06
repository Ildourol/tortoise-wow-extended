
#include "AhBotConfig.h"
#include "SystemConfig.h"
#include "Log.h"
std::vector<std::string> split(const std::string &s, char delim);

INSTANTIATE_SINGLETON_1(AhBotConfig);

AhBotConfig::AhBotConfig() :
    config(new Config()),
    enabled(false), guid(0), updateInterval(900), historyDays(30), maxSellInterval(3600 * 8),
    itemBuyMinInterval(600), itemBuyMaxInterval(7200),
    itemSellMinInterval(600), itemSellMaxInterval(7200),
    alwaysAvailableMoney(200000), priceMultiplier(1.0f), priceQualityMultiplier(1.0f),
    defaultMinPrice(20), stackReducePrice(1000000), maxItemLevel(199), maxRequiredLevel(80),
    underPriceProbability(0.05f), sendmail(true)
{
}

template <class T>
void LoadSet(std::string value, T &res)
{
    std::vector<std::string> ids = split(value, ',');
    for (std::vector<std::string>::iterator i = ids.begin(); i != ids.end(); i++)
    {
        uint32 id = atoi((*i).c_str());
        if (!id)
            continue;

        res.insert(id);
    }
}

bool AhBotConfig::Initialize()
{
    if (!Load())
    {
        sLog.outString("AhBot is Disabled. Unable to open configuration file ahbot.conf");
        return false;
    }

    if (!enabled)
        sLog.outString("AhBot is Disabled in ahbot.conf");
    return enabled;
}

bool AhBotConfig::Reload()
{
    if (!Load())
        return false;

    if (!enabled)
        sLog.outString("AhBot is Disabled in ahbot.conf");

    return true;
}

bool AhBotConfig::Load()
{
    // Parse into a fresh Config first. A malformed or missing replacement must
    // not destroy the working in-memory configuration.
    std::unique_ptr<Config> candidate(new Config());
    if (!candidate->SetSource(SYSCONFDIR"ahbot.conf", "AHBot_"))
        return false;

    bool const newEnabled = candidate->GetBoolDefault("AhBot.Enabled", true);
    uint64 const newGuid = (uint64)candidate->GetIntDefault("AhBot.GUID", 0);
    uint32 const newUpdateInterval = std::max<int32>(3, candidate->GetIntDefault("AhBot.UpdateIntervalInSeconds", 900));
    uint32 const newHistoryDays = std::max<int32>(1, candidate->GetIntDefault("AhBot.History.Days", 30));
    uint32 const newItemBuyMinInterval = std::max<int32>(1, candidate->GetIntDefault("AhBot.ItemBuyMinInterval", 600));
    uint32 const newItemBuyMaxInterval = std::max<int32>(newItemBuyMinInterval, candidate->GetIntDefault("AhBot.ItemBuyMaxInterval", 7200));
    uint32 const newItemSellMinInterval = std::max<int32>(1, candidate->GetIntDefault("AhBot.ItemSellMinInterval", 600));
    uint32 const newItemSellMaxInterval = std::max<int32>(newItemSellMinInterval, candidate->GetIntDefault("AhBot.ItemSellMaxInterval", 7200));
    uint32 const newMaxSellInterval = std::max<int32>(1, candidate->GetIntDefault("AhBot.MaxSellInterval", 3600 * 8));
    uint32 const newAlwaysAvailableMoney = std::max<int32>(0, candidate->GetIntDefault("AhBot.AlwaysAvailableMoney", 200000));
    float const newPriceMultiplier = std::max(0.0f, candidate->GetFloatDefault("AhBot.PriceMultiplier", 1.0f));
    uint32 const newDefaultMinPrice = std::max<int32>(0, candidate->GetIntDefault("AhBot.DefaultMinPrice", 20));
    uint32 const newMaxItemLevel = std::max<int32>(0, candidate->GetIntDefault("AhBot.MaxItemLevel", 199));
    uint32 const newMaxRequiredLevel = std::max<int32>(0, candidate->GetIntDefault("AhBot.MaxRequiredLevel", 80));
    uint32 const newStackReducePrice = std::max<int32>(1, candidate->GetIntDefault("AhBot.StackReducePrice", 1000000));
    float const newPriceQualityMultiplier = std::max(0.0f, candidate->GetFloatDefault("AhBot.PriceQualityMultiplier", 1.0f));
    float const newUnderPriceProbability = std::max(0.0f, std::min(1.0f, candidate->GetFloatDefault("AhBot.UnderPriceProbability", 0.05f)));
    bool const newSendmail = candidate->GetBoolDefault("AhBot.SendMail", true);

    std::set<uint32> newIgnoreItemIds;
    std::set<uint32> newIgnoreVendorItemIds;
    LoadSet<std::set<uint32> >(candidate->GetStringDefault("AhBot.IgnoreItemIds", "49283,52200,8494,6345,6891,2460,37164,34835,17,2248"), newIgnoreItemIds);
    LoadSet<std::set<uint32> >(candidate->GetStringDefault("AhBot.IgnoreVendorItemIds", "755,858,4592,4593,1710,3827,2455,3385"), newIgnoreVendorItemIds);

    config.swap(candidate);
    enabled = newEnabled;
    guid = newGuid;
    updateInterval = newUpdateInterval;
    historyDays = newHistoryDays;
    itemBuyMinInterval = newItemBuyMinInterval;
    itemBuyMaxInterval = newItemBuyMaxInterval;
    itemSellMinInterval = newItemSellMinInterval;
    itemSellMaxInterval = newItemSellMaxInterval;
    maxSellInterval = newMaxSellInterval;
    alwaysAvailableMoney = newAlwaysAvailableMoney;
    priceMultiplier = newPriceMultiplier;
    defaultMinPrice = newDefaultMinPrice;
    maxItemLevel = newMaxItemLevel;
    maxRequiredLevel = newMaxRequiredLevel;
    stackReducePrice = newStackReducePrice;
    priceQualityMultiplier = newPriceQualityMultiplier;
    underPriceProbability = newUnderPriceProbability;
    sendmail = newSendmail;
    ignoreItemIds.swap(newIgnoreItemIds);
    ignoreVendorItemIds.swap(newIgnoreVendorItemIds);

    // Category values are lazily cached, so every successful reload must make
    // subsequent reads consult the new file.
    sellPriceMultipliers.clear();
    buyPriceMultipliers.clear();
    itemPriceMultipliers.clear();
    maxAuctionCount.clear();
    maxItemAuctionCount.clear();
    return true;
}
