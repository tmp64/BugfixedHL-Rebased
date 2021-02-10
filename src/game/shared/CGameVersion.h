#ifndef CGAMEVERSION_H
#define CGAMEVERSION_H
#include <string>
#include <semver.h>
#include "IGameVersion.h"

/**
 * Main implementation of IGameVersion.
 * See IGameVersion for details.
 * @see IGameVersion
 */
class CGameVersion : public IGameVersion
{
public:
	CGameVersion();
	CGameVersion(const IGameVersion *copy);
	CGameVersion(const CGameVersion &);
	CGameVersion &operator=(const CGameVersion &);
	CGameVersion(const char *pszVersion);
	virtual ~CGameVersion();

	/**
     * Attempts to parse pszVersion as version string.
     * Returns IsValid().
     */
	bool TryParse(const char *pszVersion);

	// IGameVersion overrides
	virtual void DeleteThis() override;
	virtual bool IsValid() const override;
	virtual int ToInt() const override;
	virtual void GetVersion(int &major, int &minor, int &patch) const override;
	virtual int GetMajor() const override;
	virtual int GetMinor() const override;
	virtual int GetPatch() const override;
	virtual bool GetTag(char *buf, int size) const override;
	virtual bool GetBuildMetadata(char *buf, int size) const override;
	virtual bool GetBranch(char *buf, int size) const override;
	virtual bool GetCommitHash(char *buf, int size) const override;
	virtual bool IsDirtyBuild() const override;

	// Comparison operators
	int Compare(const CGameVersion &rhs) const;
	bool operator==(const CGameVersion &rhs) const;
	bool operator!=(const CGameVersion &rhs) const;
	bool operator>(const CGameVersion &rhs) const;
	bool operator<(const CGameVersion &rhs) const;
	bool operator>=(const CGameVersion &rhs) const;
	bool operator<=(const CGameVersion &rhs) const;

private:
	bool m_bIsValid = false;
	semver_t m_SemVer;
	std::string m_Branch;
	std::string m_CommitHash;
	bool m_bIsDirty = false;

	void CopyFrom(const IGameVersion *copy);
};

#endif
