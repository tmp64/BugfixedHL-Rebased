#include <stdexcept>
#include <filesystem>
#include <FileSystem.h>
#include "hud.h"
#include "cl_util.h"
#include "http_client.h"

CHttpClient &CHttpClient::Get()
{
	static CHttpClient instance;
	return instance;
}

CHttpClient::CHttpClient()
{
	curl_global_init(CURL_GLOBAL_DEFAULT);
	m_WorkerThread = std::thread([this]() { WorkerThreadFunc(); });
}

CHttpClient::~CHttpClient()
{
	Shutdown();
}

void CHttpClient::Shutdown()
{
	std::unique_lock<std::mutex> lock(m_Mutex);
	m_bShutdown = true;
	m_CondVar.notify_all();
	lock.unlock();
	if (m_WorkerThread.joinable())
	{
		m_WorkerThread.join();
	}
}

std::shared_ptr<CHttpClient::DownloadStatus> CHttpClient::Get(Request &req)
{
	Assert(req.m_Callback);
	std::unique_lock<std::mutex> lock(m_Mutex);
	std::shared_ptr<DownloadStatus> res = req.m_pStatus = std::make_shared<DownloadStatus>();

	m_RequestQueue.push(std::move(req));
	m_CondVar.notify_all();

	return res;
}

void CHttpClient::RunFrame()
{
	// Print log messages
	{
		std::lock_guard<std::mutex> lock(m_LogMutex);
		while (!m_LogQueue.empty())
		{
			auto &item = m_LogQueue.front();

			if (item.first.a() == 0)
				ConPrintf("%s", item.second);
			else if (item.first.a() == 1)
				gEngfuncs.Con_DPrintf("%s", item.second);
			else
				ConPrintf(item.first, "%s", item.second);

			delete[] item.second;
			m_LogQueue.pop();
		}
	}

	// Process responses
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		while (!m_ResponseQueue.empty())
		{
			Response &item = m_ResponseQueue.front();

			item.m_Callback(item);

			m_ResponseQueue.pop();
		}
	}
}

void CHttpClient::AbortCurrentDownload()
{
	m_bAbortCurrentDownload = true;
}

void CHttpClient::LogPrintf(Color color, const char *fmt, ...)
{
	char *buf = new char[1024];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buf, 1024, fmt, args);
	color[4] = 255;

	std::lock_guard<std::mutex> lock(m_LogMutex);
	m_LogQueue.push({ color, buf });

	va_end(args);
}

void CHttpClient::LogPrintf(const char *fmt, ...)
{
	char *buf = new char[1024];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buf, 1024, fmt, args);

	std::lock_guard<std::mutex> lock(m_LogMutex);
	m_LogQueue.push({ Color(0, 0, 0, 0), buf });

	va_end(args);
}

void CHttpClient::LogDPrintf(const char *fmt, ...)
{
	char *buf = new char[1024];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buf, 1024, fmt, args);

	std::lock_guard<std::mutex> lock(m_LogMutex);
	m_LogQueue.push({ Color(0, 0, 0, 1), buf });

	va_end(args);
}

void CHttpClient::WorkerThreadFunc() noexcept
{
	// Init curl
	char szCurlError[CURL_ERROR_SIZE];
	CURL *hCurl = curl_easy_init();

	curl_easy_setopt(hCurl, CURLOPT_ERRORBUFFER, szCurlError);
	curl_easy_setopt(hCurl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(hCurl, CURLOPT_WRITEFUNCTION, WriteData);
	curl_easy_setopt(hCurl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
	curl_easy_setopt(hCurl, CURLOPT_NOPROGRESS, 0);

	if (gEngfuncs.CheckParm("-bhl_no_ssl_check", nullptr))
	{
		curl_easy_setopt(hCurl, CURLOPT_SSL_VERIFYPEER, 0);
		LogPrintf(ConColor::Yellow, "HTTP Client: Warning! SSL certificate check disabled with -bhl_no_ssl_check\n");
	}

	if (gEngfuncs.CheckParm("-bhl_curl_verbose", nullptr))
		curl_easy_setopt(hCurl, CURLOPT_VERBOSE, 1);

#if PLATFORM_LINUX
	curl_easy_setopt(hCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif

	for (;;)
	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_CondVar.wait(lock, [this]() {
			return m_RequestQueue.size() > 0 || m_bShutdown;
		});

		if (m_bShutdown)
			break;

		Request req = std::move(m_RequestQueue.front());
		m_RequestQueue.pop();
		lock.unlock();

		Assert(req.m_Callback);

		// Process the request
		LogDPrintf("HTTP Client: Processing request to\n");
		LogDPrintf("HTTP Client: %s\n", req.m_URL.c_str());
		Response resp;
		resp.m_Callback = req.m_Callback;
		struct curl_slist *headers = nullptr;

		try
		{
			// Set default write function
			if (!req.m_WriteCallback)
			{
				req.m_WriteCallback = [&resp](const char *pBuf, size_t iSize) {
					auto &buf = resp.m_ResponseData;

					// Reserve additional n*BUF_CHUNK_SIZE bytes
					if (buf.capacity() - buf.size() < iSize)
						buf.reserve(buf.capacity() + (((iSize - 1) / BUF_CHUNK_SIZE) + 1) * BUF_CHUNK_SIZE);

					size_t iOldSize = buf.size();
					buf.resize(buf.size() + iSize);
					memcpy(buf.data() + iOldSize, pBuf, iSize);
					return iSize;
				};
			}

			// Add headers
			for (std::string &i : req.m_Headers)
			{
				struct curl_slist *temp = curl_slist_append(headers, i.c_str());

				if (!temp)
					throw std::runtime_error("curl_slist_append failed");

				headers = temp;
			}

			// Set options
			curl_easy_setopt(hCurl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(hCurl, CURLOPT_WRITEDATA, &req);
			curl_easy_setopt(hCurl, CURLOPT_XFERINFODATA, &req);
			curl_easy_setopt(hCurl, CURLOPT_URL, req.m_URL.c_str());
			curl_easy_setopt(hCurl, CURLOPT_FAILONERROR, 1L);

			m_bAbortCurrentDownload = false;

			// Run
			CURLcode result = curl_easy_perform(hCurl);
			if (result != CURLE_OK)
			{
				if (!req.m_LastError.empty())
					resp.m_ErrorMsg = req.m_LastError;
				else
					resp.m_ErrorMsg = szCurlError;

				LogDPrintf("HTTP Client: Request failed: %s\n", resp.m_ErrorMsg.c_str());
			}
			else
			{
				LogDPrintf("HTTP Client: Request finished.\n");
			}
		}
		catch (const std::exception &e)
		{
			LogDPrintf("HTTP Client: Request failed: %s\n", e.what());
			resp.m_ErrorMsg = e.what();
		}

		curl_slist_free_all(headers);
		headers = nullptr;

		// Add response to the queue
		lock.lock();
		m_ResponseQueue.push(std::move(resp));
		lock.unlock();
	}

	// Cleanup
	curl_easy_cleanup(hCurl);
}

size_t CHttpClient::WriteData(const char *buffer, size_t size, size_t nmemb, void *userp) noexcept
{
	Request &req = *static_cast<Request *>(userp);

	try
	{
		Assert(req.m_WriteCallback);
		return req.m_WriteCallback(buffer, nmemb);
	}
	catch (const std::exception &e)
	{
		req.m_LastError = e.what();
		return 0;
	}
}

int CHttpClient::ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	Request &req = *static_cast<Request *>(clientp);

	req.m_pStatus->iSize = dlnow;
	req.m_pStatus->iTotalSize = dltotal;

	if (Get().m_bAbortCurrentDownload || Get().m_bShutdown)
	{
		Get().m_bAbortCurrentDownload = false;
		return 1; // Abort download
	}

	return 0; // Continue download
}

CHttpClient::Request::Request(const std::string &url)
{
	SetURL(url);
}

void CHttpClient::Request::SetURL(const std::string &url)
{
	m_URL = url;
}

void CHttpClient::Request::AddHeader(const std::string &header)
{
	m_Headers.push_back(header);
}

void CHttpClient::Request::SetCallback(const ResponseCallback &cb)
{
	m_Callback = cb;
}

void CHttpClient::Request::SetWriteCallback(const WriteCallback &cb)
{
	m_WriteCallback = cb;
}

std::vector<char> &CHttpClient::Response::GetResponseData()
{
	return m_ResponseData;
}

bool CHttpClient::Response::IsSuccess()
{
	return m_ErrorMsg.empty();
}

const std::string &CHttpClient::Response::GetError()
{
	return m_ErrorMsg;
}
