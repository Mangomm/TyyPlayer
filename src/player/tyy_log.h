#ifndef __TYY_LOG_H__
#define __TYY_LOG_H__

#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace fmt {

/**
* @brief 获取指针日志输出值
* @author: tyy
* @param[in] value 指针值
* @return 指针值
* @note:
*/
template<typename T>
const void *ptr(T *value)
{
    return static_cast<const void *>(value);
}

}

namespace TyyPlayer {

typedef enum LoggerType {
    DAILY_LOGGER = 0,
    RORATING_LOGGER,
    COUNT,
} LoggerType;

typedef enum TyyLogLevel {
    TYY_LOG_LEVEL_TRACE = 0,
    TYY_LOG_LEVEL_DEBUG,
    TYY_LOG_LEVEL_INFO,
    TYY_LOG_LEVEL_WARN,
    TYY_LOG_LEVEL_ERROR,
    TYY_LOG_LEVEL_CRITICAL,
} TyyLogLevel;

class TyyLog {
private:
    /**
    * @brief 构造日志对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    TyyLog();

    /**
    * @brief 析构日志对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~TyyLog();

public:
    /**
    * @brief 获取日志单例对象
    * @author: tyy
    * @param 无
    * @return 日志单例对象
    * @note:
    */
    static TyyLog *get_instance();

    /**
    * @brief 设置日志保存路径
    * @author: tyy
    * @param[in] path 日志保存路径
    * @return 无
    * @note:
    */
    void set_log_path(std::string path);

    /**
    * @brief 设置日志级别
    * @author: tyy
    * @param[in] level 日志级别
    * @return 无
    * @note:
    */
    void set_log_level(short level);

    /**
    * @brief 设置单个日志文件最大大小
    * @author: tyy
    * @param[in] rotating_max_size 单个日志文件最大大小
    * @return 无
    * @note: 当前自写日志暂未按大小切分文件，保留接口兼容旧代码。
    */
    void set_rotating_max_size(int rotating_max_size);

    /**
    * @brief 设置日志文件保留数量
    * @author: tyy
    * @param[in] rotating_max_files 日志文件保留数量
    * @return 无
    * @note: 当前自写日志暂未按数量清理文件，保留接口兼容旧代码。
    */
    void set_rotating_max_files(int rotating_max_files);

    /**
    * @brief 初始化日志模块
    * @author: tyy
    * @param[in] lt 日志类型
    * @return true 成功，false 失败
    * @note:
    */
    bool exec_log(LoggerType lt);

    /**
    * @brief 写入日志
    * @author: tyy
    * @param[in] level 日志级别
    * @param[in] file 源文件
    * @param[in] function 函数名
    * @param[in] line 行号
    * @param[in] format 日志格式
    * @return 无
    * @note:
    */
    template<typename... Args>
    void write_log(TyyLogLevel level, const char *file, const char *function, int line, const std::string &format, const Args&... args)
    {
        std::vector<std::string> params;
        collect_args(params, args...);
        write_log_message(level, file, function, line, format_message(format, params));
    }

private:
    /**
    * @brief 格式化日志内容
    * @author: tyy
    * @param[in] format 日志格式
    * @param[in] params 日志参数
    * @return 格式化后的日志内容
    * @note:
    */
    std::string format_message(const std::string &format, const std::vector<std::string> &params);

    /**
    * @brief 写入已经格式化的日志
    * @author: tyy
    * @param[in] level 日志级别
    * @param[in] file 源文件
    * @param[in] function 函数名
    * @param[in] line 行号
    * @param[in] message 日志内容
    * @return 无
    * @note:
    */
    void write_log_message(TyyLogLevel level, const char *file, const char *function, int line, const std::string &message);

    /**
    * @brief 获取日志级别名称
    * @author: tyy
    * @param[in] level 日志级别
    * @return 日志级别名称
    * @note:
    */
    const char *level_name(TyyLogLevel level);

    /**
    * @brief 获取当前时间字符串
    * @author: tyy
    * @param 无
    * @return 当前时间字符串
    * @note:
    */
    std::string current_time_text();

    /**
    * @brief 获取日志文件路径
    * @author: tyy
    * @param[in] lt 日志类型
    * @return 日志文件路径
    * @note:
    */
    std::string get_log_file(LoggerType lt);

    /**
    * @brief 创建日志目录
    * @author: tyy
    * @param[in] file_path 日志文件路径
    * @return true 成功，false 失败
    * @note:
    */
    bool create_log_dir(const std::string &file_path);

    /**
    * @brief 将参数转换成字符串
    * @author: tyy
    * @param[in] value 参数值
    * @return 参数字符串
    * @note:
    */
    template<typename T>
    std::string to_log_string(const T &value)
    {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    /**
    * @brief 收集日志参数
    * @author: tyy
    * @param[in] params 参数数组
    * @return 无
    * @note:
    */
    void collect_args(std::vector<std::string> &params)
    {
        (void)params;
    }

    /**
    * @brief 收集日志参数
    * @author: tyy
    * @param[in] params 参数数组
    * @param[in] value 参数值
    * @param[in] args 剩余参数
    * @return 无
    * @note:
    */
    template<typename T, typename... Args>
    void collect_args(std::vector<std::string> &params, const T &value, const Args&... args)
    {
        params.push_back(to_log_string(value));
        collect_args(params, args...);
    }

private:
    // 日志保存路径
    std::string _log_path;
    // 日志级别
    short _log_level;
    // 单个日志文件最大大小
    int _rotating_max_size;
    // 日志文件保留数量
    int _rotating_max_files;
    // 日志文件流
    std::ofstream _log_file;
    // 日志写入锁
    std::mutex _log_mutex;
};

#define TYYTRACE(...) TyyPlayer::TyyLog::get_instance()->write_log(TyyPlayer::TYY_LOG_LEVEL_TRACE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define TYYDEBUG(...) TyyPlayer::TyyLog::get_instance()->write_log(TyyPlayer::TYY_LOG_LEVEL_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define TYYINFO(...) TyyPlayer::TyyLog::get_instance()->write_log(TyyPlayer::TYY_LOG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define TYYWARN(...) TyyPlayer::TyyLog::get_instance()->write_log(TyyPlayer::TYY_LOG_LEVEL_WARN, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define TYYERROR(...) TyyPlayer::TyyLog::get_instance()->write_log(TyyPlayer::TYY_LOG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define TYYCRITICAL(...) TyyPlayer::TyyLog::get_instance()->write_log(TyyPlayer::TYY_LOG_LEVEL_CRITICAL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

}

#endif
