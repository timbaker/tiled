layerNames = {
	'0_Floor',
	'0_FloorOverlay',
	'0_FloorOverlay2',
	'0_FloorOverlay3',
	'0_FloorOverlay4',
	'0_FloorOverlay5',
	'0_FloorOverlay6',
	'0_Vegetation',
	'0_Furniture',
	'0_Furniture2',
	'0_Curbs',

	'1_Furniture',
	'1_Furniture2',
}

function indexOfLayer(layerName)
	local level = map:levelForLayerName(layerName);
	layerName = map:layerNameWithoutPrefix(layerName);
	local mapLevel = map:levelAt(level);
	for i=1,mapLevel:layerCount() do
		if mapLevel:layerAt(i-1):name() == layerName then
			return i-1
		end
	end
	return -1
end

previousExistingLayer = {}

for index,layerName in ipairs(layerNames) do
	level = map:levelForLayerName(layerName);
	mapLevel = map:levelAt(level);
	existIndex = indexOfLayer(layerName)
	if existIndex == -1 then
		if not previousExistingLayer[level] then
			previousExistingLayer[level] = 0
		end
		newLayer = map:newTileLayer(layerName)
		map:insertLayer(previousExistingLayer[level] + 1, newLayer)
		previousExistingLayer[level] = previousExistingLayer[level] + 1
	else
		previousExistingLayer[level] = existIndex
	end
end
