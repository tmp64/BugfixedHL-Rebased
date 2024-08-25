#include <cassert>
#include <cstring>
#include "CGameVersion.h"

static int binary_comparison(int x, int y)
{
	if (x == y)
		return 0;
	if (x > y)
		return 1;
	return -1;
}

CGameVersion::CGameVersion()
{
	memset(&m_SemVer, 0, sizeof(m_SemVer));
}

CGameVersion::CGameVersion(const IGameVersion *copy)
    : CGameVersion()
{
	CopyFrom(copy);
}

CGameVersion::CGameVersion(const CGameVersion &copy)
    : CGameVersion(&copy)
{
}

CGameVersion &CGameVersion::operator=(const CGameVersion &rhs)
{
	if (this != &rhs)
	{
		CopyFrom(&rhs);
	}
	return *this;
}

CGameVersion::CGameVersion(const char *pszVersion)
    : CGameVersion()
{
	TryParse(pszVersion);
}

CGameVersion::~CGameVersion()
{
	semver_free(&m_SemVer);
}

bool CGameVersion::TryParse(const char *pszVersion)
{
	m_bIsValid = false;

	// Parse with semver.c
	semver_free(&m_SemVer);
	if (semver_parse(pszVersion, &m_SemVer))
		return false;
	m_bIsValid = true;

	// Clear metadata
	m_Branch.clear();
	m_CommitHash.clear();
	m_bIsDirty = false;

	// Prase metadata
	if (m_SemVer.metadata)
	{
		std::string metadata(m_SemVer.metadata);
		size_t branchDot = metadata.find('.');
		if (branchDot != std::string::npos)
		{
			// Get branch
			m_Branch = metadata.substr(0, branchDot);

			// Get commit hash
			size_t hashDot = metadata.find('.', branchDot + 1);
			size_t hashLen = hashDot;
			if (hashLen != std::string::npos)
			{
				hashLen = hashLen - branchDot - 1;
			}

			m_CommitHash = metadata.substr(branchDot + 1, hashLen);

			// Check branch for valid chars
			bool hashValid = !m_CommitHash.empty();
			for (char c : m_CommitHash)
			{
				if (!(
				        (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
				{
					hashValid = false;
					break;
				}
			}

			if (!hashValid)
			{
				m_CommitHash.clear();
			}
			else
			{
				m_bIsDirty = hashDot != std::string::npos && hashDot == metadata.size() - 2 && metadata[metadata.size() - 1] == 'm';
			}
		}
	}

	return true;
}

//-------------------------------------------------------------------
// IGameVersion overrides
//-------------------------------------------------------------------
void CGameVersion::DeleteThis()
{
	delete this;
}

bool CGameVersion::IsValid() const
{
	return m_bIsValid;
}

int CGameVersion::ToInt() const
{
	assert(IsValid());
	return semver_numeric(const_cast<semver_t *>(&m_SemVer));
}

void CGameVersion::GetVersion(int &major, int &minor, int &patch) const
{
	assert(IsValid());
	major = GetMajor();
	minor = GetMinor();
	patch = GetPatch();
}

int CGameVersion::GetMajor() const
{
	assert(IsValid());
	return m_SemVer.major;
}

int CGameVersion::GetMinor() const
{
	assert(IsValid());
	return m_SemVer.minor;
}

int CGameVersion::GetPatch() const
{
	assert(IsValid());
	return m_SemVer.patch;
}

bool CGameVersion::GetTag(char *buf, int size) const
{
	assert(IsValid());

	if (!m_SemVer.prerelease)
		return false;

	strncpy(buf, m_SemVer.prerelease, size);
	buf[size - 1] = '\0';
	return true;
}

bool CGameVersion::GetBuildMetadata(char *buf, int size) const
{
	assert(IsValid());

	if (!m_SemVer.metadata)
		return false;

	strncpy(buf, m_SemVer.metadata, size);
	buf[size - 1] = '\0';
	return true;
}

bool CGameVersion::GetBranch(char *buf, int size) const
{
	assert(IsValid());

	if (m_Branch.empty())
		return false;

	strncpy(buf, m_Branch.c_str(), size);
	buf[size - 1] = '\0';
	return true;
}

bool CGameVersion::GetCommitHash(char *buf, int size) const
{
	assert(IsValid());

	if (m_CommitHash.empty())
		return false;

	strncpy(buf, m_CommitHash.c_str(), size);
	buf[size - 1] = '\0';
	return true;
}

bool CGameVersion::IsDirtyBuild() const
{
	assert(IsValid());
	return m_bIsDirty;
}

int CGameVersion::Compare(const CGameVersion &rhs) const
{
	int c = 0;
	bool xyzIdentical = false;

	// Compare version as in semver specs
	{
		c = binary_comparison(m_SemVer.major, rhs.m_SemVer.major);

		if (c == 0)
		{
			c = binary_comparison(m_SemVer.minor, rhs.m_SemVer.minor);
			if (c == 0)
			{
				c = binary_comparison(m_SemVer.patch, rhs.m_SemVer.patch);
				if (c == 0)
				{
					xyzIdentical = true;

					if (m_SemVer.prerelease && !rhs.m_SemVer.prerelease)
					{
						c = -1;
					}
					else if (!m_SemVer.prerelease && rhs.m_SemVer.prerelease)
					{
						c = 1;
					}
					else if (m_SemVer.prerelease && rhs.m_SemVer.prerelease)
					{
						c = strcmp(m_SemVer.prerelease, rhs.m_SemVer.prerelease);

						// strcmp is not guranteed to return -1, 0, 1
						if (c < 0)
							c = -1;
						else if (c > 0)
							c = 1;
					}
					else
					{
						c = 0;
					}
				}
			}
		}
	}

	if (c == 0)
		return 0;

	return c;
}

bool CGameVersion::operator==(const CGameVersion &rhs) const
{
	return Compare(rhs) == 0;
}

bool CGameVersion::operator!=(const CGameVersion &rhs) const
{
	return Compare(rhs) != 0;
}

bool CGameVersion::operator>(const CGameVersion &rhs) const
{
	return Compare(rhs) == 1;
}

bool CGameVersion::operator<(const CGameVersion &rhs) const
{
	return Compare(rhs) == -1;
}

bool CGameVersion::operator>=(const CGameVersion &rhs) const
{
	return Compare(rhs) != -1;
}

bool CGameVersion::operator<=(const CGameVersion &rhs) const
{
	return Compare(rhs) != 1;
}

void CGameVersion::CopyFrom(const IGameVersion *copy)
{
	// Clear
	semver_free(&m_SemVer);
	m_bIsValid = false;
	m_Branch.clear();
	m_CommitHash.clear();
	m_bIsDirty = false;

	if (!copy->IsValid())
		return;

	// Copy m_SemVer
	auto fnCopyCStr = [](const char *from, char *&to) {
		if (!from)
			return;
		if (to)
		{
			free(to);
			to = nullptr;
		}
		size_t len = strlen(from);
		to = (char *)malloc(len + 1);
		memcpy(to, from, len + 1);
	};

	m_SemVer.major = copy->GetMajor();
	m_SemVer.minor = copy->GetMinor();
	m_SemVer.patch = copy->GetPatch();

	char buf[128];
	if (copy->GetBuildMetadata(buf, sizeof(buf)))
		fnCopyCStr(buf, m_SemVer.metadata);
	if (copy->GetTag(buf, sizeof(buf)))
		fnCopyCStr(buf, m_SemVer.prerelease);

	// Copy other
	m_bIsValid = copy->IsValid();
	if (copy->GetBranch(buf, sizeof(buf)))
		m_Branch = buf;
	if (copy->GetCommitHash(buf, sizeof(buf)))
		m_CommitHash = buf;
	m_bIsDirty = copy->IsDirtyBuild();
}
