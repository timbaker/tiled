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

blendTilesByLayer = {}
blendTilesByLayer['0_FloorOverlay'] = {}
blendTilesByLayer['0_FloorOverlay2'] = {}
blendTilesByLayer['0_FloorOverlay3'] = {}
blendTilesByLayer['0_FloorOverlay4'] = {}
for _,blend in ipairs(map:blends()) do
    for _,tileName in pairs(resolveAliases({blend:blendTile()})) do
	tile = map:tile(tileName)
	if tile then
	    blendTilesByLayer[blend:layer()][tile] = true
	end
    end
end

overlay1 = map:tileLayer('0_FloorOverlay')
overlay2 = map:tileLayer('0_FloorOverlay2')
overlay3 = map:tileLayer('0_FloorOverlay3')
overlay4 = map:tileLayer('0_FloorOverlay4')

for y=0,map:height()-1 do
    for x=0,map:width()-1 do
	tile = overlay1:tileAt(x,y)
	if tile and blendTilesByLayer['0_FloorOverlay'][tile] then
	    overlay1:clearTile(x,y)
	end
	tile = overlay2:tileAt(x,y)
	if tile and blendTilesByLayer['0_FloorOverlay2'][tile] then
	    overlay2:clearTile(x,y)
	end
	tile = overlay3:tileAt(x,y)
	if tile and blendTilesByLayer['0_FloorOverlay3'][tile] then
	    overlay3:clearTile(x,y)
	end
	tile = overlay4:tileAt(x,y)
	if tile and blendTilesByLayer['0_FloorOverlay4'][tile] then
	    overlay4:clearTile(x,y)
	end
    end
end

