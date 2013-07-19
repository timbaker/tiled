-- Replaces the old sand tiles in floors_exterior_natural_01 with
-- the new ones in blends_natural_01.

old_sand = 24
old_sand_nw = 25
old_sand_se = 26
old_sand_sw = 27
old_sand_ne = 28

new_sand = {0, 5, 6, 7}
new_sand_nw = 1
new_sand_se = 2
new_sand_sw = 3
new_sand_ne = 4

layerF = map:layer("0_Floor"):asTileLayer()
layerFO = map:layer("0_FloorOverlay"):asTileLayer()
layerFO2 = map:layer("0_FloorOverlay2"):asTileLayer()
layerFO3 = map:layer("0_FloorOverlay3"):asTileLayer()
layerFO4 = map:layer("0_FloorOverlay4"):asTileLayer()

math.randomseed( os.time() )

layerF:replaceTile(map:tile('floors_exterior_natural_01',old_sand), map:tile('blends_natural_01',new_sand[math.random(1,4)]))
layerFO2:replaceTile(map:tile('floors_exterior_natural_01',old_sand_ne), map:tile('blends_natural_01',new_sand_ne))

