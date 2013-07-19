map:reloadRules()
map:reloadBlends()

dofile(scriptDirectory..'/remove-unknown-colors.lua')

floor = map:tileLayer('0_Floor')
overlay1 = map:tileLayer('0_FloorOverlay')
overlay2 = map:tileLayer('0_FloorOverlay2')
overlay3 = map:tileLayer('0_FloorOverlay3')
overlay4 = map:tileLayer('0_FloorOverlay4')

--floor:replaceTile(map:tile('floors_exterior_natural_01_24'),map:tile('blends_natural_01_0'))
if true then
    dofile(scriptDirectory..'/remove-blend-tiles.lua')
else
    overlay1:erase()
    overlay2:erase()
    overlay3:erase()
    overlay4:erase()
end

bmp0 = map:bmp(0)

function resolveAliases(tiles)
    local ret = {}
    for _,tile in pairs(tiles) do
	local alias = map:alias(tile)
	if alias then
	    for k,v in pairs(alias:tiles()) do table.insert(ret,v) end
	else
	    table.insert(ret,tile)
	end
    end
    return ret
end

ruleToColor = {}
tileToRule = {}
for _,rule in ipairs(map:rules()) do
    ruleToColor[rule] = rule:color()
    for _,tileName in pairs(resolveAliases(rule:tiles())) do
	-- print(tileName..' -> rule='.. tostring(rule))
	tile = map:tile(tileName)
	if tile then
	    tileToRule[map:tile(tileName)] = rule
	elseif #tileName > 0 then
	    print(tileName..' NOT FOUND')
	end
    end
end

black = rgb(0,0,0).pixel
for y=0,map:height()-1 do
    for x=0,map:width()-1 do
	tile = floor:tileAt(x,y)
	if tile and bmp0:pixel(x,y) == black then
	    rule = tileToRule[tile]
	    if rule then
		floor:clearTile(x,y)
		bmp0:setPixel(x,y,ruleToColor[rule])
	    end
	elseif tile then
	    floor:clearTile(x,y)
	end
    end
end
