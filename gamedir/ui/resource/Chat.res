"ui/resource/BaseChat.res"
{
	"HudChat"
	{
		"ControlName"		"EditablePanel"
		"fieldName" 		"HudChat"
		"visible" 		"1"
		"enabled" 		"1"
		"xpos"			"10"
		"ypos"			"275"
		"ypos_hidef"		"245"
		"wide"	 		"280"
		"tall"	 		"120"
		"PaintBackgroundType"	"2"
	}

	ChatInputLine
	{
		"ControlName"		"EditablePanel"
		"fieldName" 		ChatInputLine
		"visible" 		"1"
		"enabled" 		"1"
		"xpos"			"4"
		"ypos"			"395"
		"wide"	 		"272"
		"tall"	 		"2"
		"PaintBackgroundType"	"0"
	}

	"HudChatHistory"
	{
		"ControlName"		"HudChatHistory"
		"fieldName"		"HudChatHistory"
		"xpos"			"4"
		"ypos"			"4"
		"wide"	 		"272"
		"tall"			"94"
		"wrap"			"1"
		"autoResize"		"1"
		"pinCorner"		"1"
		"visible"		"1"
		"enabled"		"1"
		"labelText"		""
		"textAlignment"		"south-west"
		"font"			"ChatFont"
		"maxchars"		"-1"
	}
}
