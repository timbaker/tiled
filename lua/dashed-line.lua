function horizontal_dashes(x,y,width,layer,tile,length,gap)
    for i=0,width-1,length+gap do
	local j = 0
	while i + j < width and j < length do
	    layer:setTile(x+i+j,y,tile)
	    j = j + 1
	end
    end
end

function vertical_dashes(x,y,height,layer,tile,length,gap)
    for i=0,height-1,length+gap do
	local j = 0
	while i + j < height and j < length do
	    layer:setTile(x,y+i+j,tile)
	    j = j + 1
	end
    end
end

