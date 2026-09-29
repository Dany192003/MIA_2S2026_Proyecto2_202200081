#include "Report.h"
#include "../../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

namespace Reports {

    // ============================================================
    // BM_INODE (TXT)
    // ============================================================
    bool ReportBMInode(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg) {
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            errMsg = "Error al abrir el disco";
            return false;
        }
        
        std::stringstream ss;
        ss << "========================================\n";
        ss << "     REPORTE BITMAP DE INODOS\n";
        ss << "========================================\n\n";
        ss << "Inodos totales: " << sb.s_inodes_count << "\n";
        ss << "Inodos libres: " << sb.s_free_inodes_count << "\n\n";
        ss << "Bitmap (20 bits por línea):\n\n";
        
        int bmSize = (sb.s_inodes_count + 7) / 8;
        disk.seekg(sb.s_bm_inode_start, std::ios::beg);
        
        int count = 0;
        for (int i = 0; i < bmSize; i++) {
            char byte;
            disk.read(&byte, 1);
            for (int bit = 7; bit >= 0; bit--) {
                int index = i * 8 + (7 - bit);
                if (index >= sb.s_inodes_count) break;
                ss << ((byte & (1 << bit)) ? '1' : '0');
                count++;
                if (count % 20 == 0) ss << "\n";
            }
        }
        
        disk.close();
        return writeTextReport(path, ss.str(), errMsg);
    }

    // ============================================================
    // BM_BLOCK (TXT)
    // ============================================================
    bool ReportBMBlock(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg) {
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            errMsg = "Error al abrir el disco";
            return false;
        }
        
        std::stringstream ss;
        ss << "========================================\n";
        ss << "     REPORTE BITMAP DE BLOQUES\n";
        ss << "========================================\n\n";
        ss << "Bloques totales: " << sb.s_blocks_count << "\n";
        ss << "Bloques libres: " << sb.s_free_blocks_count << "\n\n";
        ss << "Bitmap (20 bits por línea):\n\n";
        
        int bmSize = (sb.s_blocks_count + 7) / 8;
        disk.seekg(sb.s_bm_block_start, std::ios::beg);
        
        int count = 0;
        for (int i = 0; i < bmSize; i++) {
            char byte;
            disk.read(&byte, 1);
            for (int bit = 7; bit >= 0; bit--) {
                int index = i * 8 + (7 - bit);
                if (index >= sb.s_blocks_count) break;
                ss << ((byte & (1 << bit)) ? '1' : '0');
                count++;
                if (count % 20 == 0) ss << "\n";
            }
        }
        
        disk.close();
        return writeTextReport(path, ss.str(), errMsg);
    }

    // ============================================================
    // FILE (TXT)
    // ============================================================
    bool ReportFILE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, const std::string& filePath, std::string& errMsg) {
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        std::string content = Ext2Utils::readFile(diskPath, filePath, sb, mbr, partitionIndex);
        
        std::stringstream ss;
        ss << "========================================\n";
        ss << "          REPORTE FILE\n";
        ss << "========================================\n\n";
        ss << "Archivo: " << filePath << "\n\n";
        ss << "Contenido:\n";
        ss << "----------------------------------------\n";
        ss << (content.empty() ? "(Archivo vacío)" : content);
        ss << "\n----------------------------------------\n";
        
        return writeTextReport(path, ss.str(), errMsg);
    }

}