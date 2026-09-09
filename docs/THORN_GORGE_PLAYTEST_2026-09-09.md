# Thorn Gorge playtest follow-up, September 9, 2026

Implemented changes below still require a coordinated client/server restart and live acceptance. Compilation and offline asset checks are not a complete battleground playtest.

## Completed match evidence

Build 97fb6d6c, instance 101: Horde 1600 / Alliance 1271, 1,287,604ms active simulation, 30 participants, 13 deliveries, 16 pickups, three drops and 148 deaths. Twelve completed delivery resets took 10,004–10,134ms. The final delivery ended the match. Three Horde bots initially stalled on one-point paths. Elevated unmounted bot speeds were observed; the human tester's explicit GM speed override is excluded from the defect evidence.

## Report-by-report implementation

| Report | Change and remaining live check |
| --- | --- |
| Capture speed | Existing prototype tuning retained: solo neutral capture about 30s, net two-player advantage about 15.6s. Historical rules are not established. |
| Center position and base icons | Previously corrected user GPS and native neutral/faction base icons retained; user confirmed both. |
| Inconsistent flag size | Center and dropped objects both scale 2.5 before initial visibility and collision publication. Verify first spawn, drop and repeated reset. |
| Timer and sounds | Ten-second delivery reset and 30-second dropped return retained. Explicit chat countdown, native WSG capture/reset sounds and companion HUD using the actual server timer. |
| Horde and Alliance gates | Native collision doors, closed during preparation and opened at start. Horde doorway uses supplied GPS; Alliance placement comes from the tunnel model opening. Countdown containment is retained. Both visual fit and open passage need acceptance. |
| Carrier chases kills | TG-only delivery priority above proactive attacks, below critical survival; native casts and eligibility preserved. |
| Objective allocation | Stable runner, escort/interceptor, defender and distributed capture assignments. Nearby fighting allowed; excessive pursuit yields to objective travel. Enemy carriers recognized through native TG membership and carry aura. Other BG strategies unchanged. |
| Ignoring humans | Earlier noncombat PvP reset fix retained. Human targeting and objective combat need another live run. God mode is not treated as GM invisibility. |
| Floating and mountain shortcuts | Earlier ground-spline continuation repair retained. Map821 now opts into native steep-slope exclusion with no blind/forced shortcut through an excluded route. Native query budget increased only for821. Tight-corner oscillation repaired in the native smoother. |
| Bots stuck in hut | Unchanged one-point paths now report failure. TG-only bounded native ballistic jumps can traverse small obstacles when walking fails; collision, landing and subsequent walking progress must pass. This does not guarantee every spawn exit is solved until tested live. |
| Unmounted high speed | Socketless controlled players apply native speed transitions immediately; they no longer queue impossible client ACKs that can restore stale mounted speed after dismount. Real-client ACK behavior and aura calculations preserved. |
| Four bridge step-ups | Client patch adds eight existing wooden ramps over the raised beams; matching native vmaps/mmaps rebuilt. No shared model change or teleport workaround. Test a gnome in both directions at all four ends. |
| Horde flag icon blue | Companion receives carrier team and selects faction texture on world map and battlefield minimap. Native coordinates retained. |
| Player icons missing at start | Expanded map821 WorldMapArea bounds include both spawns; companion refits/crops map art to those bounds. Other DBC rows unchanged. Native player positions are not clamped or spoofed. |
| Queue NPCs and zone | Existing spawned battlemaster NPCs: Fanwyn Wildbrand in Ironforge, Ingwelda Wildbrand in Stormwind, Suda Steelweaver in Thunder Bluff. Imported unspawned templates remain unspawned. Quests42098/42099 corrected from Moonwhisper Coast5642 to Thorn Gorge5722 only. |
| Rewards | Existing 40 honor per delivery, 100 completion honor plus200 winner honor retained. Victory quests retain XP5900, max-level money35400 and150 Ironforge/Thunder Bluff reputation. No invented marks, dedicated faction, vendor ladder or reward sets: imported evidence does not establish them. |
| Diagnostics | Bounded level2 human/bot floor, flags, speed/aura and spline samples; bounded traversal acceptance/rejection records. Controls and overhead in TURTLE_DIAGNOSTICS.md. |

## Validation and delivery

65 architecture tests pass, covering native speed ACK variants, filter exclusion and map scoping, no-progress paths, bounded native traversal, objective allocation/priorities, object publication, timers/sounds and diagnostic admission. Companion Lua tests cover stale/forged messages, faction textures, countdown, map/minimap crop/resize and restoration on other maps. Release compilation completes.

Offline extraction rebuilt map821 from the same four patched ADTs shipped to clients. Eight ramp transforms match assembled collision, slopes approximately4–27 degrees; repeat builders reproduce identical bytes. The native asset probe at16384 nodes produces42 usable paths, zero query exhaustion and zero smoothing failures. With steep exclusion,20 central-objective routes complete as walking paths;22 routes involving spawns remain partial and require native traversal. Usable partial paths are not proof that bots arrive.

Deployment selects a separate complete data directory, leaving the running server's original geometry intact until restart. Ship exactly five client MPQ files: four ADTs and WorldMapArea.dbc. Never ship the extraction-only reduced Map.dbc. The client addon and MPQ are both necessary; fully restart the client as well as the world server.

Live acceptance: both gates/countdown exits, gnome bridge passage, bot spawn departure and terrain routes, human combat, flag delivery under pressure, carrier colour, first/drop/respawn flag scale, timers/sounds, player icons at both starts and queue NPCs. Review fresh movement/traversal logs before declaring these verified.
