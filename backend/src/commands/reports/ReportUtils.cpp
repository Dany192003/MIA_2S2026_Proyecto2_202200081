#include "Report.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>

namespace Reports {

    bool createReportDirectories(const std::string& path, std::string& errMsg) {
        size_t pos = 0;
        std::string currentPath;
        
        if (path[0] == '/') {
            currentPath = "/";
            pos = 1;
        }
        
        while (pos < path.length()) {
            size_t nextSlash = path.find('/', pos);
            if (nextSlash == std::string::npos) break;
            
            std::string dir = path.substr(0, nextSlash);
            if (dir.length() > 0 && dir != "/") {
                if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
                    errMsg = "Error al crear directorio " + dir;
                    return false;
                }
            }
            pos = nextSlash + 1;
        }
        return true;
    }

    std::string trimNulls(const char* data, size_t len) {
        std::string s(data, len);
        size_t nul = s.find('\0');
        if (nul != std::string::npos) s = s.substr(0, nul);
        return s;
    }

    std::string fmt2(double v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << v;
        return oss.str();
    }

    bool writeTextReport(const std::string& path, const std::string& content, std::string& errMsg) {
        std::ofstream file(path);
        if (!file.is_open()) {
            errMsg = "Error al crear el archivo " + path;
            return false;
        }
        file << content;
        file.close();
        return true;
    }

}