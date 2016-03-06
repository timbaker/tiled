local tiles = ""
for i=0,9 do
    tiles = tiles .. "vegetation_ornamental_01_" .. i .. ";" .. "f_bushes_2_" .. i .. ";"
end

map:replaceTilesByName(tiles)

