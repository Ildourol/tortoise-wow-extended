# Shyalya integration, 2026-09-09

Starting ManTech baseline: e424d897. Upstream main integration tip: 0c1f90a0.

- 3a8472e6: accept native GetName fallback when the account cache has no username.
  AccountNameCacheTest executes native GetName and ChangePassword with mock DB/hash
  services, proving a partial cache cannot select an empty hash username.
- f35efe3d: upstream merge ancestry recorded; Penqle content already integrated.
  Preserve our audited native module GO gossip callback before the Eluna fallback.
- 6a2ddc82 and 83a61bc3: owner notification guard and auction/item snapshot locking
  already present; no regression or second lock layer introduced.
- 0c1f90a0: adopt ENABLE_SOAP build switch. Default ON on both platforms because
  our matching gSOAP runtime builds from source. SOAP.Enabled remains runtime OFF
  by default. Compile guards cover include, startup AND shutdown join; disabled
  builds omit gSOAP and SOAP sources. No Linux archive is linked on Windows.
- 9c1e8266: scope retirement notice to Shyalya's upstream, not this active fork.

All 16 current upstream branches and reachable Git history are preserved under
archive/shyalya/2026-09-09/* and in a verified standalone local Git bundle.
These snapshots preserve source history, not issues, PR conversations, release
assets or wiki contents. No tags were advertised by upstream at this snapshot.

Other branch review:
- 1181-rogue-fixes, 1181dev, challenges, dev, feat-spell-dbc-loader, main, shop,
  and fix/bot-death-loop have no commits missing from the starting baseline.
- fix/vmap-tree-lock: implementation already present in our baseline (file diff
  is empty), so retain it without duplicating the change.
- fix/movespline-clamp: reviewed separately for adaptation to parallel logging.
- bot-helpers-port, bot-hooks, fix/worldsession-merge-brace and
  fix/login-yield-state target a different headless-session architecture, including
  legacy bot removal. Our CharacterHandler has no HeadlessSessionState or
  WORLDHOOK_ON_BOT_LOGIN_YIELD path. Preserve snapshots without replacing our
  holder/map-owner lifecycle to apply a non-applicable one-line state fix.
- merge/transport-stack includes an alternate Docker/WSG/test-tool stack and
  extensive divergent work; preserve it as a reference, not a blanket baseline
  merge. Mainline ancestry and code adaptations are reviewed independently.

No database migration or production deployment is part of this source integration.

Movement follow-up: adapted 7e63fae3 as a separate commit. Native advance skips
already-expired segment time and continues the native segment/cycle/arrival
logic. Replace the upstream shared non-atomic log timestamp with an atomic CAS;
otherwise parallel map updates would race. No movement teleport/fallback added.
