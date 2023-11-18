#include <cstdlib>
#include <tier0/dbg.h>
#include "compat/FileSystemCompat.h"

class CFileSystemCompat final : public IFileSystem
{
public:
	CFileSystemCompat(IFileSystemCompat* pIface)
	{
		m_pIface = pIface;
	}

	virtual void Mount(void) override
	{
		m_pIface->Mount();
	}

	virtual void Unmount(void) override
	{
		m_pIface->Unmount();
	}

	virtual void RemoveAllSearchPaths(void) override
	{
		m_pIface->RemoveAllSearchPaths();
	}

	virtual void AddSearchPath(const char *pPath, const char *pathID) override
	{
		m_pIface->AddSearchPath(pPath, pathID);
	}

	virtual bool RemoveSearchPath(const char *pPath) override
	{
		return m_pIface->RemoveSearchPath(pPath);
	}

	virtual void RemoveFile(const char *pRelativePath, const char *pathID) override
	{
		m_pIface->RemoveFile(pRelativePath, pathID);
	}

	virtual void CreateDirHierarchy(const char *path, const char *pathID) override
	{
		m_pIface->CreateDirHierarchy(path, pathID);
	}

	virtual bool FileExists(const char *pFileName) override
	{
		return m_pIface->FileExists(pFileName);
	}

	virtual bool IsDirectory(const char *pFileName) override
	{
		return m_pIface->IsDirectory(pFileName);
	}

	virtual FileHandle_t Open(const char *pFileName, const char *pOptions, const char *pathID) override
	{
		return m_pIface->Open(pFileName, pOptions, pathID);
	}

	virtual void Close(FileHandle_t file) override
	{
		m_pIface->Close(file);
	}

	virtual void Seek(FileHandle_t file, int pos, FileSystemSeek_t seekType) override
	{
		m_pIface->Seek(file, pos, seekType);
	}

	virtual unsigned int Tell(FileHandle_t file) override
	{
		return m_pIface->Tell(file);
	}

	virtual unsigned int Size(FileHandle_t file) override
	{
		return m_pIface->Size(file);
	}

	virtual unsigned int Size(const char *pFileName) override
	{
		return m_pIface->Size(pFileName);
	}

	virtual long GetFileTime(const char *pFileName) override
	{
		return m_pIface->GetFileTime(pFileName);
	}

	virtual long GetFileModificationTime(const char *pFileName) override
	{
		return m_pIface->GetFileTime(pFileName);
	}

	virtual void FileTimeToString(char *pStrip, int maxCharsIncludingTerminator, long fileTime) override
	{
		m_pIface->FileTimeToString(pStrip, maxCharsIncludingTerminator, fileTime);
	}

	virtual bool IsOk(FileHandle_t file) override
	{
		return m_pIface->IsOk(file);
	}

	virtual void Flush(FileHandle_t file) override
	{
		m_pIface->Flush(file);
	}

	virtual bool EndOfFile(FileHandle_t file) override
	{
		return m_pIface->EndOfFile(file);
	}

	virtual int Read(void *pOutput, int size, FileHandle_t file) override
	{
		return m_pIface->Read(pOutput, size, file);
	}

	virtual int Write(void const *pInput, int size, FileHandle_t file) override
	{
		return m_pIface->Write(pInput, size, file);
	}

	virtual char *ReadLine(char *pOutput, int maxChars, FileHandle_t file) override
	{
		return m_pIface->ReadLine(pOutput, maxChars, file);
	}

	virtual int FPrintf(FileHandle_t file, char *pFormat, ...) override
	{
		Assert(!("FPrintf is not implemented"));
		std::abort();
		return 0;
	}

	virtual void *GetReadBuffer(FileHandle_t file, int *outBufferSize, bool failIfNotInCache) override
	{
		return m_pIface->GetReadBuffer(file, outBufferSize, failIfNotInCache);
	}

	virtual void ReleaseReadBuffer(FileHandle_t file, void *readBuffer) override
	{
		m_pIface->ReleaseReadBuffer(file, readBuffer);
	}

	virtual const char *FindFirst(const char *pWildCard, FileFindHandle_t *pHandle, const char *pathID) override
	{
		return m_pIface->FindFirst(pWildCard, pHandle, pathID);
	}

	virtual const char *FindNext(FileFindHandle_t handle) override
	{
		return m_pIface->FindNext(handle);
	}

	virtual bool FindIsDirectory(FileFindHandle_t handle) override
	{
		return m_pIface->FindIsDirectory(handle);
	}

	virtual void FindClose(FileFindHandle_t handle) override
	{
		m_pIface->FindClose(handle);
	}

	virtual void GetLocalCopy(const char *pFileName) override
	{
		m_pIface->GetLocalCopy(pFileName);
	}

	virtual const char *GetLocalPath(const char *pFileName, char *pLocalPath, int localPathBufferSize) override
	{
		return m_pIface->GetLocalPath(pFileName, pLocalPath, localPathBufferSize);
	}

	virtual char *ParseFile(char *pFileBytes, char *pToken, bool *pWasQuoted) override
	{
		return m_pIface->ParseFile(pFileBytes, pToken, pWasQuoted);
	}

	virtual bool FullPathToRelativePath(const char *pFullpath, char *pRelative) override
	{
		return m_pIface->FullPathToRelativePath(pFullpath, pRelative);
	}

	virtual bool GetCurrentDirectory(char *pDirectory, int maxlen) override
	{
		return m_pIface->GetCurrentDirectory(pDirectory, maxlen);
	}

	virtual void PrintOpenedFiles(void) override
	{
		m_pIface->PrintOpenedFiles();
	}

	virtual void SetWarningFunc(void (*pfnWarning)(const char *fmt, ...)) override
	{
		m_pIface->SetWarningFunc(pfnWarning);
	}

	virtual void SetWarningLevel(FileWarningLevel_t level) override
	{
		m_pIface->SetWarningLevel(level);
	}

	virtual void LogLevelLoadStarted(const char *name) override
	{
		m_pIface->LogLevelLoadStarted(name);
	}

	virtual void LogLevelLoadFinished(const char *name) override
	{
		m_pIface->LogLevelLoadFinished(name);
	}

	virtual int HintResourceNeed(const char *hintlist, int forgetEverything) override
	{
		return m_pIface->HintResourceNeed(hintlist, forgetEverything);
	}

	virtual int PauseResourcePreloading(void) override
	{
		return m_pIface->PauseResourcePreloading();
	}

	virtual int ResumeResourcePreloading(void) override
	{
		return m_pIface->ResumeResourcePreloading();
	}

	virtual int SetVBuf(FileHandle_t stream, char *buffer, int mode, long size) override
	{
		return m_pIface->SetVBuf(stream, buffer, mode, size);
	}

	virtual void GetInterfaceVersion(char *p, int maxlen) override
	{
		m_pIface->GetInterfaceVersion(p, maxlen);
	}

	virtual bool IsFileImmediatelyAvailable(const char *pFileName) override
	{
		return m_pIface->IsFileImmediatelyAvailable(pFileName);
	}

	virtual WaitForResourcesHandle_t WaitForResources(const char *resourcelist) override
	{
		return m_pIface->WaitForResources(resourcelist);
	}

	virtual bool GetWaitForResourcesProgress(WaitForResourcesHandle_t handle, float *progress, bool *complete) override
	{
		return m_pIface->GetWaitForResourcesProgress(handle, progress, complete);
	}

	virtual void CancelWaitForResources(WaitForResourcesHandle_t handle) override
	{
		m_pIface->CancelWaitForResources(handle);
	}

	virtual bool IsAppReadyForOfflinePlay(int appID) override
	{
		return m_pIface->IsAppReadyForOfflinePlay(appID);
	}

	virtual bool AddPackFile(const char *fullpath, const char *pathID) override
	{
		return m_pIface->AddPackFile(fullpath, pathID);
	}

	virtual FileHandle_t OpenFromCacheForRead(const char *pFileName, const char *pOptions, const char *pathID) override
	{
		return m_pIface->OpenFromCacheForRead(pFileName, pOptions, pathID);
	}

	virtual void AddSearchPathNoWrite(const char *pPath, const char *pathID) override
	{
		m_pIface->AddSearchPathNoWrite(pPath, pathID);
	}

private:
	IFileSystemCompat *m_pIface = nullptr;
};

IFileSystem *Compat_CreateFileSystem(IFileSystemCompat *pIface)
{
	return new CFileSystemCompat(pIface);
}
