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
	for i=1,map:layerCount() do
		if map:layerAt(i-1):name() == layerName then
			return i-1
		end
	end
	return -1
end

layers = {}
previousExistingLayer = -1
for index,layerName in ipairs(layerNames) do
	existIndex = indexOfLayer(layerName)
	if existIndex == -1 then
		if previousExistingLayer == -1 then
			previousExistingLayer = 0
		end
		newLayer = map:newTileLayer(layerName)
		map:insertLayer(previousExistingLayer + 1, newLayer)
		previousExistingLayer = previousExistingLayer + 1
	else
		previousExistingLayer = existIndex
	end
end
