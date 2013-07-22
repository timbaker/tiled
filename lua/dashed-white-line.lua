sel = map:tileSelection()
if sel:isEmpty() then
    error('the tile selection is empty')
end
bounds = map:tileSelection():boundingRect()

layer = map:tileLayer('0_FloorOverlay')
if not layer then
    error 'layer 0_FloorOverlay not found'
end

dofile(scriptDirectory..'/dashed-line.lua')

if bounds:width() > bounds:height() then
    tile = map:tile('street_trafficlines_01_6')
    if not tile then
	error 'tile street_trafficlines_01_6 not found'
    end
    horizontal_dashes(bounds:x(),bounds:y(),bounds:width(),layer,tile,2,2)
else
    tile = map:tile('street_trafficlines_01_4')
    if not tile then
	error 'tile street_trafficlines_01_4 not found'
    end
    vertical_dashes(bounds:x(),bounds:y(),bounds:height(),layer,tile,2,2)
end


