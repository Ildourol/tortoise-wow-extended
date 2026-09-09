"""Exercise upstream migration effects on local fixtures using repository columns.

SQLite validates names, statements and resulting relations, not MySQL types,
live ID availability, DBC contents, or gameplay. No production connection.
"""
from pathlib import Path
import re
import sqlite3

root = Path(__file__).resolve().parents[2]
sql_root = root / 'sql/database_updates/world'
spell_sql = (sql_root / '20260907165343_world.sql').read_text(encoding='utf-8')
quest_sql = (sql_root / '20260908193409_world.sql').read_text(encoding='utf-8')
tables = set(re.findall(r'(?:INSERT INTO|UPDATE|DELETE FROM)\s+`?(\w+)', spell_sql + quest_sql))
db = sqlite3.connect(':memory:')
for table in sorted(tables):
    schema = (root / 'sql/base' / f'tw_world_{table}.sql').read_text(encoding='utf-8')
    ddl = re.search(r'CREATE TABLE `\w+` \((.*?)\n\) ENGINE=', schema, re.S)[1]
    columns = re.findall(r'^\s+(`\w+`)\s', ddl, re.M)
    keys = re.findall(r'PRIMARY KEY\s*(\([^\n]*?\))', ddl)
    db.execute(f'CREATE TABLE `{table}` ({",".join(columns + ["PRIMARY KEY " + k for k in keys])})')

db.executescript('''
INSERT INTO quest_template(entry,SrcItemCount,StartScript,SpecialFlags) VALUES
 (41806,1,0,16),(41731,0,0,16),(41848,0,0,16),(41802,0,0,16),(999999,9,99,32);
INSERT INTO creature_template(entry,scale,spell_list_id,gossip_menu_id) VALUES
 (62585,1,0,0),(62569,1,0,0),(62432,1,0,0),(62421,1,0,0),(999999,3,99,99);
INSERT INTO gameobject_template(entry,flags) VALUES (2020229,0),(999999,16);
INSERT INTO gossip_menu(entry,text_id,script_id,condition_id) VALUES (999999,999999,0,0);
INSERT INTO gossip_menu_option(menu_id,id,option_text) VALUES (999999,0,'Custom option');
INSERT INTO creature_loot_template(entry,item,ChanceOrQuestChance) VALUES (4857,999999,25);
INSERT INTO areatrigger_involvedrelation(id,quest) VALUES (999999,999999);
INSERT INTO spell_chain(spell_id,first_spell,req_spell) VALUES
 (3035,0,0),(16689,16689,339),(13165,13165,0),(999999,999999,42);
INSERT INTO spell_proc_event(entry,ppmRate) VALUES (15335,0),(15270,0),(999999,3);
''')
db.executescript(spell_sql)
assert db.execute('SELECT first_spell FROM spell_chain WHERE spell_id=3035').fetchone() == (3035,)
assert db.execute('SELECT req_spell FROM spell_chain WHERE spell_id=16689').fetchone() == (0,)
assert not db.execute('SELECT 1 FROM spell_chain WHERE spell_id=13165').fetchone()
assert db.execute('SELECT req_spell FROM spell_chain WHERE spell_id=999999').fetchone() == (42,)
assert db.execute('SELECT entry FROM spell_proc_event ORDER BY entry').fetchall() == [(15270,), (999999,)]
db.execute('UPDATE spell_chain SET first_spell=123 WHERE spell_id=3035')
db.execute('UPDATE spell_chain SET req_spell=123 WHERE spell_id=16689')
db.executescript(spell_sql)
assert db.execute('SELECT first_spell FROM spell_chain WHERE spell_id=3035').fetchone() == (123,)
assert db.execute('SELECT req_spell FROM spell_chain WHERE spell_id=16689').fetchone() == (123,)

db.executescript(quest_sql)
assert db.execute('SELECT SrcItemCount FROM quest_template WHERE entry=41806').fetchone() == (3,)
assert db.execute('SELECT StartScript FROM quest_template WHERE entry=41731').fetchone() == (41731,)
assert db.execute('SELECT SpecialFlags FROM quest_template WHERE entry IN (41848,41802)').fetchall() == [(18,), (18,)]
assert db.execute('SELECT SrcItemCount,StartScript,SpecialFlags FROM quest_template WHERE entry=999999').fetchone() == (9,99,32)
assert db.execute('SELECT scale,spell_list_id,gossip_menu_id FROM creature_template WHERE entry=999999').fetchone() == (3,99,99)
assert db.execute('SELECT scale,spell_list_id FROM creature_template WHERE entry=62585').fetchone() == (2.5,62585)
assert db.execute('SELECT spellId_1,spellId_2 FROM creature_spells WHERE entry=62585').fetchone() == (14145,16046)
assert db.execute('SELECT type,value1,value2 FROM conditions WHERE condition_entry=41742').fetchone() == (9,41742,1)
assert db.execute('SELECT action_script_id,condition_id FROM gossip_menu_option WHERE menu_id=41904').fetchone() == (4190402,41742)
assert db.execute('SELECT command,datalong,dataint4 FROM gossip_scripts WHERE id=4190402 AND delay=3').fetchone() == (10,62585,7)
assert db.execute('SELECT command,datalong,condition_id FROM gossip_scripts WHERE id=6242104').fetchone() == (8,60081,41803)
assert db.execute('SELECT action_menu_id,action_script_id FROM gossip_menu_option WHERE menu_id=6242104').fetchone() == (6242105,6242104)
assert db.execute('SELECT COUNT(*) FROM quest_start_scripts WHERE id=41731').fetchone() == (11,)
assert db.execute('SELECT delay,datalong,datalong2,datalong3 FROM quest_start_scripts WHERE id=41731 AND command=4 ORDER BY delay').fetchall() == [(0,147,3,2),(20,147,3,1)]
assert db.execute('SELECT delay,datalong FROM quest_start_scripts WHERE id=41731 AND command=8').fetchone() == (19,60079)
assert db.execute('SELECT data_flags FROM quest_start_scripts WHERE id=41731 AND command=15 AND datalong=51206').fetchone() == (4,)
assert db.execute('SELECT id,map FROM gameobject WHERE guid=5025863').fetchone() == (2020229,0)
assert db.execute('SELECT item,ChanceOrQuestChance FROM creature_loot_template WHERE entry=4857 ORDER BY item').fetchall() == [(41800,-100),(999999,25)]
for table in ('creature_questrelation', 'creature_involvedrelation'):
    assert db.execute(f'SELECT quest FROM {table} WHERE id=62569').fetchone() == (41894,)
assert db.execute('SELECT id,quest FROM areatrigger_involvedrelation ORDER BY id,quest').fetchall() == [(700,41802),(5601,41848),(999999,999999)]
assert db.execute('SELECT option_text FROM gossip_menu_option WHERE menu_id=999999').fetchone() == ('Custom option',)
assert not db.execute('SELECT 1 FROM npc_text n LEFT JOIN broadcast_text b ON b.entry=n.BroadcastTextID0 WHERE b.entry IS NULL').fetchone()
assert not db.execute('SELECT 1 FROM gossip_menu g LEFT JOIN npc_text n ON n.ID=g.text_id WHERE g.entry<>999999 AND n.ID IS NULL').fetchone()
assert db.execute('SELECT chat_type FROM broadcast_text WHERE entry IN (4190403,6243202)').fetchall() == [(2,),(2,)]
print('PASS: Penqle spell guards, quest relations, conditional gossip, native script data, and unrelated fixture preservation')
