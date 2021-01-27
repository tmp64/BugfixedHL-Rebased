#ifndef CCVARTEXTENTRY_H
#define CCVARTEXTENTRY_H
#include <vgui_controls/TextEntry.h>

typedef struct cvar_s cvar_t;

class CCvarTextEntry : public vgui2::TextEntry
{
	DECLARE_CLASS_SIMPLE(CCvarTextEntry, vgui2::TextEntry);
public:
	enum class CvarType
	{
		String, Float, Int
	};

	CCvarTextEntry(vgui2::Panel *parent, const char *panelName, const char *cvarName, CvarType type = CvarType::String);
	void ResetData();
	void ApplyChanges();
	void SetValue(int value);
	void SetValue(float value);
	float GetFloat();
	int GetInt();

private:
	cvar_t *m_pCvar = nullptr;
	CvarType m_nType = CvarType::String;

	virtual void FireActionSignal();
};

#endif
