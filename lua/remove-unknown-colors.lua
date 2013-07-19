colorToRule = {}
for _,rule in ipairs(map:rules()) do
    if not colorToRule[rule:color().pixel] then
	colorToRule[rule:color().pixel] = {}
	print('colorToRule['..rule:color().r..','..rule:color().g..','..rule:color().b..']='..rule:label())
    end
    table.insert(colorToRule[rule:color().pixel], rule)
end

function removeUnknownColors(x1,y1,x2,y2)
    local black = rgb(0,0,0)
    local blackPixel = black.pixel
    local bmp0 = map:bmp(0)
    local bmp1 = map:bmp(1)
    for y=y1,y2 do
	for x=x1,x2 do
	    local col0 = bmp0:pixel(x,y)
	    local col1 = bmp1:pixel(x,y)
	    if col0 ~= blackPixel and not colorToRule[col0] then
		bmp0:setPixel(x,y,black)
	    end
	    if col1 ~= blackPixel and not colorToRule[col1] then
		bmp1:setPixel(x,y,black)
	    end
	end
    end
end

removeUnknownColors(0,0,map:width()-1,map:height()-1)
