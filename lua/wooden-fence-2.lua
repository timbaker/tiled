sel = map:tileSelection()
if sel:isEmpty() then
    error 'the tile selection is empty'
end
bounds = sel:boundingRect()

layer = map:tileLayer('0_Furniture')
if not layer then
    error 'layer 0_Furniture not found'
end

dofile(scriptDirectory..'/dashed-line.lua')

if bounds:width() > bounds:height() then
    tile = map:tile('fencing_01_32')
    if not tile then
	error 'tile fencing_01_32 not found'
    end
    horizontal_dashes(bounds:x(),bounds:y(),bounds:width(),layer,tile,1,1)
    tile = map:tile('fencing_01_33')
    if not tile then
	error 'tile fencing_01_33 not found'
    end
    horizontal_dashes(bounds:x()+1,bounds:y(),bounds:width()-1,layer,tile,1,1)
else
    tile = map:tile('fencing_01_35')
    if not tile then
	error 'tile fencing_01_35 not found'
    end
    vertical_dashes(bounds:x(),bounds:y(),bounds:height(),layer,tile,1,1)
    tile = map:tile('fencing_01_34')
    if not tile then
	error 'tile fencing_01_34 not found'
    end
    vertical_dashes(bounds:x(),bounds:y()+1,bounds:height()-1,layer,tile,1,1)
end

