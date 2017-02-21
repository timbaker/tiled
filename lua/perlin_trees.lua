tileset1 = map:tileset("vegetation_trees_01")
tileset2 = map:tileset("blends_natural_01")
if not tileset1 or not tileset2 then return end

if map:cellX() == -1 then
	print('ERROR: cellX,cellY = -1,-1.  Add the WorldEd project to the list in the settings.')
	return
end

print('cell x,y='..map:cellX()..','..map:cellY())

perlin = Perlin:new()

local bmp0 = map:bmp(0)
local bmp1 = map:bmp(1)
local lightGrass = rgb(145,135,60)
local mediumGrass = rgb(117,117,47)
local darkGrass = rgb(90,100,35)
local tallGrass = rgb(0,255,0)
local bushTreeGrass = rgb(255,0,255)
local onlyTrees = rgb(255,0,0)
local manyGrassFewTrees = rgb(0,128,0)

function OctavePerlin(x, y, z, octaves, persistence)
	local total = 0
	local frequency = 1
	local amplitude = 1
	local maxValue = 0
	for i=1,octaves do
		total = total + perlin:perlin(x * frequency, y * frequency, z * frequency) * amplitude
		maxValue = maxValue + amplitude
		amplitude = amplitude * persistence
		frequency = frequency * 2
	end
	return total / maxValue
end

--[[
for y=0,map:height()-1 do
	for x=0,map:width()-1 do
		tile = tl:tileAt(x, y)
		if tile and (tile:tileset() == tileset1) and (tile:id() >= 8) then
			local noise = perlin:perlin(x / 20, y / 20, 0.5)
			local noise = OctavePerlin(x / 40, y / 40, 0.5, 6, 0.5)
			if noise < 0.5 then
				tl:setTile(x, y, map:noneTile())
				count = count + 1
			end
		end
	end
end
]]--

local dx = map:cellX() * 300
local dy = map:cellY() * 300
local black = rgb(0,0,0)

function doIt(x, y)
	if bmp1:pixel(x,y) == onlyTrees.pixel then
		-- tree subtraction
		local noise = OctavePerlin((dx + x) / 10, (dy + y) / 10, 0.5, 6, 0.5)
		if noise < 0.5 and noise > 0.4 then
			bmp1:setPixel(x, y, black)
		end
	end
	local pixel = bmp0:pixel(x,y)
	if pixel == darkGrass.pixel then
--					local noise = perlin:perlin((dx + x) / 40, (dy + y) / 40, 0.5)
		local noise = OctavePerlin((dx + x) / (300 * 20 / 256), (dy + y) / (300 * 20 / 256), 0.5, 6, 0.5)
		if noise < 0.35 then
			bmp0:setPixel(x, y, lightGrass)
		elseif noise < 0.45 then
			bmp0:setPixel(x, y, mediumGrass)
		end
		if bmp1:pixel(x,y) == black.pixel then
			if noise > 0.6 then
				bmp1:setPixel(x, y, bushTreeGrass)
			elseif noise > 0.5 then
				bmp1:setPixel(x, y, tallGrass)
			end
		end
	end
	if bmp1:pixel(x,y) == black.pixel then
		-- different noise for tall grass on light/medium flat grass
		noise = OctavePerlin((dx + x) / (300 * 20 / 512), (dy + y) / (300 * 20 / 512), 0.5, 6, 0.5)
		if noise < 0.35 and noise > 0.32 then
			bmp1:setPixel(x, y, tallGrass)
		elseif noise < 0.45 and noise > 0.42 then
			bmp1:setPixel(x, y, tallGrass)
		end
	end
end

local selection = map:tileSelection()
if not selection:isEmpty() then
	local rects = selection:rects()
	for _,rect in ipairs(rects) do
		for y=rect:top(),rect:bottom() do
			for x=rect:left(),rect:right() do
				doIt(x, y)
			end
		end
	end
	return
end

for y=0,map:height()-1 do
	for x=0,map:width()-1 do
		doIt(x, y)
	end
end

