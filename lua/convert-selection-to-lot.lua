-- TileZed lua script
-- Convert the selected area of the map to a new Lot

---------------------------
--- TODO: allow scripts to make some sort of UI for configuring
TMX_FILE = './convert-to-lot.tmx'
---------------------------

if map:tileSelection():isEmpty() then return end
bounds = map:tileSelection():boundingRect()

mapOrient = Map.LevelIsometric
mapWidth = bounds:width()
mapHeight = bounds:height()
maxLevel = map:maxLevel()

mapOffset = {x=0, y=0}
if mapOrient == Map.Isometric then
    offset = maxLevel * 3
    mapOffset.x = offset
    mapOffset.y = offset
    mapWidth = mapWidth + offset
    mapHeight = mapHeight + offset
end

clone = Map:new(mapOrient, mapWidth, mapHeight)
for i=0,map:tilesetCount()-1 do
    clone:addTileset(map:tilesetAt(i))
end

for i=0,map:layerCount()-1 do
    layer = map:layerAt(i)
    tl = layer:asTileLayer()
    if tl then
	if tl:level() <= maxLevel then
	    cloneLayer = clone:newTileLayer(tl:name())
	    clone:addLayer(cloneLayer)

	    offset = 0
	    offsetSrc = 0
	    if mapOrient == Map.Isometric then
		offset = mapOffset.x - tl:level() * 3
	    end
	    if map:orientation() == Map.Isometric then
		offsetSrc = -tl:level() * 3
	    end

	    for y=bounds:top(),bounds:bottom() do
		for x=bounds:left(),bounds:right() do
		    if x + offsetSrc < 0 or y + offsetSrc < 0 then
		    elseif tl:tileAt(x + offsetSrc, y + offsetSrc) then
			cloneLayer:setTile(offset + x - bounds:left(),
					   offset + y - bounds:top(),
					   tl:tileAt(x + offsetSrc, y + offsetSrc))
		    end
		end
	    end
	end
    end
    og = layer:asObjectGroup()
    if og then
	    print(og:name())
	if string.match(og:name(),'%d+_RoomDefs') then
	    -- FIXME: don't clone the layer if there aren't any RoomDefs.
	    cloneLayer = ObjectGroup:new(og:name(), 0, 0, mapWidth, mapHeight)
	    cloneLayer:setColor(og:color())
	    clone:addLayer(cloneLayer)

	    offset = mapOffset
	    if map:orientation() == Map.LevelIsometric and
		    mapOrient == Map.Isometric then
		offset.x = offset.x - 3 * og:level()
		offset.y = offset.y - 3 * og:level()
	    end
	    if map:orientation() == Map.Isometric and
		    mapOrient == Map.LevelIsometric then
		offset.x = 3 * og:level()
		offset.y = 3 * og:level()
	    end

	    remove = {}
	    objects = og:objects()
	    for j=1,#og:objects() do
		o = objects[j]
		objectBounds = o:bounds()
		if map:orientation() == Map.Isometric then
		    objectBounds:translate(og:level() * 3, og:level() * 3)
		end
		if objectBounds:intersects(bounds) then
		    cloneObj = MapObject:new(o:name(),
			o:type(), offset.x + o:bounds():x() - bounds:x(),
			offset.y + o:bounds():y() - bounds:y(),
			o:bounds():width(), o:bounds():height())
		    cloneLayer:addObject(cloneObj)
		end
	    end
	end
    end
end

clone:write(TMX_FILE)
