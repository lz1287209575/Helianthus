#pragma once

#include <string>
#include <algorithm>
#include <cctype>
#include <vector>
#include <sstream>

namespace Helianthus::Common
{

class Utils
{
public:
    // 去除字符串前后的空白字符
    static std::string Trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    // 转换为小写
    static std::string ToLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    // 转换为大写
    static std::string ToUpper(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
    
    // 检查字符串是否为空或只包含空白字符
    static bool IsEmptyOrWhitespace(const std::string& str)
    {
        return str.empty() || str.find_first_not_of(" \t\r\n") == std::string::npos;
    }
    
    // 分割字符串
    static std::vector<std::string> Split(const std::string& str, char delimiter)
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(Trim(token));
        }
        return tokens;
    }
    
    // 连接字符串
    static std::string Join(const std::vector<std::string>& tokens, const std::string& delimiter)
    {
        if (tokens.empty()) {
            return "";
        }
        
        std::string result = tokens[0];
        for (size_t i = 1; i < tokens.size(); ++i) {
            result += delimiter + tokens[i];
        }
        return result;
    }
    
    // 检查字符串是否以指定前缀开始
    static bool StartsWith(const std::string& str, const std::string& prefix)
    {
        if (prefix.length() > str.length()) {
            return false;
        }
        return str.compare(0, prefix.length(), prefix) == 0;
    }
    
    // 检查字符串是否以指定后缀结束
    static bool EndsWith(const std::string& str, const std::string& suffix)
    {
        if (suffix.length() > str.length()) {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }
    
    // 替换字符串中的子串
    static std::string Replace(const std::string& str, const std::string& from, const std::string& to)
    {
        std::string result = str;
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
        return result;
    }
};

} // namespace Helianthus::Common
