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

DIR = {}
DIR[BmpBlend.W] = 'W'
DIR[BmpBlend.N] = 'N'
DIR[BmpBlend.E] = 'E'
DIR[BmpBlend.S] = 'S'
DIR[BmpBlend.NW] = 'NW'
DIR[BmpBlend.NE] = 'NE'
DIR[BmpBlend.SE] = 'SE'
DIR[BmpBlend.SW] = 'SW'

-- blends['darkgrass'].NW = {tile, tile, ...}
-- blends['darkgrass'].N = {tile, tile, ...}
blends = {}
mainTiles = {}
for _,blend in ipairs(map:blends()) do
	if not blends[blend:mainTile()] then
		blends[blend:mainTile()] = {}
		table.insert(mainTiles, blend:mainTile())
	end
	tiles = {}
	for _,tileName in pairs(resolveAliases({blend:blendTile()})) do
		tile = map:tile(tileName)
		if tile then
			table.insert(tiles, tile)
		end
	end
	blends[blend:mainTile()][DIR[blend:direction()]] = tiles
end

overlay1 = map:tileLayer('0_FloorOverlay')
overlay2 = map:tileLayer('0_FloorOverlay2')
overlay3 = map:tileLayer('0_FloorOverlay3')
overlay4 = map:tileLayer('0_FloorOverlay4')

function inTable(table, elem)
	if not table or not elem then return false end
	for _,v in pairs(table) do
		if v == elem then return true end
	end
	return false;
end

function isCorner(tile)
	for _,mainTile in pairs(mainTiles) do
		if inTable(blends[mainTile].NW, tile) then
--print('tile is NW')
			return { blends[mainTile].N, blends[mainTile].W }
		end
		if inTable(blends[mainTile].NE, tile) then
--print('tile is NE')
			return { blends[mainTile].N, blends[mainTile].E }
		end
		if inTable(blends[mainTile].SE, tile) then
--print('tile is SE ' .. mainTile)
			return { blends[mainTile].S, blends[mainTile].E }
		end
		if inTable(blends[mainTile].SW, tile) then
--print('tile is SW')
			return { blends[mainTile].S, blends[mainTile].W }
		end
	end
	return nil
end

function removeEdge(layer, x, y, tile)
	print('removed ' .. tile:tileset():name() .. "_" .. tile:id() .. " " .. x .. ',' .. y)
	layer:clearTile(x, y)
end

function removeEdges(x, y, edges, tile1, tile2, tile3, tile4)
	for _,tiles in pairs(edges) do
		if inTable(tiles, tile1) then removeEdge(overlay1, x, y, tile1) end
		if inTable(tiles, tile2) then removeEdge(overlay2, x, y, tile2) end
		if inTable(tiles, tile3) then removeEdge(overlay3, x, y, tile3) end
		if inTable(tiles, tile4) then removeEdge(overlay4, x, y, tile4) end
	end
end

for y=0,map:height()-1 do
	for x=0,map:width()-1 do
--x,y = 259,198
		tile1 = overlay1:tileAt(x,y)
		tile2 = overlay2:tileAt(x,y)
		tile3 = overlay3:tileAt(x,y)
		tile4 = overlay4:tileAt(x,y)
		edges = isCorner(tile1)
		if edges then
--print('tile1 is corner')
			removeEdges(x, y, edges, "", tile2, tile3, tile4)
		end
		edges = isCorner(tile2)
		if edges then
--print('tile2 is corner')
			removeEdges(x, y, edges, tile1, "", tile3, tile4)
		end
		edges = isCorner(tile3)
		if edges then
--print('tile3 is corner')
			removeEdges(x, y, edges, tile1, tile2, "", tile4)
		end
		edges = isCorner(tile4)
		if edges then
--print('tile4 is corner')
			removeEdges(x, y, edges, tile1, tile2, tile3, "")
		end
	end
end

