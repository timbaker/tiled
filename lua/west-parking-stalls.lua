sel = map:tileSelection()
if sel:isEmpty() then
    error('the tile selection is empty')
end
bounds = map:tileSelection():boundingRect()

dofile(scriptDirectory..'/parking-stall.lua')

for y=bounds:top(),bounds:bottom()-2,3 do
    west_parking_stall(bounds:x(),y)
end
