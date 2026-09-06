-- Persistent per-item AuctionHouseBot overrides, compatible with the CMaNGOS
-- `.ahbot item` administrator command. A value of 0 bans the item; add_chance
-- is a percentage; min_amount/max_amount control generated stack sizes.
CREATE TABLE IF NOT EXISTS `ahbot_items` (
  `item` int unsigned NOT NULL DEFAULT '0',
  `value` int unsigned NOT NULL DEFAULT '0',
  `add_chance` int unsigned NOT NULL DEFAULT '0',
  `min_amount` int unsigned NOT NULL DEFAULT '0',
  `max_amount` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=COMPACT COMMENT='AuctionHouseBot per-item overrides';
