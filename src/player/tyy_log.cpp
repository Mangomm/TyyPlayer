#include "tyy_log.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

using namespace TyyPlayer;

TyyLog::TyyLog() :
    _log_path(""),
    _log_level(TYY_LOG_LEVEL_DEBUG),
    _rotating_max_size(1048576 * 30),
    _rotating_max_files(10)
{
}

TyyLog::~TyyLog()
{
	std::lock_guard<std::mutex> locker(_log_mutex);
	if (_log_file.is_open()) {
		_log_file.flush();
		_log_file.close();
	}
}

TyyLog *TyyLog::get_instance()
{
	static TyyLog log;
	return &log;
}

void TyyLog::set_log_path(std::string path)
{
	_log_path = path;
}

void TyyLog::set_log_level(short level)
{
	_log_level = level;
}

void TyyLog::set_rotating_max_size(int rotating_max_size)
{
	_rotating_max_size = rotating_max_size;
}

void TyyLog::set_rotating_max_files(int rotating_max_files)
{
	_rotating_max_files = rotating_max_files;
}

bool TyyLog::exec_log(LoggerType lt)
{
	std::lock_guard<std::mutex> locker(_log_mutex);
	std::string log_file = get_log_file(lt);
	if (!create_log_dir(log_file)) {
		return false;
	}

	if (_log_file.is_open()) {
		_log_file.flush();
		_log_file.close();
	}

	_log_file.open(log_file.c_str(), std::ios::out | std::ios::app);
	return _log_file.is_open();
}

std::string TyyLog::format_message(const std::string &format, const std::vector<std::string> &params)
{
	std::string result;
	size_t pos = 0;
	size_t param_index = 0;

	while (pos < format.size()) {
		if (format[pos] == '{') {
			size_t end_pos = format.find('}', pos + 1);
			if (end_pos != std::string::npos && param_index < params.size()) {
				result += params[param_index++];
				pos = end_pos + 1;
				continue;
			}
		}

		result += format[pos];
		pos++;
	}

	for (; param_index < params.size(); param_index++) {
		result += " ";
		result += params[param_index];
	}

	return result;
}

void TyyLog::write_log_message(TyyLogLevel level, const char *file, const char *function, int line, const std::string &message)
{
	if (level < _log_level) {
		return;
	}

	std::lock_guard<std::mutex> locker(_log_mutex);
	std::ostringstream oss;
	oss << "[" << current_time_text() << "] ";
	oss << "[" << level_name(level) << "] ";
	oss << "[t " << std::this_thread::get_id() << "] ";
	oss << "[" << (file ? file : "") << ":" << (function ? function : "") << ":" << line << "] ";
	oss << "--->>> " << message;

	std::string text = oss.str();
	std::cout << text << std::endl;
	if (_log_file.is_open()) {
		_log_file << text << std::endl;
		_log_file.flush();
	}
}

const char *TyyLog::level_name(TyyLogLevel level)
{
	switch (level) {
	case TYY_LOG_LEVEL_TRACE:
		return "trace";
	case TYY_LOG_LEVEL_DEBUG:
		return "debug";
	case TYY_LOG_LEVEL_INFO:
		return "info";
	case TYY_LOG_LEVEL_WARN:
		return "warn";
	case TYY_LOG_LEVEL_ERROR:
		return "error";
	case TYY_LOG_LEVEL_CRITICAL:
		return "critical";
	default:
		return "unknown";
	}
}

std::string TyyLog::current_time_text()
{
	std::time_t now = std::time(NULL);
	std::tm tm_value;
#ifdef _WIN32
	localtime_s(&tm_value, &now);
#else
	localtime_r(&now, &tm_value);
#endif

	std::ostringstream oss;
	oss << std::setfill('0')
		<< std::setw(4) << tm_value.tm_year + 1900 << "-"
		<< std::setw(2) << tm_value.tm_mon + 1 << "-"
		<< std::setw(2) << tm_value.tm_mday << " "
		<< std::setw(2) << tm_value.tm_hour << ":"
		<< std::setw(2) << tm_value.tm_min << ":"
		<< std::setw(2) << tm_value.tm_sec;
	return oss.str();
}

std::string TyyLog::get_log_file(LoggerType lt)
{
	std::string base_path = _log_path;
	if (!base_path.empty()) {
		char last_char = base_path[base_path.size() - 1];
		if (last_char != '/' && last_char != '\\') {
			base_path += "/";
		}
	}

	if (lt == DAILY_LOGGER) {
		return base_path + "daily-logs/daily.txt";
	}

	return base_path + "rotating-logs/rotating.txt";
}

bool TyyLog::create_log_dir(const std::string &file_path)
{
	size_t pos = 0;
	while (true) {
		pos = file_path.find_first_of("/\\", pos);
		if (pos == std::string::npos) {
			break;
		}

		std::string dir = file_path.substr(0, pos);
		if (!dir.empty() && !(dir.size() == 2 && dir[1] == ':')) {
#ifdef _WIN32
			_mkdir(dir.c_str());
#else
			mkdir(dir.c_str(), 0755);
#endif
		}

		pos++;
	}

	return true;
}
