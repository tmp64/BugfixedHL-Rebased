#ifndef CONSOLE_H
#define CONSOLE_H
#include <Color.h>

namespace console
{

void Initialize();
void HudInit();
void HudPostInit();
void HudShutdown();
void HudPostShutdown();

/**
 * Returns current console color.
 */
::Color GetColor();

/**
 * Sets new console color.
 */
void SetColor(::Color c);

/**
 * Restores default console color.
 */
void ResetColor();

}

/**
 * Contains default color for ConPrintf.
 */
class ConColor
{
public:
	static const ::Color Red, Green, Yellow, Cyan;
};

/**
 * Prints a message to the console.
 */
void ConPrintf(const char *fmt, ...);

/**
 * Prints a colored message to the console.
 */
void ConPrintf(::Color color, const char *fmt, ...);

#endif
