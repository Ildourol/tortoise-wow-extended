-- Run with a Lua interpreter; UI methods are mocked, production addon is loaded.
math.mod = math.mod or math.fmod
unpack = unpack or table.unpack
local now,map=0,"ThornGorge"
local function region()
    local r={width=256,height=256,point={"TOPLEFT","parent","TOPLEFT",0,0},shown=true,text=""}
    function r:GetWidth() return self.width end
    function r:GetHeight() return self.height end
    function r:SetWidth(v) self.width=v end
    function r:SetHeight(v) self.height=v end
    function r:GetPoint() return unpack(self.point) end
    function r:SetPoint(...) self.point={...} end
    function r:ClearAllPoints() self.point={} end
    function r:SetTexture(v) self.texture=v end
    function r:Show() self.shown=true end
    function r:Hide() self.shown=false end
    function r:GetText() return self.text end
    function r:CreateTexture(name) local t=region();_G[name]=t;return t end
    function r:GetTexCoord() return 0,1,0,1 end
    function r:SetTexCoord(...) self.uv={...} end
    function r:IsShown() return self.shown end
    function r:SetAllPoints() end
    function r:SetText(v) self.text=v end
    function r:RegisterEvent() end
    function r:SetScript(e,f) self[e]=f end
    function r:CreateFontString() self.label=region();return self.label end
    return r
end
UIParent=region();WorldMapDetailFrame=region();WorldMapDetailFrame.width=1002;WorldMapDetailFrame.height=668
WorldMapZoneDropDownText=region();WorldMapZoneDropDownText.text="Elwynn Forest"
WorldMapContinentDropDownText=region();WorldMapContinentDropDownText.text="Eastern Kingdoms"
for i=1,12 do _G["WorldMapDetailTile"..i]=region() end
WorldMapFlag1Texture=region();WorldMapOverlay1=region();NUM_WORLDMAP_OVERLAYS=1
function CreateFrame(_,name) _G[name]=region();return _G[name] end
function GetTime() return now end
function GetMapInfo() return map end
function UnitName() return "Tester" end
function GetNumBattlefieldFlagPositions() return 1 end
function WorldMapFrame_Update() WorldMapOverlay1.width=64;WorldMapOverlay1.height=32;WorldMapOverlay1.point={"TOPLEFT",WorldMapDetailFrame,"TOPLEFT",100,-200} end
function WorldMapButton_OnUpdate() WorldMapFlag1Texture.texture="native" end
assert(loadfile(arg[1]))()
local f=ManTechThornGorgeStatus
-- Simulate the Blizzard load-on-demand map arriving after this addon.
BattlefieldMinimap=region();BattlefieldMinimap.width=200;BattlefieldMinimap.height=133.333
for i=1,12 do _G["BattlefieldMinimap"..i]=region();_G["BattlefieldMinimap"..i].width=50 end
BattlefieldMinimapFlag1Texture=region();BattlefieldMinimapOverlay1=region();NUM_BATTLEFIELDMAP_OVERLAYS=1
function BattlefieldMinimap_Update()
    BattlefieldMinimapOverlay1.width=64*BattlefieldMinimap1.width/256
    BattlefieldMinimapOverlay1.height=32*BattlefieldMinimap1.width/256
    BattlefieldMinimapOverlay1.point={"TOPLEFT",BattlefieldMinimap,"TOPLEFT",10,-20}
end
function BattlefieldMinimap_OnUpdate() BattlefieldMinimapFlag1Texture.texture="native" end
event="ADDON_LOADED";arg1="Blizzard_BattlefieldMinimap";f.OnEvent()
for i=1,3 do
    BattlefieldMinimap_Update()
    assert(math.abs(BattlefieldMinimap1.width-50*694/870)<.001)
    assert(math.abs(BattlefieldMinimapOverlay1.width-12.5*694/870)<.001)
end
BattlefieldMinimap.resizing=true;BattlefieldMinimap.width=400;BattlefieldMinimap_OnUpdate(0)
assert(math.abs(BattlefieldMinimap1.width-100*694/870)<.001)
assert(math.abs(BattlefieldMinimapOverlay1.width-25*694/870)<.001)
BattlefieldMinimap.resizing=false
local function receive(payload,sender,prefix)
 event="CHAT_MSG_ADDON";arg1=prefix or "MT_TG1";arg2=payload;arg3="GUILD";arg4=sender or "Tester";f.OnEvent();f.OnUpdate()
end
for i=1,3 do WorldMapFrame_Update();assert(math.abs(WorldMapDetailTile1.width-256*694/870)<.001);assert(math.abs(WorldMapOverlay1.width-64*694/870)<.001) end
assert(WorldMapZoneDropDownText.text=="Thorn Gorge")
assert(WorldMapContinentDropDownText.text=="Battleground")
local outerArea=0
for i=1,4 do local t=_G["ManTechThornMapMargin"..i];assert(t.shown);outerArea=outerArea+t.width*t.height end
assert(math.abs(outerArea+1002*668*(694/870)^2-1002*668)<.001)
assert(math.abs(WorldMapDetailTile12.height-156*694/870)<.001)
assert(math.abs(WorldMapDetailTile12.width-234*694/870)<.001)
receive("1;101;3;1;1;0");WorldMapButton_OnUpdate(0)
assert(WorldMapFlag1Texture.texture=="Interface\\WorldStateFrame\\HordeFlag")
BattlefieldMinimap_OnUpdate(0)
assert(BattlefieldMinimapFlag1Texture.texture=="Interface\\WorldStateFrame\\HordeFlag")
receive("1;101;3;1;0;0");WorldMapButton_OnUpdate(0)
assert(WorldMapFlag1Texture.texture=="Interface\\WorldStateFrame\\AllianceFlag")
receive("1;101;3;3;2;10000");assert(string.find(f.label.text,"10s",1,true))
now=2.1;f.OnUpdate();assert(string.find(f.label.text,"8s",1,true))
receive("1;101;3;3;2;999999");assert(string.find(f.label.text,"8s",1,true))
receive("1;101;3;1;1;0","AnotherPlayer");assert(string.find(f.label.text,"8s",1,true))
now=6;f.OnUpdate();assert(f.label.text=="")
receive("1;101;4;0;2;0");assert(f.label.text=="")
map="WarsongGulch";WorldMapFrame_Update();WorldMapButton_OnUpdate(0)
assert(WorldMapDetailTile1.width==256 and WorldMapDetailTile1.point[2]=="parent")
assert(WorldMapFlag1Texture.texture=="native")
BattlefieldMinimap_Update();BattlefieldMinimap_OnUpdate(0)
assert(BattlefieldMinimap1.width==100 and BattlefieldMinimap1.point[2]=="parent")
assert(BattlefieldMinimapFlag1Texture.texture=="native")
assert(not ManTechThornMapMargin1.shown, "margin leaked to another map")
assert(WorldMapZoneDropDownText.text=="Elwynn Forest", "zone label not restored")
assert(WorldMapContinentDropDownText.text=="Eastern Kingdoms", "continent label not restored")
map="ThornGorge";WorldMapFrame_Update();receive("1;102;3;2;2;30000");assert(string.find(f.label.text,"30s",1,true))
event="ZONE_CHANGED_NEW_AREA";f.OnEvent();f.OnUpdate();assert(f.label.text=="")
print("Production addon: both carrier colours, timer, stale/forged messages, repeat map updates and non-TG restoration passed")
