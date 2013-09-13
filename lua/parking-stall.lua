local LayerBumper = '0_Furniture'
local LayerLines = '0_FloorOverlay5'

function west_parking_stall(x,y)
    return {
	{ LayerBumper, x, y, 'street_decoration_01_37' },
	{ LayerBumper, x, y+1, 'street_decoration_01_36' },
	{ LayerBumper, x, y+2, 'street_decoration_01_35' },
	{ LayerLines, x, y, 'street_trafficlines_01_2' },
	{ LayerLines, x+1, y, 'street_trafficlines_01_2' },
	{ LayerLines, x+2, y, 'street_trafficlines_01_2' },
	{ LayerLines, x+3, y, 'street_trafficlines_01_2' }
    }
end

function east_parking_stall(x,y)
    return {
	{ LayerBumper, x, y, 'street_decoration_01_45' },
	{ LayerBumper, x, y+1, 'street_decoration_01_44' },
	{ LayerBumper, x, y+2, 'street_decoration_01_43' },
	{ LayerLines, x-3, y, 'street_trafficlines_01_2' },
	{ LayerLines, x-2, y, 'street_trafficlines_01_2' },
	{ LayerLines, x-1, y, 'street_trafficlines_01_2' },
	{ LayerLines, x, y, 'street_trafficlines_01_2' }
    }
end

function north_parking_stall(x,y)
    return {
	{ LayerBumper, x, y, 'street_decoration_01_32' },
	{ LayerBumper, x+1, y, 'street_decoration_01_33' },
	{ LayerBumper, x+2, y, 'street_decoration_01_34' },
	{ LayerLines, x, y, 'street_trafficlines_01_0' },
	{ LayerLines, x, y+1, 'street_trafficlines_01_0' },
	{ LayerLines, x, y+2, 'street_trafficlines_01_0' },
	{ LayerLines, x, y+3, 'street_trafficlines_01_0' }
    }
end

function south_parking_stall(x,y)
    return {
	{ LayerBumper, x, y, 'street_decoration_01_40' },
	{ LayerBumper, x+1, y, 'street_decoration_01_41' },
	{ LayerBumper, x+2, y, 'street_decoration_01_42' },
	{ LayerLines, x, y-3, 'street_trafficlines_01_0' },
	{ LayerLines, x, y-2, 'street_trafficlines_01_0' },
	{ LayerLines, x, y-1, 'street_trafficlines_01_0' },
	{ LayerLines, x, y, 'street_trafficlines_01_0' }
    }
end

