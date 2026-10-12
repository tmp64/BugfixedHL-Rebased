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

//! Features the client reports in the "_bhl" userinfo key (docs/scoreboard-protocol.md, 3.1)
enum class E_ClientCaps : unsigned int
{
	None = 0,
	ScoreboardV1 = (1 << 0), //!< Receives "BhlScore" protocol version 1
	HudV1 = (1 << 1), //!< Draws HUD elements; only set together with ScoreboardV1
	All = ScoreboardV1 | HudV1 //!< Every bit this build knows, the default value of "_bhl"
};

}
#endif
