sel = map:tileSelection()
if sel:isEmpty() then
    print('the tile selection is empty, don\'t know where to make the street')
    return
end
bounds = map:tileSelection():boundingRect()

if true then
    layer = map:tileLayer('0_Floor')
    if not layer then
        print("there's no 0_Floor layer in this map")
        return
	end
else
    -- weird: the class is Layer not TileLayer
    -- calling layer:asTileLayer() changes the class to TileLayer!
    layer = map:layer("0_Floor")

    if not layer or not layer:asTileLayer() then
        print("there's no 0_Floor layer in this map")
        return
	end

	layer = layer:asTileLayer()
end

tile = map:tile('floors_exterior_street_01_16')
if not tile then
    print("no such tile 'floors_exterior_street_01_16'")
    return
end

layer:fill(bounds, tile)
