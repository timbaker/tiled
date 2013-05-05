sel = map:tileSelection()
if sel:isEmpty() then
    print('tile selection is empty')
    return
end
bounds = map:tileSelection():boundingRect()

 -- weird: the class is Layer not TileLayer
 -- calling layer:asTileLayer() changes the class to TileLayer!
layer = map:layer("0_Floor")

if not layer or not layer:asTileLayer() then
    print("there's no 0_Floor layer in this map")
    return
end

layer = layer:asTileLayer()

tile = map:tile('floors_exterior_street_01_16')
if not tile then
    print("no such tile 'floors_exterior_street_01_16' you arse-bucket!")
    return
end

layer:fill(bounds, tile)
