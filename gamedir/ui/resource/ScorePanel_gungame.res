///////////////////////////////////////////////////////////
// Scoreboard layout of the "gungame" preset
//
// Loaded instead of ScorePanel.res while a server with the GunGame
// plugin sends the custom scoreboard. Keys that are not set here
// come from ScorePanel.res.
//
// Set here only keys that ScorePanel.res also has, or "gg_" keys:
// any other key would keep its GunGame value after switching back.
///////////////////////////////////////////////////////////
#base "ScorePanel.res"

"ui/resource/ScorePanel_gungame.res"
{
	"ScorePanel"
	{
		// Position and size are set in code: see gg_wide_percent and gg_top_percent
		"xpos"		"c-300"
		"wide"		"600"

		// Column widths. The player column takes the free width.
		"column_avatar"		"20"
		"column_deaths"		"50"
		"column_ping"		"50"

		// Minimum distance between the bottom edge of the score panel and the bottom edge of the screen
		"vertical_margin"	"20"

		// Width in percent of the screen width. The panel is wider if the columns need it.
		"gg_wide_percent"	"70"

		// Distance from the top edge of the screen in percent of the screen height
		"gg_top_percent"	"10"

		// Ping from this value is highlighted
		"gg_ping_high"			"100"
		"gg_ping_high_color"	"255 80 80 255"

		// Rows of this player and of players on the last level
		"gg_own_row_bgcolor"	"255 160 0 56"
		"gg_final_row_bgcolor"	"255 64 32 40"

		// Own name, level and weapon on the last level
		"gg_emphasis_color"		"255 176 0 255"

		// Marks of the first three places. Use opaque colors.
		"gg_place_first_color"		"255 160 0 255"
		"gg_place_first_textcolor"	"0 0 0 255"
		"gg_place_top_color"		"112 72 16 255"
		"gg_place_final_color"		"255 64 32 255"		// First place on the last level

		// Banner "Leader on the last level"
		"gg_banner_bgcolor"			"255 64 32 48"
		"gg_banner_accent_color"	"255 160 0 255"
		"gg_banner_padding"			"3"
		"gg_banner_accent_wide"		"2"
	}
	"ServerName"
	{
		"xpos"		"8"
		"ypos"		"4"
		"wide"		"320"
		"tall"		"18"
		"font"		"GunGameHudLarge"
	}
	"MapName"
	{
		"xpos"		"8"
		"ypos"		"23"
		"wide"		"320"
		"tall"		"12"
	}
	"PlayerCount"
	{
		"visible"		"0"
	}
	"LeaderTitle"
	{
		"fieldName"		"LeaderTitle"
		"xpos"		"0"
		"ypos"		"5"
		"wide"		"100"
		"tall"		"12"
		"visible"		"0"
		"textAlignment"		"east"
		"font"		"GunGameHudText"
		"fgcolor_override"	"255 160 0 255"
	}
	"LeaderLine"
	{
		"fieldName"		"LeaderLine"
		"xpos"		"0"
		"ypos"		"18"
		"wide"		"100"
		"tall"		"12"
		"visible"		"0"
		"textAlignment"		"east"
		"font"		"Default"
	}
	"PlayerList"
	{
		"xpos"		"8"
		"ypos"		"40"
	}
}
