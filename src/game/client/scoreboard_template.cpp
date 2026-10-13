#include <cstdio>
#include <cstring>
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

	struct BuiltinName
	{
		const char *pszName;
		E_Builtin builtin;
	};

	constexpr BuiltinName BUILTIN_NAMES[] = {
		{ "place", E_Builtin::Place },
		{ "total", E_Builtin::Total },
		{ "state", E_Builtin::State },
		{ "rival", E_Builtin::Rival },
		{ "gap", E_Builtin::Gap },
		{ "pass", E_Builtin::Pass },
		{ "lead", E_Builtin::Lead },
		{ "behind", E_Builtin::Behind },
		{ "score", E_Builtin::Score },
		{ "frags", E_Builtin::Frags },
		{ "deaths", E_Builtin::Deaths },
		{ "name", E_Builtin::Name },
		{ "value", E_Builtin::Value },
		{ "item", E_Builtin::Item },
		{ "index", E_Builtin::Index },
		{ "a", E_Builtin::A },
		{ "b", E_Builtin::B },
		{ "c", E_Builtin::C },
		{ "d", E_Builtin::D },
		{ "icon", E_Builtin::Icon },
	};

	E_Builtin FindBuiltin(const char *name)
	{
		for (const BuiltinName &item : BUILTIN_NAMES)
		{
			if (!strcmp(item.pszName, name))
				return item.builtin;
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

	bool IsAsciiAlpha(unsigned int c)
	{
		return IsAsciiLower(c) || (c >= 'A' && c <= 'Z');
	}

	bool IsKeyChar(unsigned int c)
	{
		return IsAsciiLower(c) || IsAsciiDigit(c) || c == '_';
	}

	bool IsHighSurrogate(wchar_t c)
	{
		return WIDE_IS_UTF16 && c >= 0xD800 && c <= 0xDBFF;
	}

	bool IsLowSurrogate(wchar_t c)
	{
		return WIDE_IS_UTF16 && c >= 0xDC00 && c <= 0xDFFF;
	}

	//! Number of wchar_t in the code point at text[0]
	size_t GetCodePointSize(const wchar_t *text, size_t len)
	{
		return len >= 2 && IsHighSurrogate(text[0]) && IsLowSurrogate(text[1]) ? 2 : 1;
	}

	//! Number of UTF-16 units of the code point at text[0] that takes size wchar_t
	int GetUtf16Units(const wchar_t *text, size_t size)
	{
		if constexpr (WIDE_IS_UTF16)
			return static_cast<int>(size);

		return static_cast<unsigned int>(text[0]) > 0xFFFF ? 2 : 1;
	}

	//! wcslen that doesn't depend on the C library's idea of wchar_t
	size_t GetWideLength(const wchar_t *text, size_t maxLen)
	{
		size_t len = 0;

		while (len < maxLen && text[len])
			len++;

		return len;
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

	//! Encodes wide text as UTF-8 for messages
	std::string WideToUtf8(const std::wstring &text)
	{
		std::string out;

		for (size_t i = 0; i < text.size(); i++)
		{
			unsigned int cp = static_cast<unsigned int>(text[i]);

			if (IsHighSurrogate(text[i]) && i + 1 < text.size() && IsLowSurrogate(text[i + 1]))
			{
				cp = 0x10000 + ((cp - 0xD800) << 10) + (static_cast<unsigned int>(text[i + 1]) - 0xDC00);
				i++;
			}
			else if (cp >= 0xD800 && cp <= 0xDFFF)
			{
				cp = REPLACEMENT_CHAR;
			}

			if (cp > 0x10FFFF)
				cp = REPLACEMENT_CHAR;

			if (cp < 0x80)
			{
				out.push_back(static_cast<char>(cp));
			}
			else if (cp < 0x800)
			{
				out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
				out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
			}
			else if (cp < 0x10000)
			{
				out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
				out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
			}
			else
			{
				out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
				out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
			}
		}

		return out;
	}

	//! Copies ASCII text to a wide buffer
	void AsciiToWide(const char *text, wchar_t *buf, int bufLen)
	{
		if (bufLen <= 0)
			return;

		int i = 0;

		for (; text[i] && i < bufLen - 1; i++)
			buf[i] = static_cast<wchar_t>(text[i]);

		buf[i] = L'\0';
	}

	long long FloorDiv(long long a, long long b)
	{
		return a >= 0 ? a / b : -((-a + b - 1) / b);
	}

	long long CeilDiv(long long a, long long b)
	{
		return a >= 0 ? (a + b - 1) / b : -(-a / b);
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
				int units = GetUtf16Units(text + pos, size);

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

		void Append(const std::wstring &text)
		{
			Append(text.data(), text.size());
		}

		void Append(wchar_t c)
		{
			Append(&c, 1);
		}

		void AppendAscii(const char *text)
		{
			for (; *text; text++)
				Append(static_cast<wchar_t>(*text));
		}

		void AppendNumber(int n)
		{
			char buf[16];
			snprintf(buf, sizeof(buf), "%d", n);
			AppendAscii(buf);
		}

		bool IsTruncated() const
		{
			return m_bTruncated;
		}

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

	//! Writes a value without a text format (8.10, "Values are written as")
	void WritePlainValue(const TemplateValue &value, CTextWriter &out)
	{
		switch (value.type)
		{
		case E_TemplateValueType::Int:
			out.AppendNumber(value.iValue);
			break;
		case E_TemplateValueType::Text:
			out.Append(value.wszText, GetWideLength(value.wszText, TEMPLATE_BUF_LEN));
			break;
		case E_TemplateValueType::Progress:
			out.AppendNumber(value.iValue);

			if (value.iMax != 0)
			{
				out.Append(L'/');
				out.AppendNumber(value.iMax);
			}

			break;
		case E_TemplateValueType::Clock:
		{
			wchar_t buf[TEMPLATE_BUF_LEN];
			FormatTime(GetClockSeconds(value.iValue, value.bClockDown), buf, TEMPLATE_BUF_LEN);
			out.Append(buf, GetWideLength(buf, TEMPLATE_BUF_LEN));
			break;
		}
		default:
			break;
		}
	}

}

//----------------------------------------------------------------------------------------------------
// Refs and values
//----------------------------------------------------------------------------------------------------
bool ParseRef(const char *text, TemplateRef &out)
{
	out = TemplateRef();

	if (!text || !text[0])
		return false;

	if (!IsValidRef(text))
	{
		out.builtin = E_Builtin::Unknown;
		return false;
	}

	const char *name = text;

	if (*name == '^')
	{
		out.bFirstRow = true;
		name++;
	}

	const char *format = strchr(name, ':');
	size_t nameLen = format ? static_cast<size_t>(format - name) : strlen(name);
	bool bBuiltin = name[0] == '@';

	if (bBuiltin)
	{
		name++;
		nameLen--;
	}

	memcpy(out.szKey, name, nameLen);
	out.szKey[nameLen] = '\0';

	if (bBuiltin)
		out.builtin = FindBuiltin(out.szKey);

	if (format)
	{
		format++;

		if (!strcmp(format, "cur"))
			out.format = E_RefFormat::Cur;
		else if (!strcmp(format, "max"))
			out.format = E_RefFormat::Max;
		else
			out.format = E_RefFormat::Left;
	}

	return true;
}

bool TemplateValue::GetNumber(int &out) const
{
	switch (type)
	{
	case E_TemplateValueType::Int:
	case E_TemplateValueType::Progress:
		out = iValue;
		return true;
	case E_TemplateValueType::Clock:
		out = GetClockSeconds(iValue, bClockDown);
		return true;
	default:
		return false;
	}
}

//----------------------------------------------------------------------------------------------------
// CTemplate
//----------------------------------------------------------------------------------------------------
bool CTemplate::Parse(const char *text, ITemplateLocalizer *pLocalizer)
{
	if (!text)
		text = "";

	if (IsClientToken(text))
	{
		const wchar_t *localized = pLocalizer ? pLocalizer->Find(text) : nullptr;

		if (localized)
			return Parse(localized);

		// A missing token is shown as its name (8.10)
		return Parse(Utf8ToWide(text + 1).c_str());
	}

	return Parse(Utf8ToWide(text).c_str());
}

bool CTemplate::Parse(const wchar_t *text)
{
	Clear();
	m_bSet = true;

	if (text)
		m_Source.assign(text, GetWideLength(text, static_cast<size_t>(-1)));

	const std::wstring &src = m_Source;
	size_t len = src.size();
	size_t pos = 0;
	std::wstring literal;

	while (pos < len)
	{
		wchar_t c = src[pos];

		if ((c == L'{' || c == L'}') && pos + 1 < len && src[pos + 1] == c)
		{
			literal.push_back(c);
			pos += 2;
		}
		else if (c == L'{')
		{
			size_t end = pos + 1;

			while (end < len && src[end] != L'}')
				end++;

			if (end == len)
			{
				// After a '{' without a closing '}' the rest is literal
				AppendWithoutColors(literal, src, pos, len);
				break;
			}

			if (ParsePlaceholder(src, pos, end))
			{
				if (!m_Error.empty())
					break;

				Segment segment;
				segment.literal.swap(literal);
				segment.iPlaceholder = static_cast<int>(m_Placeholders.size()) - 1;
				m_Segments.push_back(segment);
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

	if (!m_Error.empty())
	{
		m_Segments.clear();
		m_Placeholders.clear();
		return false;
	}

	Segment segment;
	segment.literal.swap(literal);
	m_Segments.push_back(segment);
	return true;
}

bool CTemplate::ParsePlaceholder(const std::wstring &text, size_t begin, size_t end)
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

		while (pos < end && text[pos] != L'|' && text[pos] != L'{')
			pos++;

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

	ph.ref.szKey[nameLen] = '\0';
	ph.ref.builtin = bBuiltin ? FindBuiltin(ph.ref.szKey) : E_Builtin::None;
	ph.source.assign(text, begin, end - begin + 1);

	auto fnError = [&](const char *reason) {
		m_Error = WideToUtf8(ph.source) + ": " + reason;
		return true;
	};

	if (m_Placeholders.size() >= static_cast<size_t>(MAX_PLACEHOLDERS))
		return fnError("more than 16 placeholders");

	for (size_t i = 0; i < formats.size(); i++)
	{
		const std::string &format = formats[i];

		for (size_t j = 0; j < i; j++)
		{
			if (formats[j] == format)
				return fnError("repeated format");
		}

		if (format == "cur" || format == "max" || format == "left")
		{
			if (ph.ref.format != E_RefFormat::None)
				return fnError("more than one of cur, max and left");

			ph.ref.format = format == "cur" ? E_RefFormat::Cur : (format == "max" ? E_RefFormat::Max : E_RefFormat::Left);
		}
		else if (format == "time" || format == "time2" || format == "name" || format == "team")
		{
			if (ph.textFormat != E_TextFormat::None)
				return fnError("more than one of time, time2, name, team and word forms");

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
				return fnError("more than one cut");

			int cut = 0;

			for (char digit : format)
			{
				cut = cut * 10 + (digit - '0');

				// Any longer number is out of range too
				if (cut > MAX_CUT)
					break;
			}

			if (cut < 1 || cut > MAX_CUT)
				return fnError("cut out of the range 1-255");

			ph.iCut = cut;
		}
		else
		{
			ph.bUnknownFormat = true;
		}
	}

	if (formCount > 0)
	{
		if (ph.textFormat != E_TextFormat::None)
			return fnError("more than one of time, time2, name, team and word forms");

		ph.textFormat = E_TextFormat::Forms;
		ph.iFormCount = formCount < MAX_FORMS ? formCount : MAX_FORMS;
	}

	m_Placeholders.push_back(ph);
	return true;
}

void CTemplate::Clear()
{
	m_bSet = false;
	m_Source.clear();
	m_Error.clear();
	m_Segments.clear();
	m_Placeholders.clear();
}

const wchar_t *CTemplate::GetPlaceholderSource(int i) const
{
	if (i < 0 || i >= GetPlaceholderCount())
		return L"";

	return m_Placeholders[i].source.c_str();
}

const TemplateRef &CTemplate::GetPlaceholderRef(int i) const
{
	static const TemplateRef s_EmptyRef;

	if (i < 0 || i >= GetPlaceholderCount())
		return s_EmptyRef;

	return m_Placeholders[i].ref;
}

bool CTemplate::WriteValue(const Placeholder &ph, ITemplateContext &ctx, wchar_t *buf, int bufLen, bool &bTruncated)
{
	CTextWriter out(buf, bufLen);
	TemplateValue value;
	bTruncated = false;

	if (ph.bUnknownFormat || ph.ref.builtin == E_Builtin::Unknown || !ctx.GetValue(ph.ref, value))
		return false;

	if (value.type == E_TemplateValueType::Missing)
		return false;

	int number = 0;
	bool bIsNumber = value.GetNumber(number);
	wchar_t text[TEMPLATE_BUF_LEN];
	text[0] = L'\0';

	switch (ph.textFormat)
	{
	case E_TextFormat::None:
		WritePlainValue(value, out);
		break;
	case E_TextFormat::Forms:
		if (!bIsNumber)
			return false;

		WriteForm(ph.forms[GetWordForm(number, ph.iFormCount)], number, out);
		break;
	case E_TextFormat::Time:
		if (!bIsNumber)
			return false;

		FormatTime(number, text, TEMPLATE_BUF_LEN);
		out.Append(text, GetWideLength(text, TEMPLATE_BUF_LEN));
		break;
	case E_TextFormat::Time2:
		if (value.type == E_TemplateValueType::Int || value.type == E_TemplateValueType::Progress)
			FormatTime2(value.iValue, false, text, TEMPLATE_BUF_LEN);
		else if (value.type == E_TemplateValueType::Clock)
			FormatTime2(value.iValue, value.bClockDown, text, TEMPLATE_BUF_LEN);
		else
			return false;

		out.Append(text, GetWideLength(text, TEMPLATE_BUF_LEN));
		break;
	case E_TextFormat::Name:
		if (!bIsNumber || !ctx.GetPlayerName(number, text, TEMPLATE_BUF_LEN))
			return false;

		out.Append(text, GetWideLength(text, TEMPLATE_BUF_LEN));
		break;
	case E_TextFormat::Team:
		if (!bIsNumber || !ctx.GetTeamName(number, text, TEMPLATE_BUF_LEN))
			return false;

		out.Append(text, GetWideLength(text, TEMPLATE_BUF_LEN));
		break;
	}

	bTruncated = out.IsTruncated();
	return true;
}

bool CTemplate::Evaluate(ITemplateContext &ctx, wchar_t *buf, int bufLen, unsigned int *pMissing) const
{
	if (pMissing)
		*pMissing = 0;

	if (!buf || bufLen <= 0)
		return false;

	buf[0] = L'\0';

	if (!m_bSet || HasError())
		return false;

	CTextWriter out(buf, bufLen);
	bool bMissing = false;

	for (const Segment &segment : m_Segments)
	{
		out.Append(segment.literal);

		if (segment.iPlaceholder < 0)
			continue;

		const Placeholder &ph = m_Placeholders[segment.iPlaceholder];
		wchar_t text[TEMPLATE_BUF_LEN];
		bool bTruncated = false;

		if (!WriteValue(ph, ctx, text, TEMPLATE_BUF_LEN, bTruncated))
		{
			if (pMissing)
				*pMissing |= 1u << segment.iPlaceholder;

			if (!ph.bOptional)
			{
				bMissing = true;

				if (!pMissing)
					break;
			}

			continue;
		}

		size_t len = GetWideLength(text, TEMPLATE_BUF_LEN);

		// Only the TABs of the template separate the columns of a Board row (8.14)
		for (size_t i = 0; i < len; i++)
		{
			if (text[i] == L'\t')
				text[i] = L' ';
		}

		if (ph.iCut > 0)
		{
			size_t size = GetCodePointsSize(text, len, ph.iCut);
			bool bCut = bTruncated || size < len;
			out.Append(text, size);

			if (bCut)
				out.Append(ELLIPSIS);
		}
		else
		{
			out.Append(text, len);
		}
	}

	if (bMissing)
	{
		buf[0] = L'\0';
		return false;
	}

	// Line breaks are drawn as spaces (8.6)
	for (wchar_t *c = buf; *c; c++)
	{
		if (*c == L'\r' || *c == L'\n')
			*c = L' ';
	}

	return buf[0] != L'\0';
}

//----------------------------------------------------------------------------------------------------
// Helpers
//----------------------------------------------------------------------------------------------------
bool IsClientToken(const char *str)
{
	if (!str || str[0] != '#')
		return false;

	unsigned char first = static_cast<unsigned char>(str[1]);

	if (!IsAsciiAlpha(first) && first != '_')
		return false;

	for (const char *c = str + 2; *c; c++)
	{
		unsigned char ch = static_cast<unsigned char>(*c);

		if (!IsAsciiAlpha(ch) && !IsAsciiDigit(ch) && ch != '_')
			return false;
	}

	return true;
}

int GetClockSeconds(int ms, bool bDown)
{
	return static_cast<int>(bDown ? CeilDiv(ms, 1000) : FloorDiv(ms, 1000));
}

void FormatTime(int seconds, wchar_t *buf, int bufLen)
{
	if (seconds < 0)
		seconds = 0;

	char text[32];
	int hours = seconds / 3600;

	if (hours > 0)
		snprintf(text, sizeof(text), "%d:%02d:%02d", hours, seconds / 60 % 60, seconds % 60);
	else
		snprintf(text, sizeof(text), "%d:%02d", seconds / 60, seconds % 60);

	AsciiToWide(text, buf, bufLen);
}

void FormatTime2(int ms, bool bDown, wchar_t *buf, int bufLen)
{
	long long hundredths = bDown ? CeilDiv(ms, 10) : FloorDiv(ms, 10);

	if (hundredths < 0)
		hundredths = 0;

	long long seconds = hundredths / 100;
	int cc = static_cast<int>(hundredths % 100);
	int hours = static_cast<int>(seconds / 3600);
	char text[48];

	if (hours > 0)
		snprintf(text, sizeof(text), "%d:%02d:%02d.%02d", hours, static_cast<int>(seconds / 60 % 60), static_cast<int>(seconds % 60), cc);
	else
		snprintf(text, sizeof(text), "%d:%02d.%02d", static_cast<int>(seconds / 60), static_cast<int>(seconds % 60), cc);

	AsciiToWide(text, buf, bufLen);
}

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
