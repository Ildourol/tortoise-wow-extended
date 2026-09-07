-- Native database-bound Balor explosives gossip. Quest eligibility and credit
-- remain in go_balor_explosives; native text packets use this text store.
INSERT INTO `broadcast_text` (`entry`, `male_text`, `female_text`) VALUES
(4169801, '<This seems to be a fitting place for Rufus'' explosives.>', '<This seems to be a fitting place for Rufus'' explosives.>');
INSERT INTO `npc_text` (`ID`, `BroadcastTextID0`, `Probability0`) VALUES (4169801,4169801,1);
UPDATE `gameobject_template` SET `script_name`='go_balor_explosives'
WHERE `entry` IN (2020178,2020179,2020180) AND `script_name`='';
