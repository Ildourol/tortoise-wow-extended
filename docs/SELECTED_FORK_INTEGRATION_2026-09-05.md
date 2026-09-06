# Selected fork integration — 2026-09-05

## September 6 follow-up: compatibility-selected upstream update

Reviewed against deployed AHBot baseline `0c322d52`, Shyalya
`2814ea80` (`playerbots-integration-gh`) and Penqle `55fdbc0c` (`main`).
The comparison contained 38 Shyalya and 9 Penqle ancestry-only commits;
those numbers are not counts of missing fixes. This follow-up supersedes the
older status below where explicitly noted. No whole-branch merge was used.

### Integrated in this candidate

| Upstream commits | Result / compatibility decision |
|---|---|
| Shyalya `c7d9b00d` | Correct emote-field names in existing DB warnings; no validation changes. |
| Shyalya `b405b10a` | Failed migration returns false instead of waiting for console input; preserves failure propagation, migration hashing, ordering and DB worker ownership. |
| Shyalya `68397588` | Duke Dreadmoore sound registry, boss chat types and invalid emote cleanup. Existing sound rows and unexpected chat/emote values remain untouched. |
| Shyalya `6cdc923b` | Default AutoLearnTrainerSpells to enabled, matching the empty class-level shim and existing native CatchUpTrainerSpells call. Explicit deployment config still wins; no live config was overwritten. |
| Shyalya `d60461b0`, `2814ea80` | AB uses the nine real banner GO entries and allows spawned assault BUTTON banners to reach native capture validation. Retains same-map/range, CanInteract, spell and match/node/team checks. Four-second attempt spacing uses the already-registered per-bot qualified last-spell time value, shared by that bot's action instances, not upstream's global unordered_map. No new entry-diagnostic scans/logs. Other BG state gates remain unchanged. |
| Shyalya `6baeed54` | Cleanup is restricted to upstream's enumerated GUIDs AND absence of the corresponding creature row. Restored/custom live spawns keep their movement/linking data. |
| Penqle `42fdcceb` | Four Northwind NPC dialogue/menu chains. Adapted inserts to preserve existing keys; only zero gossip bindings are changed. Read-only preflight found the four bindings zero and the new dialogue/menu/text keys absent. |

### Already incorporated: do not duplicate

- Shyalya `ef4ca228`: nearby AB discovery without the ground-level static LOS
  filter is already adapted locally and covered by ForkIntegrationTest.
- Shyalya `32826645`, `5f9c0b7a`: our trainer purchase/pet-training work is
  already present. Upstream PR ancestry is not another fix to apply.
- Penqle `41a3bd79`, `6992814c`: live read-only checks now confirm quest
  40348 -> 51669, quest 40353 -> 47263, Clearcasting mask 6599486734339,
  and Seismic Strength mask 1125899906842624. Existing guarded source SQL
  remains in sql/custom; no second unguarded migration was added.

### Held out under the no-architecture-conflict requirement

- `6dbcab11`, `580170a6`, `004c194f`: coupled fresh-install/migration repair.
  Moving 27 previously unscanned migrations would activate old SQL on this
  deployment. The claim that those migrations were never applied elsewhere
  cannot be assumed for our manually updated DB. Keep the package out until
  each file is reconciled against our migration history and custom records.
- Penqle `6d8be99b`, `2d20c5c9`: broad item-set and unique-item packages,
  **not rejected as useless**. They add a shared combo-spend hook, change
  absorb snapshots, periodic damage and self-resurrection, and replace class
  spell scripts/bindings. Our native custom-aura 227–230 implementation and
  existing spell lifecycle changes require dedicated effect-level regression
  work before this bundle satisfies the user's compatibility constraint.
- Penqle `daaf9a74`: cosmetic model/gender substitutions including new display
  IDs; matching deployed client models have not been verified. Not a required
  server fix. Penqle `3463d779` disables an existing world event: a content
  policy change, not an architecture-preserving bug fix. `59f47f61` is README
  module-topic documentation, not a gameplay fix.
- Optional dungeon-clear changes `16062746`, `88476eb3`, `8f575818`,
  `7959c5b6`, `a954addd`, `69b01638`, `d5073863`, `47112601`, `618b7502`,
  `84e1b5d6`, `7e9d142c`, `c45a1883`, `4aa37b20`, `667e1ab8`, `fdf592f4`,
  `f0f1fd9f`, `f426cda3`, `6be01e53`: rosters, routes, following and test
  diagnostics for a module confirmed disabled by the actual production build.
  No dormant module was enabled and no second movement/test controller added.
- Testlab-only `b707bd9e`, `c4abff77`: changes to an unused server-management
  pipeline, not the running server. Merge-only commits `cc2b284c`, `dbb12a94`,
  `804e3337`, `c7d46275`, `6743c07e`, `55fdbc0c` carry the component changes
  classified above, not additional independent fixes.

### Validation and release boundary

- Native Windows Release build: mangosd and realmd linked successfully using
  four workers; no server process launched for a smoke test.
- Release architecture suite: 37/37 passed. New UpstreamAbCaptureTest executes
  production entry-list, state-gate and throttle fragments with deterministic
  services: all spawned/state combinations, multiple bots, multiple actions
  sharing one bot context, four-second boundary and wall-clock rollback.
- AddressSanitizer architecture build: the same 37/37 tests passed.
- UpstreamSqlSafetyTest.py passed on SQLite fixtures with only the INSERT
  IGNORE spelling adapted. Tests repeatability, preserved local dialogue/menu
  bindings and live-spawn paths, and bounded orphan deletion scope. These are
  guard tests, not a substitute for native MySQL execution.
- MySQL EXPLAIN accepted all ten statements in the three staged migrations
  against the actual world schema. EXPLAIN did not apply them. No DB changes,
  live share overwrite, server restart or deployed config changes this turn.
- No changes to world/map scheduling, login priority, DB priority queues,
  transport/taxi/movement implementation, AHBot or custom aura handlers. Runtime
  compatibility still needs live AB capture/recapture and dialogue checks;
  compilation/test doubles are not proof of every gameplay outcome.

Deployment SQL is exactly these three reviewed files, not the whole upstream
updates directory: `20260903115534_world.sql`, `20260905175321_world.sql`,
`20260906134216_world.sql`. Complete read-only preflight again at deployment;
apply through the existing migration workflow only after the operator stops
the server. This record does not authorize starting or stopping the server.

Scope: selective native fixes, not ancestry-only merging of whole forks. Existing
ManTech scheduling, player login priority, transport simulation and trainer
semantics remain unchanged. No new runtime diagnostics or config keys.

## Integrated into source

| Origin | Change | Adaptation / limits |
|---|---|---|
| Shyalya `ef4ca228ea89a6a70de2c8b8de65d1df1c35a0ae` | Nearby AB banners are not rejected by the ground-level static LOS discovery filter. | Preserves native same-map range checks, banner eligibility and capture spells. No generic LOS or PvP rule bypass. In-game capture remains untested. |
| JonahSimon `e13a01110f634cbe4f868816e1c702ab3a49ed76` | Both path-calculation overloads reject null owners with NOPATH and clear stale points. | Extracted only the core guard, not the unrelated F2 module/comment changes. Ownerless path calculation is still unsupported; this is safe failure, not a new cross-map solver. |
| Ildourol `750a9856e9b6ac964f170ad64e0d936268ce7c96` | Bot channel dispatch respects EnableBroadcasts. | Existing routing and probabilities retained. |
| Melhart9 `224353490029b69719f4966b3580d339ab323a4e` | Reduces repeated pet-existence database polling. | Only the DB fallback is cached for 30 seconds per native value instance; live pet checks keep the original calculated-value cadence. A live pet or explicit value Reset invalidates fallback state. No shared table, scheduler change or busy-bot releveling policy. DB-only changes can remain cached up to 30 seconds. |

## Prepared SQL, not applied

`sql/custom/20260905_selected_fork_gameplay.sql`, from Penqle commits
`41a3bd79e7696a2a457dfb3ba986023c9086c26c` and
`6992814c7b373295e8fc6030391b7821927c3b6b`, reviewed through trikkizerg:

- Tauren/Troll quest rewards cast their existing native learn-spell wrappers
  (51669 -> 45500; 47263 -> 45504), rather than the combat spells directly.
- Adds missing Clearcasting spell-affect and Seismic Strength proc masks.
- Updates only expected old rewards and inserts only missing keys. Does not
  overwrite unexpected custom records. Deployment must stop on preflight
  conflict rows; those SELECTs are validation, not an automatic SQL abort.

Read-only production validation confirmed the wrong old quest reward IDs,
correct wrapper effects/targets, and absence of the proposed mask records.
The SQL was not executed against production or a disposable database in this
integration. Schema/effect inspection does not substitute for quest/proc playtests.

## Excluded from this integration

- Already-present auction, trainer, VMAP, engine-reset and battleground fixes:
  no duplicate imports.
- DungeonClear routes, rosters, longer trails and test-login blacklist: optional
  module absent from the current build. Some changes add unbounded tracking or
  server-specific GUIDs; no benefit to this deployment as-is.
- Tournament/LLM/Eluna modules, dynamic XP, loot-sharing, autonomous marching,
  cosmetic gossip and inventory automation: not selected as required fixes.
- Shared CC maps, old threading and wholesale transport/session replacements:
  incompatible unchanged with current ownership or redundant architecture.
- LFG stub bundle: **held, not approved**. Activates currently dormant bot queue
  paths; native queue ownership/callers and repeated template scans need a
  complete compatible integration, not just replacing empty return values.
- Item-set bundle: **held, not discarded as useless**. Shared spell-completion,
  combo, periodic damage and resurrection changes need coherent binding/data
  integration and class regressions. Not represented as safely merged.
- BG vendor restoration: **held** pending intended item/reputation/cost validation;
  sparse current inventories alone do not prove every proposed item is correct.
- Orphan cleanup, auction-safety removal and forced interaction shortcuts:
  no justified blind application to our current data/architecture.

## Validation and deployment boundary

- Release server build completed with four compiler workers; no server smoke run.
- Release and AddressSanitizer architecture suites: 22/22 passed each.
- New ForkIntegrationTest executes extracted native boundaries for null-owner
  failure, retained valid-owner entry, broadcast gating, DB fallback expiry and
  timer wrap, live pet death/revival, and nearby same-map banner discovery.
- These tests use deterministic stand-ins, not a full live spell/AI/network engine.
- No production SQL, share overwrite, restart, extra runtime logging or bot-count
  change performed. A new binary is a candidate for normal operator-controlled
  deployment, not proof of 6,000-bot performance improvement.
- Pre-existing local audit changes remain preserved separately; this integration
  does not claim to commit or certify them. The packaged build includes the current
  working source, as did the prior deployed audit build.
