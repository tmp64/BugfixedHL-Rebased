#ifndef CLIENTSUPPORTSFLAGS_H
#define CLIENTSUPPORTSFLAGS_H
#include <enum_utils.h>

namespace bhl
{

enum class E_ClientSupports : unsigned int
{
	None = 0,
	UnicodeMotd = (1 << 0),
	HtmlMotd = (1 << 1)
};

//! Features the client reports in the "_bhl" userinfo key (docs/scoreboard-protocol.md)
enum class E_ClientCaps : unsigned int
{
	None = 0,
	ScoreboardV1 = (1 << 0),
	GunGameHudV1 = (1 << 1)
};

}
#endif
