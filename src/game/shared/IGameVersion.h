#ifndef IGAMEVERSION
#define IGAMEVERSION

/**
 * @brief Contains parsed version string
 *
 * Version string follows Semantic Versioning 2.0.0 standart.
 * Format:
 *   MAJOR.MINOR.PATCH-TAG+BRANCH.COMMIT.m
 *   Where
 *     BRANCH is Git branch title and
 *     COMMIT is short commit hash.
 *     m is set when the build tree had uncommited changes at the moment of building
 *   e.g.
 *     1.3.0+master.cc5b7c1
 *     1.4.3-dev+feature-something.a17f3da.m
 *
 * This interface is created to allow changes to CGameVersion
 * without breaking compatibility with Server API modules.
 */
class IGameVersion
{
public:
	virtual ~IGameVersion() {};

	/**
     * Destroys this instance of CGameVersion.
     * Calling normal delete is UB.
     */
	virtual void DeleteThis() = 0;

	/**
     * Returns true if the parsed version is valid.
     * If IsValid() == false, version data methods will
     * return incorrect values.
     */
	virtual bool IsValid() const = 0;

	/**
     * Converts the version to an integer for ordering and filtering.
     * @see semver_numeric
     */
	virtual int ToInt() const = 0;

	/**
     * Returns major, minor and patch.
     */
	virtual void GetVersion(int &major, int &minor, int &patch) const = 0;

	/**
     * Return major version.
     */
	virtual int GetMajor() const = 0;

	/**
     * Return minor version.
     */
	virtual int GetMinor() const = 0;

	/**
     * Return patch version.
     */
	virtual int GetPatch() const = 0;

	/**
     * Returns true and puts the tag in buffer if the version has it set.
     * False otherwise, buffer not modified.
     */
	virtual bool GetTag(char *buf, int size) const = 0;

	/**
     * Returns true and puts the metadata string (everything after +) in
     * the buffer if the version has it set.
     */
	virtual bool GetBuildMetadata(char *buf, int size) const = 0;

	/**
     * Returns true and puts the branch in buffer if the version has it set.
     * False otherwise, buffer not modified.
     */
	virtual bool GetBranch(char *buf, int size) const = 0;

	/**
     * Returns true and puts the commit hash in buffer if the version has it set.
     * False otherwise, buffer not modified.
     */
	virtual bool GetCommitHash(char *buf, int size) const = 0;

	/**
     * Returns true if dirty flag is set (build tree contained
     * uncommited changes at the moment of building).
     */
	virtual bool IsDirtyBuild() const = 0;
};

#endif
