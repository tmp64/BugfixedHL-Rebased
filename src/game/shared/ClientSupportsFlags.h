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

}
#endif
