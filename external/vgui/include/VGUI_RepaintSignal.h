//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI1_REPAINTSIGNAL_H
#define VGUI1_REPAINTSIGNAL_H



namespace vgui
{
class Panel;
	
class RepaintSignal
{
public:
	virtual void panelRepainted(Panel* panel)=0;
};

}


#endif
