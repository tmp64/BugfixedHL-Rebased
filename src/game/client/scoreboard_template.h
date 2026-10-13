#ifndef SCOREBOARD_TEMPLATE_H
#define SCOREBOARD_TEMPLATE_H
#include <string>
#include <vector>
#include <ScoreboardProtocol.h>

/**
 * Templates of HUD elements (docs/scoreboard-protocol.md, 8.10).
 *
 * A pure unit: it doesn't use VGUI2, the engine or client globals, so a host test can link it alone.
 * Values, names and localized tokens come through ITemplateContext and ITemplateLocalizer.
 *
 * Text is wchar_t, like VGUI2 text: UTF-16 on Windows, UTF-32 on Linux.
 * Unlike the store API, buffer sizes here are counts of wchar_t, not bytes.
 * A buffer of TEMPLATE_BUF_LEN always holds a full result (255 UTF-16 units) on both platforms.
 */
namespace bhl::sb
{

constexpr int TEMPLATE_BUF_LEN = MAX_EVAL_LEN + 1;

//! Built-in values of refs (8.11)
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
 * Parses a ref of Element.select or Element.value (4.3).
 * An invalid ref is stored as E_Builtin::Unknown, so its value is always missing.
 * @return	false for "" (out is cleared) or an invalid ref
 */
bool ParseRef(const char *text, TemplateRef &out);

//! Type of a resolved value
enum class E_TemplateValueType
{
	Missing = 0,
	Int,
	Text, //!< Text, Icon (its name) and names: never a number
	Progress,
	Clock,
};

/**
 * A value resolved by ITemplateContext.
 */
struct TemplateValue
{
	E_TemplateValueType type = E_TemplateValueType::Missing;
	int iValue = 0; //!< Int; Progress cur; current value of a Clock in ms
	int iMax = 0; //!< Progress max
	bool bClockDown = false; //!< The Clock counts down, so its number is rounded up
	wchar_t wszText[TEMPLATE_BUF_LEN] = {}; //!< Text as it is shown: a localized token, a weapon name, a player name

	//! Number of the value (4.3): Int, Progress cur or Clock seconds. False for other types.
	bool GetNumber(int &out) const;
};

/**
 * Source of the values of a template, implemented by the client store for one subject and element.
 */
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
	 * @return	false if the value is missing (out.type is then Missing)
	 */
	virtual bool GetValue(const TemplateRef &ref, TemplateValue &out) = 0;

	/**
	 * Display name of player idx without color codes, for the name format. Never looked up as a token.
	 * @param	bufLen	Size of buf in wchar_t
	 * @return	false if the slot has no player
	 */
	virtual bool GetPlayerName(int idx, wchar_t *buf, int bufLen) = 0;

	/**
	 * Display name of team number team (numbered as in ScoreInfo), for the team format, as the
	 * scoreboard team header shows it, without color codes: localized when it starts with '#' and
	 * VGUI2 has it, else as it is.
	 * @return	false if there is no such team
	 */
	virtual bool GetTeamName(int team, wchar_t *buf, int bufLen) = 0;
};

/**
 * Looks up client tokens of templates (4.2), implemented with VGUI2 localization by the store.
 */
class ITemplateLocalizer
{
public:
	virtual ~ITemplateLocalizer() = default;

	/**
	 * @param	token	"#Token"
	 * @return	the text of the token or nullptr if it is missing
	 */
	virtual const wchar_t *Find(const char *token) = 0;
};

/**
 * A template parsed once into literals and placeholders, evaluated every frame.
 * Grammar, formats, the missing-value rule and the limits follow 8.10 and 10.2.
 */
class CTemplate
{
public:
	/**
	 * Parses a template that came from the server (UTF-8, not color-stripped).
	 * A text that is exactly a client token (IsClientToken) is looked up through pLocalizer and its
	 * text is parsed instead; that text is never looked up again. A missing token, or a null
	 * pLocalizer, gives the token name without '#' as a literal.
	 * Color codes ^0-^9 are dropped from literals and word forms.
	 * @return	false on a parse error: the template then evaluates to "" and GetError() says why
	 */
	bool Parse(const char *text, ITemplateLocalizer *pLocalizer);

	//! Parses a wide text, never looked up as a token
	bool Parse(const wchar_t *text);

	//! Removes the template: IsSet() becomes false
	void Clear();

	//! True after Parse, even with a parse error
	bool IsSet() const { return m_bSet; }

	bool HasError() const { return !m_Error.empty(); }

	//! Parse error for hud_scoreboard_dump, "" if none
	const char *GetError() const { return m_Error.c_str(); }

	//! The text that was parsed: the localized text for a token
	const wchar_t *GetSource() const { return m_Source.c_str(); }

	int GetPlaceholderCount() const { return static_cast<int>(m_Placeholders.size()); }

	//! Source text of a placeholder with its braces, for hud_server_eval
	const wchar_t *GetPlaceholderSource(int i) const;

	const TemplateRef &GetPlaceholderRef(int i) const;

	/**
	 * Evaluates the template. Line breaks become spaces, the result is cut to 255 UTF-16 units.
	 * @param	buf			Receives the text; "" if a value is missing (without '?'), on a parse error
	 *						or when the template isn't set
	 * @param	bufLen		Size of buf in wchar_t
	 * @param	pMissing	Optional: receives a bit per placeholder whose value was missing, '?' or not.
	 *						With it every placeholder is evaluated, otherwise evaluation stops at the
	 *						first missing value.
	 * @return	true if the text is not empty
	 */
	bool Evaluate(ITemplateContext &ctx, wchar_t *buf, int bufLen, unsigned int *pMissing = nullptr) const;

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
		TemplateRef ref; //!< With the format cur, max or left
		bool bOptional = false; //!< '?': a missing value is written as ""
		bool bUnknownFormat = false; //!< An unknown format makes the value always missing
		E_TextFormat textFormat = E_TextFormat::None;
		int iCut = 0; //!< Code points, 0 if not cut
		int iFormCount = 0; //!< Word forms in use, 0-3
		std::wstring forms[3]; //!< Word forms as written, with '#' and "##"
		std::wstring source;
	};

	struct Segment
	{
		std::wstring literal; //!< Text before the placeholder, braces unescaped
		int iPlaceholder = -1; //!< Index in m_Placeholders, -1 for the trailing literal
	};

	bool m_bSet = false;
	std::wstring m_Source;
	std::string m_Error;
	std::vector<Segment> m_Segments;
	std::vector<Placeholder> m_Placeholders;

	/**
	 * Parses text[begin..end], a '{' and the first '}' after it, and appends the placeholder.
	 * Sets m_Error on a parse error.
	 * @return	false if it is not a placeholder: the '{' is then a literal
	 */
	bool ParsePlaceholder(const std::wstring &text, size_t begin, size_t end);

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

/**
 * Writes milliseconds as m:ss.cc, from one hour as h:mm:ss.cc, below 0 as 0:00.00 (format time2).
 * Hundredths are rounded up while a clock counts down, down otherwise.
 */
void FormatTime2(int ms, bool bDown, wchar_t *buf, int bufLen);

/**
 * Chooses a word form (10.2) by the absolute value of n.
 * @param	formCount	Number of forms, 1-3
 * @return	Index of the form, 0-based
 */
int GetWordForm(int n, int formCount);

}

#endif
