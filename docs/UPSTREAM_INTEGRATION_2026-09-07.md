# September 7 upstream integration

Integrated Penqle `main` at `08df6900` (including `1181dev` at `48825a70`)
and Shyalya `playerbots-integration-gh` at `3f9a0622`, preserving upstream
history and separating local adaptations by subsystem.

The native Brewmaster missing-reward guard, Wild Regeneration script,
bounded Arathi Basin per-bot state, auction implementation and auction
regression coverage were retained. The duplicate module AH schema migration
was removed; the existing root character migration remains authoritative.

Local adaptations:

- Build matching gSOAP 2.8.135 from source instead of linking a Linux archive.
  Retain account command permissions, shared queued callback ownership,
  shutdown cancellation and explicit joining before database shutdown.
  SOAP remains configurable and disabled by default beside console settings.
- Dispatch all bot holders on the joined world owner with generation checks.
  AI-less socketless bots complete native near/far teleport ACKs, with
  ownership revalidation before subsequent player access.
- Bind Balor explosives through native database script IDs and NPC text
  storage. Preserve quest/objective/spawn checks and repeat-credit prevention.
- Resolve DungeonClear script actions against offered gossip options and
  encode the native 1.12 GUID/option-index packet. Preserve the existing
  empty-menu fallback without broadening it to nonempty menus.

Validation: Windows Release build; 43 architecture tests in Release and
AddressSanitizer configurations; MSVC syntax checks for all 13 changed
DungeonClear translation units with its compatibility prelude. The new
tests execute production fragments for SOAP cancellation/late callbacks,
holder replacement/removal, specialty bag families, Balor missing/completed
quests and repeat requests, and dungeon gossip action selection/packet shape.
Source indexes were regenerated without a database audit refresh.

Four world migrations were rehearsed on isolated copies of the 18 affected
tables, with verified backup checksums, matching native objective references,
script bindings and migration hashes. The running server's loader log confirms
the spell cleanup targets rejected/redundant chain rows. Deployment must
recheck the exact source hashes and unchanged production table fingerprints
before applying that rehearsal.

Production build options retain playerbots enabled and DungeonClear disabled.
Bot activity, auction settings and population were not tuned. These checks
do not constitute live gameplay, dungeon progression or SOAP client acceptance;
the user-controlled restart is the next runtime acceptance step.
