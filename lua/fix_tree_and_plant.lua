-- remove vegetation_groundcover plants that have a tree on the same square

tileset1 = map:tileset("vegetation_groundcover_01")
tileset2 = map:tileset("vegetation_trees_01")
if not tileset1 or not tileset2 then return end

count = 0
for i=1,map:layerCount() do
	layer = map:layerAt(i-1)
	tl = layer:asTileLayer()
	if tl then
		for y=0,map:height()-1 do
			for x=0,map:width()-1 do
				tile = tl:tileAt(x, y)
				if tile and (tile:tileset() == tileset1) and (tile:id() >= 18 and tile:id() <= 23) then
					for j=1,map:layerCount() do
						tl2 = map:layerAt(j-1):asTileLayer()
						if tl2 then
							tile2 = tl2:tileAt(x, y)
							if tile2 and (tile2:tileset() == tileset2) then
								print('removed id=' .. tile:id() .. ' in layer ' .. tl:name() .. ' at ' .. x .. ',' .. y)
								tl:setTile(x, y, map:noneTile())
								count = count + 1
								break
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
