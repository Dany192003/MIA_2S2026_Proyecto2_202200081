#ifndef REPORT_GRAPHVIZ_H
#define REPORT_GRAPHVIZ_H

#include <string>
#include "../../structures/mbr.h"

namespace Reports {
    bool ReportMBR(const MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg);
    bool ReportDISK(const MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg);
    bool ReportSB(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportINODE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportBLOCK(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportTREE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportLS(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, const std::string& dirPath, std::string& errMsg);


}

#endif