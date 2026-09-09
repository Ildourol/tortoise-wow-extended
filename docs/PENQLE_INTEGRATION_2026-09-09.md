# Penqle mainline integration, 2026-09-09

Starting ManTech baseline: ae8c67e7. Penqle main tip:
`b8f24bef6cfc69feafc5870ac6a8918a521253d7` (15 missing ancestry commits).

## Source decisions

- Accept 37b058c3 and 4f9a2c6b: `LoadChatChannels` retains each database ID
  instead of assigning every entry ID 1. Name lookup matches fixed names and
  shortcuts exactly; localized `%s` patterns require their prefix and suffix.
  `Channel` uses the returned ID/flags for built-in identity, and `ChannelMgr`
  uses the flags to enforce zone-dependent admission. Custom channel names
  containing a built-in name are no longer misclassified. Empty localized names
  remain ignored, and the existing empty-zone prefix match is preserved.
- 114d5163 special-container families and 4355834c VMap tree locking are already
  present. Keep the current native implementations and existing regression tests.
- 7e63fae3 spline clamping is already adapted in ae8c67e7. Resolve the merge
  conflict with our atomic log timestamp; reverting to the upstream static
  non-atomic timestamp would introduce a race between map updates. Native
  segment, arrival and cycle processing remain unchanged.
- No Penqle archive branches or Git bundle are created. Mainline ancestry is
  merged; divergent feature branches are not production updates. In particular,
  `bot-helpers` belongs to the alternate headless-session architecture discussed
  in the Shyalya integration notes, not our current holder/map-owner lifecycle.

## Database migrations included in source

`20260907165343_world.sql` removes stale spell-chain/proc rows and repairs two
specific chain prerequisites. Native `SpellMgr` derives rank chains from DBC,
rejects conflicting DB chains, and rejects non-first-rank proc overrides with
zero PPM. Live rows must be checked for custom accepted overrides before applying
the upstream deletion list; a local fixture is not evidence of live contents.

`20260908193409_world.sql` supplies Grim Reaches quest data: Dragonfire Bombs,
Expelling Evil, I Am Become Death, Rebuilding the Relic, the Chromatic Servo
Motor, the Ritual of Uth'okk, the Skardyn, To Cure the Withered, and Tomb of
Ancestors. It uses the existing gossip, condition, creature spell-list, quest
credit, area-trigger and DB-script mechanisms. Relevant native contracts:

- Broadcast chat type 2 is `CHAT_TYPE_TEXT_EMOTE`, not a client chat opcode.
- Script field 147 is `FIELD_UNIT_NPC_FLAGS`; the loader maps it to the current
  build's update-field index. Command 4 options 2/1 remove/add the gossip and
  questgiver bits around the ritual. Command 8 awards normal quest kill credit.
- Quest-start scripts run with the NPC as source and player as target. Data
  flag 4 makes the ritual's Enrage cast target the source. Skardyn credit is
  guarded by the quest-taken condition even on the final gossip script.
- The single-text zero probability values follow the existing database
  convention; the native gossip serializer passes these weights to the client.
  Client display and the complete quest chains still require gameplay testing.
- Upstream describes the Flame of Dagnoth combat spell selection as an estimate,
  not a confirmed reconstruction of official behavior.

These are once-only migrations, not idempotent deployment scripts. Before live
application, check the migration ledger and every affected key, especially GO
GUID 5025863, menu/text/condition IDs, and area triggers 700/5601 (one quest per
trigger in this schema). Confirm the existing fire gossip binding, quest/NPC/
item/spell references and current DB overrides. Do not mask a conflicting row
with blanket INSERT IGNORE or REPLACE.

## Validation and deployment boundary

`ChatChannelLookupTest` compiles the actual native loader and lookup functions,
with database fixtures for IDs, metadata, locales, shortcuts, custom names,
prefix/suffix rejection, numeric misses and reloads. Existing spline, VMap,
container and bot lifecycle tests remain in the architecture suite.

`PenqleMigrationTest.py` executes both migrations in local SQLite fixtures using
column names and primary keys extracted from the repository's base schemas. It
checks prerequisite guards, unrelated-row preservation, gossip/text linkage,
conditional credit, ritual flags, loot and quest/area-trigger relations. It
does not validate MySQL types, production collisions, DBC availability, or live
quest behavior. Run it with the architecture suite's Python interpreter.

The production-data rehearsal did not run: the game DB account rejected this
workstation, and automatic approval review rejected the administrator-credential
invocation. No production tables were changed. Live DB preflight and application
remain outstanding. This integration does not deploy a binary, edit production
configuration, or start/stop the server.
