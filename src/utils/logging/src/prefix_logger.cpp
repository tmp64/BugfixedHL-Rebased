#include <bhl/logging/prefix_logger.h>

CPrefixLogger::CPrefixLogger(ILogger* pBaseLogger, std::string_view prefix)
{
	m_pBaseLogger = pBaseLogger;
	m_Prefix = fmt::format("[{}] ", prefix);
}

void CPrefixLogger::LogMessage(ELogLevel logLevel, std::string_view msg)
{
	std::string prefixedMsg = m_Prefix;
	prefixedMsg.append(msg);
	m_pBaseLogger->LogMessage(logLevel, prefixedMsg);
}
