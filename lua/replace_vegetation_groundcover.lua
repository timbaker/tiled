-- the flowers on row 3 are replaced with NaturePlants (in Java).
-- the game replaces all blends_grassoverlays with e_newgrass which are seasonal.

-- greenest, short to tall
local tiles = "vegetation_groundcover_01_0;blends_grassoverlays_01_16;"
tiles = tiles .. "vegetation_groundcover_01_1;blends_grassoverlays_01_8;"
tiles = tiles .. "vegetation_groundcover_01_2;blends_grassoverlays_01_0;"

-- lightest, short to tall
tiles = tiles .. "vegetation_groundcover_01_3;blends_grassoverlays_01_64;"
tiles = tiles .. "vegetation_groundcover_01_4;blends_grassoverlays_01_56;"
tiles = tiles .. "vegetation_groundcover_01_5;blends_grassoverlays_01_48;"

-- in-between green, short to tall
tiles = tiles .. "vegetation_groundcover_01_44;blends_grassoverlays_01_40;"
tiles = tiles .. "vegetation_groundcover_01_45;blends_grassoverlays_01_32;"
tiles = tiles .. "vegetation_groundcover_01_46;blends_grassoverlays_01_24;"

map:replaceTilesByName(tiles)

