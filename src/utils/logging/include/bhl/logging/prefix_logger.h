#pragma once
#include <bhl/logging/ILogger.h>

class CPrefixLogger : public ILogger
{
public:
	CPrefixLogger(ILogger* pBaseLogger, std::string_view prefix);
	virtual void LogMessage(ELogLevel logLevel, std::string_view msg) override;

private:
	ILogger* m_pBaseLogger = nullptr;
	std::string m_Prefix;
};
