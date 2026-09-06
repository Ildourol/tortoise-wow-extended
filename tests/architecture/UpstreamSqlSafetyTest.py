"""Execute the selected SQL guards on fixtures, not the live database.

SQLite substitutes INSERT OR IGNORE for MySQL INSERT IGNORE. MySQL schema/
syntax validation is separate; this tests preservation and repeat execution.
"""
from pathlib import Path
import sqlite3

root = Path(__file__).resolve().parents[2]
db = sqlite3.connect(':memory:')
db.executescript('''
CREATE TABLE creature (guid INTEGER PRIMARY KEY);
CREATE TABLE creature_movement (id INTEGER, point INTEGER);
CREATE TABLE creature_linking (guid INTEGER PRIMARY KEY);
CREATE TABLE creature_template (entry INTEGER PRIMARY KEY, gossip_menu_id INTEGER);
CREATE TABLE gossip_menu (entry INTEGER, text_id INTEGER, script_id INTEGER, condition_id INTEGER,
 PRIMARY KEY(entry,text_id));
CREATE TABLE sound_entries (id INTEGER PRIMARY KEY, name TEXT);
CREATE TABLE broadcast_text (entry INTEGER PRIMARY KEY, male_text TEXT, female_text TEXT,
 chat_type INTEGER, sound_id INTEGER, language_id INTEGER, emote_id1 INTEGER, emote_id2 INTEGER,
 emote_id3 INTEGER, emote_delay1 INTEGER, emote_delay2 INTEGER, emote_delay3 INTEGER);
CREATE TABLE npc_text (ID INTEGER PRIMARY KEY,
 BroadcastTextID0 INTEGER, Probability0 REAL, BroadcastTextID1 INTEGER, Probability1 REAL,
 BroadcastTextID2 INTEGER, Probability2 REAL, BroadcastTextID3 INTEGER, Probability3 REAL,
 BroadcastTextID4 INTEGER, Probability4 REAL, BroadcastTextID5 INTEGER, Probability5 REAL,
 BroadcastTextID6 INTEGER, Probability6 REAL, BroadcastTextID7 INTEGER, Probability7 REAL);
INSERT INTO creature VALUES (8880),(99967);
INSERT INTO creature_movement VALUES (8880,1),(8988,1),(42,1);
INSERT INTO creature_linking VALUES (99967),(99968),(43);
INSERT INTO creature_template VALUES (62306,9001),(62380,0),(62479,0),(62480,0);
INSERT INTO sound_entries VALUES (60443,'local-custom');
INSERT INTO broadcast_text(entry,male_text,chat_type,emote_id1) VALUES
 (6230601,'local-custom',0,0),(6249501,'boss',12,0),(6249502,'local-channel',6,0),
 (6271501,'boss',11,0),(92031,'emote',1,55);
''')
files = ['20260903115534_world.sql', '20260905175321_world.sql', '20260906134216_world.sql']
def snapshot():
    tables = [r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")]
    return {t: db.execute(f'SELECT * FROM {t} ORDER BY 1').fetchall() for t in tables}
for repeat in range(2):
    for name in files:
        sql = (root / 'sql/database_updates/world' / name).read_text(encoding='utf-8-sig')
        db.executescript(sql.replace('INSERT IGNORE', 'INSERT OR IGNORE'))
    current = snapshot()
    if repeat:
        assert current == previous, 'second execution changed the result'
    previous = current
assert current['creature_movement'] == [(42,1),(8880,1)]
assert current['creature_linking'] == [(43,),(99967,)]
assert current['creature_template'] == [(62306,9001),(62380,62380),(62479,62479),(62480,62480)]
assert current['sound_entries'] == [(60443,'local-custom'),(60444,'Dukedread2')]
assert db.execute('SELECT male_text FROM broadcast_text WHERE entry=6230601').fetchone()[0] == 'local-custom'
assert db.execute('SELECT chat_type FROM broadcast_text WHERE entry=6249501').fetchone()[0] == 1
assert db.execute('SELECT chat_type FROM broadcast_text WHERE entry=6249502').fetchone()[0] == 6
assert db.execute('SELECT chat_type FROM broadcast_text WHERE entry=6271501').fetchone()[0] == 0
assert db.execute('SELECT emote_id1 FROM broadcast_text WHERE entry=92031').fetchone()[0] == 0
assert len(current['npc_text']) == 4 and len(current['gossip_menu']) == 4
print('PASS: selected SQL preserves custom rows, live spawn paths, unrelated rows, and is repeatable')
