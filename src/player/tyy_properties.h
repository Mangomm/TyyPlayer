#ifndef PROPERTIES_H
#define PROPERTIES_H

#include <stdint.h>
#include <map>
#include <string>

#if defined(TYY_PLAYER_STATIC)
#define CG_API
#elif defined(PLAYER_EXPORTS)
#define CG_API __declspec(dllexport)
#else
#define CG_API __declspec(dllimport)
#endif

namespace TyyPlayer {

class CG_API Properties
{
public:
    /**
    * @brief 构造属性对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    Properties();

    /**
    * @brief 拷贝构造属性对象
    * @author: tyy
    * @param[in] other 其他属性对象
    * @return 无
    * @note:
    */
    Properties(const Properties &other);

    /**
    * @brief 析构属性对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~Properties();

    /**
    * @brief 赋值属性对象
    * @author: tyy
    * @param[in] other 其他属性对象
    * @return 当前属性对象
    * @note:
    */
    Properties& operator=(const Properties &other);

public:
    /**
    * @brief 判断属性是否存在
    * @author: tyy
    * @param[in] key 属性键
    * @return true 存在，false 不存在
    * @note:
    */
    bool has_property(const std::string &key) const;

    /**
    * @brief 设置整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] intval 属性值
    * @return 无
    * @note:
    */
    void set_property(const char* key, int intval);

    /**
    * @brief 设置无符号整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] val 属性值
    * @return 无
    * @note:
    */
    void set_property(const char* key, uint32_t val);

    /**
    * @brief 设置无符号长整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] val 属性值
    * @return 无
    * @note:
    */
    void set_property(const char* key, uint64_t val);

    /**
    * @brief 设置字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] val 属性值
    * @return 无
    * @note:
    */
    void set_property(const char* key, const char* val);

    /**
    * @brief 设置字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] val 属性值
    * @return 无
    * @note:
    */
    void set_property(const std::string &key, const std::string &val);

    /**
    * @brief 获取字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @return 属性值
    * @note:
    */
    const char* get_property(const char* key) const;

    /**
    * @brief 获取字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    std::string get_property(const char* key, const std::string default_value) const;

    /**
    * @brief 获取字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    std::string get_property(const std::string &key, const std::string default_value) const;

    /**
    * @brief 获取字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    const char* get_property(const char* key, const char *default_value) const;

    /**
    * @brief 获取字符串属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    const char* get_property(const std::string &key, char *default_value) const;

    /**
    * @brief 获取整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    int get_property(const char* key, int default_value) const;

    /**
    * @brief 获取整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    int get_property(const std::string &key, int default_value) const;

    /**
    * @brief 获取无符号长整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    uint64_t get_property(const char* key, uint64_t default_value) const;

    /**
    * @brief 获取无符号长整型属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    uint64_t get_property(const std::string &key, uint64_t default_value) const;

    /**
    * @brief 获取布尔属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    bool get_property(const char* key, bool default_value) const;

    /**
    * @brief 获取布尔属性
    * @author: tyy
    * @param[in] key 属性键
    * @param[in] default_value 默认值
    * @return 属性值
    * @note:
    */
    bool get_property(const std::string &key, bool default_value) const;

private:
    // 属性键值表
    std::map<std::string, std::string> _properties;
};

}

#endif
