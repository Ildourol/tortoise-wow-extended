set(core "${SOURCE_ROOT}/src/game/AuctionHouse/AuctionHouseMgr.h")
set(handler "${SOURCE_ROOT}/src/game/Handlers/AuctionHouseHandler.cpp")
set(manager "${SOURCE_ROOT}/src/game/AuctionHouse/AuctionHouseMgr.cpp")

file(READ "${core}" core_text)
file(READ "${handler}" handler_text)
file(READ "${manager}" manager_text)

foreach(required
        "typedef std::recursive_mutex ItemsMutex;"
        "ItemsMutex& GetItemsLock() const"
        "ItemGuard g(m_itemsLock);")
    string(FIND "${core_text}" "${required}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Missing auction item lifetime contract: ${required}")
    endif()
endforeach()

string(FIND "${handler_text}" "AuctionHouseObject::Guard auctionGuard(auctionHouse->GetLock());" auction_guard)
string(FIND "${handler_text}" "AuctionHouseMgr::ItemGuard itemGuard(sAuctionMgr.GetItemsLock());" item_guard)
if(auction_guard LESS 0 OR item_guard LESS 0 OR NOT auction_guard LESS item_guard)
    message(FATAL_ERROR "Auction client query must lock auction entries before auction items")
endif()

string(FIND "${manager_text}" "if (!proto)" invalid_proto_guard)
string(FIND "${manager_text}" "skipped in client search" invalid_proto_log)
if(invalid_proto_guard LESS 0 OR invalid_proto_log LESS 0)
    message(FATAL_ERROR "Auction client search must reject invalid item templates")
endif()

message(STATUS "Auction query item-lifetime contract verified")
