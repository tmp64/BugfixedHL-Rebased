#ifndef UPDATER_HTTP_CLIENT_H
#define UPDATER_HTTP_CLIENT_H
#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <winsani_in.h>
#include <curl/curl.h>
#include <winsani_out.h>
#include <Color.h>

class CHttpClient
{
public:
	static constexpr size_t BUF_CHUNK_SIZE = 16384; // 16 KiB

	class Response;

	using ResponseCallback = std::function<void(Response &response)>;
	using WriteCallback = std::function<size_t(const char *pBuf, size_t iSize)>;

	static CHttpClient &Get();

	class DownloadStatus
	{
	public:
		size_t iSize = 0;
		size_t iTotalSize = 0;
	};

	class Request
	{
	public:
		/**
		 * Constructs an empty request.
		 */
		Request() = default;

		/**
		 * Constructs a request with a URL;
		 */
		Request(const std::string &url);

		/**
		 * Sets request URL.
		 */
		void SetURL(const std::string &url);

		/**
		 * Adds an HTTP header.
		 * AddHeader("User-Agent: Something");
		 */
		void AddHeader(const std::string &header);

		/**
		 * Sets the response callback. Called in the main thread.
		 */
		void SetCallback(const ResponseCallback &cb);

		/**
		 * Sets the write callback. Called from worker thread many times to write
		 * the data into a buffer/file.
		 * If not set, writes into the response buffer.
		 */
		void SetWriteCallback(const WriteCallback &cb);

	private:
		std::list<std::string> m_Headers;
		std::string m_URL;
		ResponseCallback m_Callback;
		WriteCallback m_WriteCallback;
		std::string m_LastError;
		std::shared_ptr<DownloadStatus> m_pStatus;

		friend class CHttpClient;
	};

	class Response
	{
	public:
		/**
		 * Returns repsonse data. Only valid if write callback was not overriden;
		 */
		std::vector<char> &GetResponseData();

		/**
		 * If false, request has failed. GetResponseData() may return invalid data.
		 */
		bool IsSuccess();

		/**
		 * Returns error message.
		 */
		const std::string &GetError();

	private:
		std::vector<char> m_ResponseData;
		std::string m_ErrorMsg;
		ResponseCallback m_Callback;

		friend class CHttpClient;
	};

	CHttpClient();
	~CHttpClient();

	/**
	 * Performs a GET request.
	 * Request is invalidated by this call.
	 */
	std::shared_ptr<DownloadStatus> Get(Request &req);

	/**
	 * Processes callbacks and queues.
	 */
	void RunFrame();

	/**
	 * Download of currently downloading file will be aborted.
	 */
	void AbortCurrentDownload();

private:
	std::thread m_WorkerThread;
	std::atomic_bool m_bShutdown = false;
	std::atomic_bool m_bAbortCurrentDownload = false;
	std::condition_variable m_CondVar;
	std::queue<Request> m_RequestQueue;
	std::queue<Response> m_ResponseQueue;
	std::mutex m_Mutex;

	std::mutex m_LogMutex;
	std::queue<std::pair<Color, char *>> m_LogQueue;

	// Thread-safe logging functions
	void LogPrintf(Color color, const char *fmt, ...);
	void LogPrintf(const char *fmt, ...);
	void LogDPrintf(const char *fmt, ...);

	void WorkerThreadFunc() noexcept;
	static size_t WriteData(const char *buffer, size_t size, size_t nmemb, void *userp) noexcept;
	static int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
};

#endif
