"ui/resource/ScorePanel.res"
{
	"ScorePanel"
	{
		"ControlName"		"CScorePanel"
		"fieldName"		"ScorePanel"
		"xpos"		"c-276"
		"ypos"		"c-224"
		"wide"		"552"
		"tall"		"448"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"settitlebarvisible"		"0"
		"bgcolor_override" "0 0 0 64"
		
		// Column widths
		// Sum of these must equal width of PlayerList
		"column_avatar"		"26"
		"column_name"		"170"
		"column_steamid"	"88"
		"column_eff"		"60"
		"column_frags"		"60"
		"column_deaths"		"60"
		"column_ping"		"80"
		
		// Icon of muted players (without .tga)
		"muted_icon"		"ui/gfx/muted_icon32"
		
		// Minimum height of the score panel
		"min_height"		"200"
		
		// Minimum distance between vertical edges of the score panel and vertical edges of the screen
		"vertical_margin"	"30"
		
		// Distance from bottom edge of player list to bottom edge of score panel
		"bottom_padding"	"4"
	}
	"ServerName"
	{
		"ControlName"		"Label"
		"fieldName"		"ServerName"
		"xpos"		"8"
		"ypos"		"3"
		"wide"		"500"
		"tall"		"10"
		"autoResize"		"1"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"labelText"		"A Half-Life Server"
		"textAlignment"		"west"
		"dulltext"		"0"
		"brighttext"		"1"
		"font"		"Default"
		"wrap"		"0"
	}
	"MapName"
	{
		"ControlName"		"Label"
		"fieldName"		"MapName"
		"xpos"		"8"
		"ypos"		"12"
		"wide"		"192"
		"tall"		"10"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"labelText"		"Map: crossfire"
		"textAlignment"		"west"
		"dulltext"		"0"
		"brighttext"		"0"
		"wrap"		"0"
	}
	"PlayerCount"
	{
		"ControlName"		"Label"
		"fieldName"		"PlayerCount"
		"xpos"		"496"
		"ypos"		"12"
		"wide"		"48"
		"tall"		"8"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"labelText"		"2/32"
		"textAlignment"		"east"
		"dulltext"		"0"
		"brighttext"		"0"
		"wrap"		"0"
	}
	"PlayerList"
	{
		"ControlName"		"SectionedListPanel"
		"fieldName"		"PlayerList"
		"xpos"		"4"
		"ypos"		"24"
		"wide"		"544"
		"tall"		"420"
		"autoResize"		"0"
		"pinCorner"		"0"
		"visible"		"1"
		"enabled"		"1"
		"tabPosition"		"0"
		"sectiongap"	"4"
		"bgcolor_override" "0 0 0 84"
	}
}
