-- remove multiple vegetation_groundcover plants on the same square

tileset1 = map:tileset("vegetation_groundcover_01")
if not tileset1 then return end

-- FIXME: only check layers in each level

count = 0
for i=map:layerCount(),1,-1 do
	layer = map:layerAt(i-1)
	tl = layer:asTileLayer()
	if tl then
		for y=0,map:height()-1 do
			for x=0,map:width()-1 do
				tile = tl:tileAt(x, y)
				if tile and (tile:tileset() == tileset1) and (tile:id() >= 18 and tile:id() <= 23) then
					for j=i-1,1,-1 do
						tl2 = map:layerAt(j-1):asTileLayer()
						if tl2 then
							tile2 = tl2:tileAt(x, y)
							if tile2 and (tile2:tileset() == tileset1) and (tile2:id() >= 18 and tile2:id() <= 23) then
								print('removed id=' .. tile2:id() .. ' in layer ' .. tl2:name() .. ' at ' .. x .. ',' .. y)
								tl2:setTile(x, y, map:noneTile())
								count = count + 1
							end
						end
					end
				end
			end
		end
	end
end
if count > 0 then
	print('removed ' .. count .. ' plants')
end
