#ifndef SCOREBOARD_TEMPLATE_H
#define SCOREBOARD_TEMPLATE_H
#include <string>
#include <vector>
#include <ScoreboardProtocol.h>

/**
 * Templates of HUD elements (docs/scoreboard-protocol.md, 8.10).
 *
 * A pure unit: it doesn't use VGUI2, the engine or client globals, so a host test can link it alone.
 * Values and names come through ITemplateContext.
 *
 * Text is wchar_t, like VGUI2 text: UTF-16 on Windows, UTF-32 on Linux.
 * Unlike the store API, buffer sizes here are counts of wchar_t, not bytes.
 * A buffer of TEMPLATE_BUF_LEN always holds a full result (255 UTF-16 units) on both platforms.
 */
namespace bhl::sb
{

constexpr int TEMPLATE_BUF_LEN = MAX_EVAL_LEN + 1;

//! Built-in values of refs (8.11); BUILTIN_NAMES in scoreboard_template.cpp lists the names of Place to Icon in this order
enum class E_Builtin
{
	None = 0, //!< Not a built-in: the ref names a column key
	Unknown, //!< "@name" this client doesn't know, or an invalid ref: the value is always missing
	Place,
	Total,
	State,
	Rival,
	Gap,
	Pass,
	Lead,
	Behind,
	Score,
	Frags,
	Deaths,
	Name,
	Value,

	// Only in a context that has them (TemplateExtras of CScoreboardData::EvaluateTemplate), missing elsewhere
	Item, //!< List item of a Board row
	Index, //!< Number of a Board row, 1-based
	A, //!< Event args 0-3 by position
	B,
	C,
	D,
	Icon, //!< First Icon arg of an event
};

//! Format of a ref that makes a number of a Progress
enum class E_RefFormat
{
	None = 0,
	Cur,
	Max,
	Left, //!< max(max - cur, 0)
};

/**
 * A parsed ref: "[^](key|@name)", with the format cur, max or left.
 * In a placeholder the format comes from its format list, in Element.select and Element.value
 * from the ":cur", ":max" or ":left" suffix.
 */
struct TemplateRef
{
	bool bFirstRow = false; //!< "^": read for the first row (6.4) instead of the subject
	E_Builtin builtin = E_Builtin::None;
	char szKey[MAX_KEY_LEN + 1] = {}; //!< Column key; for a built-in its name without '@'
	E_RefFormat format = E_RefFormat::None;

	//! False for a cleared ref ("" in Element.select and Element.value)
	bool IsSet() const { return builtin != E_Builtin::None || szKey[0]; }
};

/**
 * Parses a ref of Element.select or Element.value (4.3). "" clears out.
 * An invalid ref is stored as E_Builtin::Unknown, so its value is always missing.
 */
void ParseRef(const char *text, TemplateRef &out);

//! A value resolved by ITemplateContext
struct TemplateValue
{
	wchar_t wszText[TEMPLATE_BUF_LEN] = {}; //!< The value written without a text format (8.10): a number, cur/max, a Clock as time, a localized text, an icon name, a player or team name
	bool bIsNumber = false; //!< Int, Progress or Clock (4.3): only they take a text format
	int iNumber = 0; //!< Int, Progress cur or Clock seconds
	int iMs = 0; //!< For time2: the Int, the Progress cur or the current value of a Clock
	bool bClockDown = false; //!< A Clock that counts down: its seconds and time2 are rounded up
};

//! Source of the values of a template, implemented by the client store for one subject and element
class ITemplateContext
{
public:
	virtual ~ITemplateContext() = default;

	/**
	 * Resolves a ref (8.10): a cell of the subject, its team or the whole game, or a built-in value;
	 * for the first row if ref.bFirstRow.
	 * ref.format is applied: cur, max and left make an Int of a Progress, other types are missing.
	 * Text cells come localized, Icon cells as their name (10.3), Clock cells with their current value.
	 * Names (@name, @rival) are display names without color codes, never looked up as tokens.
	 *
	 * "@value" is the element's own value ref, resolved at most one level: its own format first, then
	 * ref.format. When the value ref is itself "@value", with or without '^' or a format, the value
	 * is missing, so a self-reference can't recurse.
	 * @return	false if the value is missing
	 */
	virtual bool GetValue(const TemplateRef &ref, TemplateValue &out) = 0;

	/**
	 * Display name of player idx without color codes, for the name format. Never looked up as a token.
	 * @param	bufLen	Size of buf in wchar_t
	 * @return	false if the slot has no player
	 */
	virtual bool GetPlayerName(int idx, wchar_t *buf, int bufLen) = 0;

	//! Name of team number team (as in ScoreInfo) as the team header shows it, without color codes, for the team format; false if there is no such team
	virtual bool GetTeamName(int team, wchar_t *buf, int bufLen) = 0;
};

/**
 * A template parsed once into literals and placeholders, evaluated every frame.
 * Grammar, formats, the missing-value rule and the limits follow 8.10 and 10.2.
 */
class CTemplate
{
public:
	/**
	 * Parses a template that came from the server (UTF-8, not color-stripped). A client token (IsClientToken)
	 * gives its name without '#' as a literal: the caller parses the text of a token it finds instead (4.2).
	 * Color codes ^0-^9 are dropped from literals and word forms.
	 * @return	false on a parse error: the template then evaluates to ""
	 */
	bool Parse(const char *text);

	//! Parses a wide text, never looked up as a token
	bool Parse(const wchar_t *text);

	//! Removes the template: IsSet() becomes false
	void Clear() { *this = CTemplate(); }

	//! True after Parse, even with a parse error
	bool IsSet() const { return m_bSet; }

	bool HasError() const { return m_bError; }

	const TemplateRef &GetPlaceholderRef(int i) const;

	/**
	 * Evaluates the template. Line breaks become spaces, the result is cut to 255 UTF-16 units.
	 * @param	buf		Receives the text; "" if a value is missing (without '?'), on a parse error
	 *					or when the template isn't set
	 * @param	bufLen	Size of buf in wchar_t
	 * @return	true if the text is not empty
	 */
	bool Evaluate(ITemplateContext &ctx, wchar_t *buf, int bufLen) const;

private:
	enum class E_TextFormat
	{
		None = 0,
		Time,
		Time2,
		Name,
		Team,
		Forms,
	};

	struct Placeholder
	{
		std::wstring literal; //!< Text before the placeholder: braces unescaped, color codes dropped
		TemplateRef ref; //!< With the format cur, max or left
		bool bOptional = false; //!< '?': a missing value is written as ""
		bool bUnknownFormat = false; //!< An unknown format makes the value always missing
		E_TextFormat textFormat = E_TextFormat::None;
		int iCut = 0; //!< Code points, 0 if not cut
		int iFormCount = 0; //!< Word forms in use, 0-3
		std::wstring forms[3]; //!< Word forms as written, with '#' and "##"
	};

	bool m_bSet = false;
	bool m_bError = false;
	std::vector<Placeholder> m_Placeholders;
	std::wstring m_Tail; //!< Text after the last placeholder

	/**
	 * Parses text[begin..end], a '{' and the first '}' after it, and appends the placeholder, which
	 * takes literal as the text before it. Sets m_bError on a parse error.
	 * @return	false if it is not a placeholder: the '{' is then a literal
	 */
	bool ParsePlaceholder(const std::wstring &text, size_t begin, size_t end, std::wstring &literal);

	/**
	 * Writes the value of a placeholder with its formats except the cut.
	 * @param	bTruncated	Set if the text didn't fit
	 * @return	false if the value is missing
	 */
	static bool WriteValue(const Placeholder &ph, ITemplateContext &ctx, wchar_t *buf, int bufLen, bool &bTruncated);
};

//----------------------------------------------------------------------------------------------------
// Helpers shared with the store
//----------------------------------------------------------------------------------------------------

//! True if str is exactly a client token: '#' followed by [A-Za-z_][A-Za-z0-9_]* (4.2)
bool IsClientToken(const char *str);

//! Number of a Clock in whole seconds: rounded up while it counts down, down otherwise (4.3)
int GetClockSeconds(int ms, bool bDown);

//! Writes seconds as m:ss, from one hour as h:mm:ss, below 0 as 0:00 (format time)
void FormatTime(int seconds, wchar_t *buf, int bufLen);

}

#endif
