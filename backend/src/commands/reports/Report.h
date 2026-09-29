#ifndef REPORT_H
#define REPORT_H

#include <string>
#include <vector>
#include <map>
#include "../../structures/mbr.h"
#include "../../structures/superblock.h"
#include "../../structures/inode.h"
#include "../../structures/block.h"
#include "../../structures/ebr.h"

namespace Reports {

    // ============================================================
    // REPORTES EN TEXTO
    // ============================================================
    bool ReportBMInode(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportBMBlock(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportFILE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, const std::string& filePath, std::string& errMsg);
    
    // ============================================================
    // REPORTES CON GRAPHVIZ 
    // ============================================================
    bool ReportMBR(const MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg);
    bool ReportDISK(const MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg);
    bool ReportSB(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportINODE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportBLOCK(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportTREE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg);
    bool ReportLS(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, const std::string& dirPath, std::string& errMsg);

    // ============================================================
    // UTILIDADES
    // ============================================================
    bool createReportDirectories(const std::string& path, std::string& errMsg);
    std::string trimNulls(const char* data, size_t len);
    std::string fmt2(double v);
    bool writeTextReport(const std::string& path, const std::string& content, std::string& errMsg);

}

#endif