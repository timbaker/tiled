function west_parking_stall(x,y)
    layer = map:tileLayer('0_Furniture')
    tile = map:tile('street_decoration_01_37')
    layer:setTile(x,y,tile)
    tile = map:tile('street_decoration_01_36')
    layer:setTile(x,y+1,tile)
    tile = map:tile('street_decoration_01_35')
    layer:setTile(x,y+2,tile)

    layer = map:tileLayer('0_FloorOverlay')
    tile = map:tile('street_trafficlines_01_2')
    layer:fill(x,y,4,1,tile)
end
