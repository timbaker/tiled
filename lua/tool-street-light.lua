params = {}
params.undoText = 'Street Light'
params.layer0 = '0_Furniture'
params.layer1 = '1_Furniture'
params.tile0 = { w = 'lighting_outdoor_01_16',
                 n = 'lighting_outdoor_01_16',
                 e = 'lighting_outdoor_01_16',
                 s = 'lighting_outdoor_01_16' }
params.tile1 = { w = 'lighting_outdoor_01_11',
                 n = 'lighting_outdoor_01_8',
                 e = 'lighting_outdoor_01_9',
                 s = 'lighting_outdoor_01_10' }
params.distance = true

dofile(scriptDirectory..'/tool-four-directions.lua')
