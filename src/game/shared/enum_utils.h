#ifndef ENUM_UTILS_H
#define ENUM_UTILS_H

/**
 * Template functions for "enum class" bitfields.
 *
 * Enums should have the following format (notice "unsigned int" part):
 * enum class E_SomeFlags : unsigned int
 * {
 *     None = 0,
 *     Something = (1 << 0),		// or Something = 1 or Something = 0x1
 *     SomethingElse = (1 << 1),	// or Something = 2 or Something = 0x2
 *     ...
 * }
 *
 * That way there is no need to create overloads for operator |, operator &, operator |=...
 *
 * If enum is not derived from usigned int, behavior is undefined.
 *
 * Examples:
 *   if (IsEnumFlagSet(m_Flags, E_Flags::Something)) { ... }	// m_Flags is not changed
 *   SetEnumFlag(m_Flags, E_Flags::Something);					// m_Flags is modified
 *   ClearEnumFlag(m_Flags, E_Flags::Something);				// m_Flags is modified
 */

/**
 * Returns true if flag is set (like bitwise AND), false otherwise.
 */
template <class T>
inline bool IsEnumFlagSet(T bitfield, T flag)
{
	return !!(static_cast<unsigned int>(bitfield) & static_cast<unsigned int>(flag));
}

/**
 * Sets flag to 1 in a bitfield.
 */
template <class T>
inline void SetEnumFlag(T &bitfield, T flag)
{
	reinterpret_cast<unsigned int &>(bitfield) |= static_cast<unsigned int>(flag);
}

/**
 * Sets flag to 0 in a bitfield.
 */
template <class T>
inline void ClearEnumFlag(T &bitfield, T flag)
{
	reinterpret_cast<unsigned int &>(bitfield) &= ~static_cast<unsigned int>(flag);
}

#endif
