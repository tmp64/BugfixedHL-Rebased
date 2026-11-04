#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include "scoreboard_template.h"

namespace bhl::sb
{

namespace
{

	//! VGUI2 text is UTF-16 on Windows and UTF-32 elsewhere
	constexpr bool WIDE_IS_UTF16 = sizeof(wchar_t) == 2;

	constexpr wchar_t ELLIPSIS = 0x2026;
	constexpr unsigned int REPLACEMENT_CHAR = 0xFFFD;
	constexpr int MAX_FORMS = 3;

	//! Names of the built-ins after '@', indexed by E_Builtin from Place on
	constexpr const char *BUILTIN_NAMES[] = { "place", "total", "state", "rival", "gap", "pass", "lead", "behind", "score", "frags",
		"deaths", "name", "value", "item", "index", "a", "b", "c", "d", "icon" };
	static_assert(std::size(BUILTIN_NAMES) == static_cast<size_t>(E_Builtin::Icon) - static_cast<size_t>(E_Builtin::Place) + 1, "every built-in needs a name");

	E_Builtin FindBuiltin(const char *name)
	{
		for (size_t i = 0; i < std::size(BUILTIN_NAMES); i++)
		{
			if (!strcmp(BUILTIN_NAMES[i], name))
				return static_cast<E_Builtin>(static_cast<size_t>(E_Builtin::Place) + i);
		}

		return E_Builtin::Unknown;
	}

	bool IsAsciiLower(unsigned int c)
	{
		return c >= 'a' && c <= 'z';
	}

	bool IsAsciiDigit(unsigned int c)
	{
		return c >= '0' && c <= '9';
	}

	bool IsKeyChar(unsigned int c)
	{
		return IsAsciiLower(c) || IsAsciiDigit(c) || c == '_';
	}

	//! Number of wchar_t in the code point at text[0]: 2 for a UTF-16 surrogate pair
	size_t GetCodePointSize(const wchar_t *text, size_t len)
	{
		return WIDE_IS_UTF16 && len >= 2 && text[0] >= 0xD800 && text[0] <= 0xDBFF && text[1] >= 0xDC00 && text[1] <= 0xDFFF ? 2 : 1;
	}

	//! Size in wchar_t of the first count code points of text
	size_t GetCodePointsSize(const wchar_t *text, size_t len, int count)
	{
		size_t size = 0;

		for (int i = 0; i < count && size < len; i++)
			size += GetCodePointSize(text + size, len - size);

		return size;
	}

	bool IsColorCodeAt(const std::wstring &text, size_t pos)
	{
		return text[pos] == L'^' && pos + 1 < text.size() && text[pos + 1] >= L'0' && text[pos + 1] <= L'9';
	}

	//! Appends text[begin..end) without color codes (4.2)
	void AppendWithoutColors(std::wstring &out, const std::wstring &text, size_t begin, size_t end)
	{
		for (size_t i = begin; i < end; i++)
		{
			if (IsColorCodeAt(text, i) && i + 1 < end)
				i++;
			else
				out.push_back(text[i]);
		}
	}

	void AppendCodePoint(std::wstring &out, unsigned int cp)
	{
		if (WIDE_IS_UTF16 && cp > 0xFFFF)
		{
			cp -= 0x10000;
			out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
			out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
		}
		else
		{
			out.push_back(static_cast<wchar_t>(cp));
		}
	}

	//! Decodes UTF-8; every invalid sequence becomes U+FFFD
	std::wstring Utf8ToWide(const char *text)
	{
		std::wstring out;
		const unsigned char *p = reinterpret_cast<const unsigned char *>(text);

		while (*p)
		{
			unsigned int c = *p;
			unsigned int cp = REPLACEMENT_CHAR;
			int extra = 0;
			unsigned int lower = 0x80;
			unsigned int upper = 0xBF;

			if (c < 0x80)
			{
				cp = c;
			}
			else if (c >= 0xC2 && c <= 0xDF)
			{
				cp = c & 0x1F;
				extra = 1;
			}
			else if (c >= 0xE0 && c <= 0xEF)
			{
				cp = c & 0x0F;
				extra = 2;
				lower = c == 0xE0 ? 0xA0 : 0x80;
				upper = c == 0xED ? 0x9F : 0xBF;
			}
			else if (c >= 0xF0 && c <= 0xF4)
			{
				cp = c & 0x07;
				extra = 3;
				lower = c == 0xF0 ? 0x90 : 0x80;
				upper = c == 0xF4 ? 0x8F : 0xBF;
			}

			p++;

			for (int i = 0; i < extra; i++)
			{
				unsigned int next = *p;

				// Only the first continuation byte has a narrower range
				if (next < (i == 0 ? lower : 0x80) || next > (i == 0 ? upper : 0xBF))
				{
					cp = REPLACEMENT_CHAR;
					break;
				}

				cp = (cp << 6) | (next & 0x3F);
				p++;
			}

			if (c >= 0x80 && extra == 0)
				cp = REPLACEMENT_CHAR;

			AppendCodePoint(out, cp);
		}

		return out;
	}

	//! ms in whole units, rounded up while a clock counts down and down otherwise
	int DivideClock(int ms, int unit, bool bDown)
	{
		int quotient = ms / unit;
		int remainder = ms % unit;
		return bDown ? quotient + (remainder > 0) : quotient - (remainder < 0);
	}

	/**
	 * Appends whole code points to a buffer of bufLen wchar_t, up to MAX_EVAL_LEN UTF-16 units.
	 * Text that doesn't fit is dropped, a surrogate pair is never split.
	 */
	class CTextWriter
	{
	public:
		CTextWriter(wchar_t *buf, int bufLen)
		    : m_pBuf(buf)
		    , m_iBufLen(bufLen)
		{
			m_pBuf[0] = L'\0';
		}

		void Append(const wchar_t *text, size_t len)
		{
			size_t pos = 0;

			while (pos < len && !m_bTruncated)
			{
				size_t size = GetCodePointSize(text + pos, len - pos);

				// A code point above U+FFFF takes two UTF-16 units, also where wchar_t is UTF-32
				int units = static_cast<unsigned int>(text[pos]) > 0xFFFF ? 2 : static_cast<int>(size);

				if (m_iLen + static_cast<int>(size) > m_iBufLen - 1 || m_iUnits + units > MAX_EVAL_LEN)
				{
					m_bTruncated = true;
					break;
				}

				for (size_t i = 0; i < size; i++)
					m_pBuf[m_iLen++] = text[pos + i];

				m_iUnits += units;
				pos += size;
			}

			m_pBuf[m_iLen] = L'\0';
		}

		void Append(const wchar_t *text)
		{
			Append(text, wcslen(text));
		}

		void Append(wchar_t c)
		{
			Append(&c, 1);
		}

		void AppendNumber(int n)
		{
			wchar_t buf[16];
			swprintf(buf, std::size(buf), L"%d", n);
			Append(buf);
		}

		bool IsTruncated() const { return m_bTruncated; }

	private:
		wchar_t *m_pBuf = nullptr;
		int m_iBufLen = 0;
		int m_iLen = 0;
		int m_iUnits = 0;
		bool m_bTruncated = false;
	};

	//! Writes a word form: '#' is the number, "##" a literal '#' (the signed number)
	void WriteForm(const std::wstring &form, int number, CTextWriter &out)
	{
		for (size_t i = 0; i < form.size(); i++)
		{
			if (form[i] != L'#')
			{
				out.Append(form[i]);
			}
			else if (i + 1 < form.size() && form[i + 1] == L'#')
			{
				out.Append(L'#');
				i++;
			}
			else
			{
				out.AppendNumber(number);
			}
		}
	}

	//! Writes milliseconds as m:ss.cc, from one hour as h:mm:ss.cc, below 0 as 0:00.00 (format time2)
	void FormatTime2(int ms, bool bDown, wchar_t *buf, int bufLen)
	{
		int hundredths = std::max(DivideClock(ms, 10, bDown), 0);
		wchar_t text[16];
		FormatTime(hundredths / 100, text, static_cast<int>(std::size(text)));
		swprintf(buf, bufLen, L"%ls.%02d", text, hundredths % 100);
	}

	//! Index of the word form (10.2) for the absolute value of n, with formCount forms (1-3)
	int GetWordForm(int n, int formCount)
	{
		unsigned int value = n < 0 ? 0u - static_cast<unsigned int>(n) : static_cast<unsigned int>(n);

		if (formCount <= 1)
			return 0;

		if (formCount == 2)
			return value == 1 ? 0 : 1;

		if (value % 10 == 1 && value % 100 != 11)
			return 0;

		if (value % 10 >= 2 && value % 10 <= 4 && (value % 100 < 12 || value % 100 > 14))
			return 1;

		return 2;
	}

}

//----------------------------------------------------------------------------------------------------
// Refs
//----------------------------------------------------------------------------------------------------
void ParseRef(const char *text, TemplateRef &out)
{
	out = TemplateRef();

	if (!text || !text[0])
		return;

	if (!IsValidRef(text))
	{
		out.builtin = E_Builtin::Unknown;
		return;
	}

	// A valid ref is a placeholder without its braces, '?', word forms and other formats
	std::wstring source = L"{" + std::wstring(text, text + strlen(text)) + L"}";
	CTemplate tpl;
	tpl.Parse(source.c_str());
	out = tpl.GetPlaceholderRef(0);
}

//----------------------------------------------------------------------------------------------------
// CTemplate
//----------------------------------------------------------------------------------------------------
bool CTemplate::Parse(const char *text)
{
	if (!text)
		text = "";

	// A missing token is shown as its name (8.10)
	return Parse(Utf8ToWide(IsClientToken(text) ? text + 1 : text).c_str());
}

bool CTemplate::Parse(const wchar_t *text)
{
	Clear();
	m_bSet = true;

	std::wstring src = text ? text : L"";
	size_t len = src.size();
	size_t pos = 0;
	std::wstring literal;

	while (pos < len && !HasError())
	{
		wchar_t c = src[pos];

		if ((c == L'{' || c == L'}') && pos + 1 < len && src[pos + 1] == c)
		{
			literal.push_back(c);
			pos += 2;
		}
		else if (c == L'{')
		{
			size_t end = src.find(L'}', pos + 1);

			if (end == std::wstring::npos)
			{
				// After a '{' without a closing '}' the rest is literal
				AppendWithoutColors(literal, src, pos, len);
				break;
			}

			if (ParsePlaceholder(src, pos, end, literal))
			{
				pos = end + 1;
			}
			else
			{
				literal.push_back(c);
				pos++;
			}
		}
		else if (IsColorCodeAt(src, pos))
		{
			pos += 2;
		}
		else
		{
			literal.push_back(c);
			pos++;
		}
	}

	m_Tail.swap(literal);
	return !HasError();
}

bool CTemplate::ParsePlaceholder(const std::wstring &text, size_t begin, size_t end, std::wstring &literal)
{
	Placeholder ph;
	size_t pos = begin + 1;

	if (pos < end && text[pos] == L'^')
	{
		ph.ref.bFirstRow = true;
		pos++;
	}

	bool bBuiltin = pos < end && text[pos] == L'@';

	if (bBuiltin)
		pos++;

	size_t nameBegin = pos;

	while (pos < end && (bBuiltin ? IsAsciiLower(text[pos]) : IsKeyChar(text[pos])))
		pos++;

	size_t nameLen = pos - nameBegin;

	if (nameLen < 1 || nameLen > static_cast<size_t>(bBuiltin ? MAX_BUILTIN_NAME_LEN : MAX_KEY_LEN))
		return false;

	if (pos < end && text[pos] == L'?')
	{
		ph.bOptional = true;
		pos++;
	}

	std::vector<std::string> formats;

	while (pos < end && text[pos] == L':')
	{
		std::string format;

		for (pos++; pos < end && (IsAsciiLower(text[pos]) || IsAsciiDigit(text[pos])); pos++)
			format.push_back(static_cast<char>(text[pos]));

		if (format.empty())
			return false;

		formats.push_back(format);
	}

	int formCount = 0;

	while (pos < end && text[pos] == L'|')
	{
		size_t formBegin = ++pos;
		pos = std::min(text.find_first_of(L"|{", pos), end);

		if (pos < end && text[pos] == L'{')
			return false;

		if (formCount < MAX_FORMS)
			AppendWithoutColors(ph.forms[formCount], text, formBegin, pos);

		formCount++;
	}

	if (pos != end)
		return false;

	// The syntax matches: from here on every problem is a parse error, not a literal
	for (size_t i = 0; i < nameLen; i++)
		ph.ref.szKey[i] = static_cast<char>(text[nameBegin + i]);

	ph.ref.builtin = bBuiltin ? FindBuiltin(ph.ref.szKey) : E_Builtin::None;

	auto fnError = [this]() {
		m_bError = true;
		return true;
	};

	if (m_Placeholders.size() >= static_cast<size_t>(MAX_PLACEHOLDERS))
		return fnError();

	for (size_t i = 0; i < formats.size(); i++)
	{
		const std::string &format = formats[i];

		if (std::find(formats.begin(), formats.begin() + i, format) != formats.begin() + i)
			return fnError();

		if (format == "cur" || format == "max" || format == "left")
		{
			if (ph.ref.format != E_RefFormat::None)
				return fnError();

			ph.ref.format = format == "cur" ? E_RefFormat::Cur : (format == "max" ? E_RefFormat::Max : E_RefFormat::Left);
		}
		else if (format == "time" || format == "time2" || format == "name" || format == "team")
		{
			if (ph.textFormat != E_TextFormat::None)
				return fnError();

			if (format == "time")
				ph.textFormat = E_TextFormat::Time;
			else if (format == "time2")
				ph.textFormat = E_TextFormat::Time2;
			else if (format == "name")
				ph.textFormat = E_TextFormat::Name;
			else
				ph.textFormat = E_TextFormat::Team;
		}
		else if (format.find_first_not_of("0123456789") == std::string::npos)
		{
			if (ph.iCut != 0)
				return fnError();

			long cut = strtol(format.c_str(), nullptr, 10);

			if (cut < 1 || cut > MAX_CUT)
				return fnError();

			ph.iCut = static_cast<int>(cut);
		}
		else
		{
			ph.bUnknownFormat = true;
		}
	}

	if (formCount > 0)
	{
		if (ph.textFormat != E_TextFormat::None)
			return fnError();

		ph.textFormat = E_TextFormat::Forms;
		ph.iFormCount = formCount < MAX_FORMS ? formCount : MAX_FORMS;
	}

	ph.literal.swap(literal);
	m_Placeholders.push_back(ph);
	return true;
}

const TemplateRef &CTemplate::GetPlaceholderRef(int i) const
{
	static const TemplateRef s_EmptyRef;

	if (i < 0 || i >= static_cast<int>(m_Placeholders.size()))
		return s_EmptyRef;

	return m_Placeholders[i].ref;
}

bool CTemplate::WriteValue(const Placeholder &ph, ITemplateContext &ctx, wchar_t *buf, int bufLen, bool &bTruncated)
{
	CTextWriter out(buf, bufLen);
	TemplateValue value;
	wchar_t text[TEMPLATE_BUF_LEN];
	text[0] = L'\0';
	bTruncated = false;

	// Every text format takes a number
	if (ph.bUnknownFormat || ph.ref.builtin == E_Builtin::Unknown || !ctx.GetValue(ph.ref, value) || (ph.textFormat != E_TextFormat::None && !value.bIsNumber))
		return false;

	switch (ph.textFormat)
	{
	case E_TextFormat::None:
		out.Append(value.wszText);
		break;
	case E_TextFormat::Forms:
		WriteForm(ph.forms[GetWordForm(value.iNumber, ph.iFormCount)], value.iNumber, out);
		break;
	case E_TextFormat::Time:
		FormatTime(value.iNumber, text, TEMPLATE_BUF_LEN);
		break;
	case E_TextFormat::Time2:
		FormatTime2(value.iMs, value.bClockDown, text, TEMPLATE_BUF_LEN);
		break;
	case E_TextFormat::Name:
		if (!ctx.GetPlayerName(value.iNumber, text, TEMPLATE_BUF_LEN))
			return false;

		break;
	case E_TextFormat::Team:
		if (!ctx.GetTeamName(value.iNumber, text, TEMPLATE_BUF_LEN))
			return false;

		break;
	}

	out.Append(text);
	bTruncated = out.IsTruncated();
	return true;
}

bool CTemplate::Evaluate(ITemplateContext &ctx, wchar_t *buf, int bufLen) const
{
	if (!buf || bufLen <= 0)
		return false;

	CTextWriter out(buf, bufLen);

	if (!m_bSet || HasError())
		return false;

	for (const Placeholder &ph : m_Placeholders)
	{
		wchar_t text[TEMPLATE_BUF_LEN];
		bool bTruncated = false;
		out.Append(ph.literal.data(), ph.literal.size());

		if (!WriteValue(ph, ctx, text, TEMPLATE_BUF_LEN, bTruncated))
		{
			if (ph.bOptional)
				continue;

			buf[0] = L'\0';
			return false;
		}

		size_t len = wcslen(text);
		size_t size = ph.iCut > 0 ? GetCodePointsSize(text, len, ph.iCut) : len;

		// Only the TABs of the template separate the columns of a Board row (8.14)
		std::replace(text, text + size, L'\t', L' ');
		out.Append(text, size);

		if (ph.iCut > 0 && (bTruncated || size < len))
			out.Append(ELLIPSIS);
	}

	out.Append(m_Tail.data(), m_Tail.size());

	// Line breaks are drawn as spaces (8.6)
	std::replace_if(buf, buf + wcslen(buf), [](wchar_t c) { return c == L'\r' || c == L'\n'; }, L' ');
	return buf[0] != L'\0';
}

//----------------------------------------------------------------------------------------------------
// Helpers
//----------------------------------------------------------------------------------------------------
bool IsClientToken(const char *str)
{
	if (!str || str[0] != '#' || !str[1] || IsAsciiDigit(str[1]))
		return false;

	return strspn(str + 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") == strlen(str + 1);
}

int GetClockSeconds(int ms, bool bDown)
{
	return DivideClock(ms, 1000, bDown);
}

void FormatTime(int seconds, wchar_t *buf, int bufLen)
{
	seconds = std::max(seconds, 0);

	if (seconds >= 3600)
		swprintf(buf, bufLen, L"%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else
		swprintf(buf, bufLen, L"%d:%02d", seconds / 60, seconds % 60);
}

}
