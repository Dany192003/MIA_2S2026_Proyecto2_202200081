#include "ReportGraphviz.h"
#include "Report.h"
#include "../../structures/ebr.h"
#include "../../utils/ext2_utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <vector>
#include <map>
#include <set>         
#include <unistd.h>
#include <sys/stat.h>

namespace Reports {

    // ============================================================
    // UTILIDAD: obtener extensión
    // ============================================================
    static std::string getExt(const std::string& path) {
        std::string ext = std::filesystem::path(path).extension().string();
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        return ext;
    }

    // ============================================================
    // UTILIDAD: generar imagen y dot 
    // ============================================================
    static bool generateGraphvizWithDot(const std::string& dotContent, const std::string& outputPath, std::string& errMsg) {
        // 1. Guardar el .dot 
        std::string dotFilePath = outputPath + ".dot";
        std::ofstream dotFile(dotFilePath);
        if (!dotFile.is_open()) {
            errMsg = "Error al crear el archivo .dot: " + dotFilePath;
            return false;
        }
        dotFile << dotContent;
        dotFile.close();

        // 2. Determinar formato de salida
        std::string ext = getExt(outputPath);
        if (ext.empty()) ext = "png";

        // 3. Ejecutar Graphviz para generar la imagen
        std::string cmd = "dot -T" + ext + " \"" + dotFilePath + "\" -o \"" + outputPath + "\"";
        int ret = system(cmd.c_str());

        if (ret != 0) {
            errMsg = "Error al ejecutar Graphviz (formato: " + ext + ")";
            return false;
        }

        // 4. Verificar que la imagen se creó
        struct stat buffer;
        if (stat(outputPath.c_str(), &buffer) != 0) {
            errMsg = "Error: No se pudo generar la imagen";
            return false;
        }

        return true;
    }

    // ============================================================
    // REPORTE MBR
    // ============================================================
    bool ReportMBR(const MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        std::ostringstream dot;
        dot << "digraph G {\n";
        dot << "  labelloc=\"t\";\n";
        dot << "  label = \"Reporte MBR - Disco: " << std::filesystem::path(diskPath).filename().string() << "\";\n";
        dot << "  node [shape=plaintext];\n\n";
        dot << "  tabla [label=<\n";
        dot << "  <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#F9F9F9\">\n";
        dot << "    <tr><td colspan=\"7\" bgcolor=\"#4CAF50\" color=\"white\"><b>INFORMACION DEL DISCO</b></td></tr>\n";
        dot << "    <tr><td><b>Ruta</b></td><td colspan=\"6\">" << diskPath << "</td></tr>\n";
        dot << "    <tr><td><b>Tamaño</b></td><td colspan=\"6\">" << mbr.mbr_tamano << " bytes</td></tr>\n";
        
        std::string fecha = ctime(&mbr.mbr_fecha_creacion);
        if (!fecha.empty() && fecha.back() == '\n') fecha.pop_back();
        dot << "    <tr><td><b>Fecha Creación</b></td><td colspan=\"6\">" << fecha << "</td></tr>\n";
        dot << "    <tr><td><b>Signature</b></td><td colspan=\"6\">" << mbr.mbr_dsk_signature << "</td></tr>\n";
        dot << "    <tr><td><b>Ajuste</b></td><td colspan=\"6\">" << mbr.dsk_fit << "</td></tr>\n";
        dot << "    <tr><td colspan=\"7\" bgcolor=\"#2196F3\" color=\"white\"><b>PARTICIONES MBR</b></td></tr>\n";
        dot << "    <tr><td><b>#</b></td><td><b>Estado</b></td><td><b>Tipo</b></td><td><b>Ajuste</b></td><td><b>Inicio</b></td><td><b>Tamaño</b></td><td><b>Nombre</b></td></tr>\n";
        
        int extendedSlot = -1;
        for (int i = 0; i < 4; i++) {
            const auto& p = mbr.mbr_partitions[i];
            if (p.part_s == 0) {
                dot << "    <tr><td>" << (i+1) << "</td><td colspan=\"6\" bgcolor=\"#E0E0E0\">Libre</td></tr>\n";
            } else {
                dot << "    <tr><td>" << (i+1) << "</td><td>" << (p.part_status == '1' ? "Montada" : "Libre") << "</td>";
                dot << "<td>" << (p.part_type == 'P' ? "Primaria" : p.part_type == 'E' ? "Extendida" : "Desconocido") << "</td>";
                dot << "<td>" << p.part_fit << "</td>";
                dot << "<td>" << p.part_start << "</td>";
                dot << "<td>" << p.part_s << "</td>";
                dot << "<td>" << trimNulls(p.part_name, 16) << "</td></tr>\n";
                if (p.part_type == 'E') extendedSlot = i;
            }
        }
        
        if (extendedSlot != -1) {
            dot << "    <tr><td colspan=\"7\" bgcolor=\"#FF9800\" color=\"white\"><b>EBR - PARTICIONES LOGICAS</b></td></tr>\n";
            dot << "    <tr><td><b>#</b></td><td><b>Inicio</b></td><td><b>Tamaño</b></td><td><b>Nombre</b></td><td colspan=\"3\"><b>Siguiente EBR</b></td></tr>\n";
            
            std::ifstream file(diskPath, std::ios::binary);
            if (file.is_open()) {
                EBR ebr{};
                int64_t currentPos = mbr.mbr_partitions[extendedSlot].part_start;
                int count = 1;
                
                while (true) {
                    file.seekg(currentPos, std::ios::beg);
                    file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                    if (!file || ebr.part_s == 0) break;
                    
                    dot << "    <tr><td>" << count << "</td>";
                    dot << "<td>" << ebr.part_start << "</td>";
                    dot << "<td>" << ebr.part_s << "</td>";
                    dot << "<td>" << trimNulls(ebr.part_name, 16) << "</td>";
                    dot << "<td colspan=\"3\">" << (ebr.part_next == -1 ? "Fin" : std::to_string(ebr.part_next)) << "</td></tr>\n";
                    
                    if (ebr.part_next == -1) break;
                    currentPos = ebr.part_next;
                    count++;
                }
                file.close();
            }
        }
        
        dot << "  </table>>]; }\n";
        
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

    // ============================================================
    // REPORTE SB
    // ============================================================
    bool ReportSB(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        if (sb.s_magic != 0xEF53) {
            errMsg = "La partición no está formateada como EXT2 (magic inválido)";
            return false;
        }
        
        std::ostringstream dot;
        dot << "digraph G {\n";
        dot << "  labelloc=\"t\";\n";
        dot << "  label = \"Reporte SUPERBLOQUE - Partición: " << partitionIndex << "\";\n";
        dot << "  node [shape=plaintext];\n\n";
        dot << "  tabla [label=<\n";
        dot << "  <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#F9F9F9\">\n";
        dot << "    <tr><td colspan=\"2\" bgcolor=\"#FF9800\" color=\"white\"><b>SUPERBLOQUE</b></td></tr>\n";
        dot << "    <tr><td><b>Tipo de sistema</b></td><td>" << sb.s_filesystem_type << "</td></tr>\n";
        dot << "    <tr><td><b>Inodos totales</b></td><td>" << sb.s_inodes_count << "</td></tr>\n";
        dot << "    <tr><td><b>Bloques totales</b></td><td>" << sb.s_blocks_count << "</td></tr>\n";
        dot << "    <tr><td><b>Inodos libres</b></td><td>" << sb.s_free_inodes_count << "</td></tr>\n";
        dot << "    <tr><td><b>Bloques libres</b></td><td>" << sb.s_free_blocks_count << "</td></tr>\n";
        dot << "    <tr><td><b>Magic</b></td><td>0x" << std::hex << sb.s_magic << std::dec << "</td></tr>\n";
        dot << "    <tr><td><b>Tamaño inodo</b></td><td>" << sb.s_inode_s << "</td></tr>\n";
        dot << "    <tr><td><b>Tamaño bloque</b></td><td>" << sb.s_block_s << "</td></tr>\n";
        dot << "    <tr><td><b>Primer inodo</b></td><td>" << sb.s_first_ino << "</td></tr>\n";
        dot << "    <tr><td><b>Primer bloque</b></td><td>" << sb.s_first_blo << "</td></tr>\n";
        dot << "    <tr><td><b>Bitmap inodos inicio</b></td><td>" << sb.s_bm_inode_start << "</td></tr>\n";
        dot << "    <tr><td><b>Bitmap bloques inicio</b></td><td>" << sb.s_bm_block_start << "</td></tr>\n";
        dot << "    <tr><td><b>Tabla inodos inicio</b></td><td>" << sb.s_inode_start << "</td></tr>\n";
        dot << "    <tr><td><b>Tabla bloques inicio</b></td><td>" << sb.s_block_start << "</td></tr>\n";
        
        std::string mtime = ctime(&sb.s_mtime);
        if (!mtime.empty() && mtime.back() == '\n') mtime.pop_back();
        std::string umtime = ctime(&sb.s_umtime);
        if (!umtime.empty() && umtime.back() == '\n') umtime.pop_back();
        
        dot << "    <tr><td><b>Último montaje</b></td><td>" << mtime << "</td></tr>\n";
        dot << "    <tr><td><b>Último desmontaje</b></td><td>" << umtime << "</td></tr>\n";
        dot << "    <tr><td><b>Veces montado</b></td><td>" << sb.s_mnt_count << "</td></tr>\n";
        dot << "  </table>>]; }\n";
        
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

    // ============================================================
    // REPORTE INODE 
    // ============================================================
    bool ReportINODE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        if (sb.s_magic != 0xEF53) {
            errMsg = "La partición no está formateada como EXT2 (magic inválido)";
            return false;
        }
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            errMsg = "Error al abrir el disco";
            return false;
        }
        
        int bmSize = (sb.s_inodes_count + 7) / 8;
        std::vector<char> bmInode(bmSize);
        disk.seekg(sb.s_bm_inode_start, std::ios::beg);
        disk.read(bmInode.data(), bmSize);
        
        std::ostringstream dot;
        dot << "digraph G {\n";
        dot << "  labelloc=\"t\";\n";
        dot << "  label = \"Reporte INODOS (Inodos usados)\";\n";
        dot << "  node [shape=box];\n";
        dot << "  rankdir=TB;\n\n";
        
        int count = 0;
        for (int i = 0; i < sb.s_inodes_count; i++) {
            int byteIndex = i / 8;
            int bitIndex = i % 8;
            if (!(bmInode[byteIndex] & (1 << bitIndex))) continue;
            
            Inode inode = Ext2Utils::readInode(disk, sb, i);
            std::string type = (inode.i_type == '0' ? "Carpeta" : "Archivo");
            std::string perms = std::string(inode.i_perm, 3);
            
            std::string atime = ctime(&inode.i_atime);
            std::string ctime_str = ctime(&inode.i_ctime);
            std::string mtime = ctime(&inode.i_mtime);
            if (!atime.empty() && atime.back() == '\n') atime.pop_back();
            if (!ctime_str.empty() && ctime_str.back() == '\n') ctime_str.pop_back();
            if (!mtime.empty() && mtime.back() == '\n') mtime.pop_back();
            
            dot << "  inodo" << i << " [label=\"Inodo " << i << "\\n";
            dot << "UID: " << inode.i_uid << " | GID: " << inode.i_gid << "\\n";
            dot << "Tamaño: " << inode.i_s << " bytes\\n";
            dot << "Tipo: " << type << " | Permisos: " << perms << "\\n";
            dot << "i_atime: " << atime << "\\n";
            dot << "i_ctime: " << ctime_str << "\\n";
            dot << "i_mtime: " << mtime << "\\n";
            
            bool hasBlocks = false;
            dot << "Bloques: ";
            for (int j = 0; j < 16; j++) {
                if (inode.i_block[j] != -1) {
                    dot << inode.i_block[j] << " ";
                    hasBlocks = true;
                }
            }
            if (!hasBlocks) dot << "Ninguno";
            dot << "\"];\n";
            count++;
        }
        
        if (count == 0) {
            dot << "  vacio [label=\"No hay inodos usados\", shape=plaintext];\n";
        }
        
        dot << "}\n";
        
        disk.close();
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

    // ============================================================
    // REPORTE BLOCK 
    // ============================================================
    bool ReportBLOCK(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        if (sb.s_magic != 0xEF53) {
            errMsg = "La partición no está formateada como EXT2 (magic inválido)";
            return false;
        }
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            errMsg = "Error al abrir el disco";
            return false;
        }
        
        int bmSize = (sb.s_blocks_count + 7) / 8;
        std::vector<char> bmBlock(bmSize);
        disk.seekg(sb.s_bm_block_start, std::ios::beg);
        disk.read(bmBlock.data(), bmSize);
        
        std::map<int, char> blockType;
        std::map<int, std::string> blockContent;
        
        for (int i = 0; i < sb.s_inodes_count; i++) {
            Inode inode = Ext2Utils::readInode(disk, sb, i);
            for (int j = 0; j < 16; j++) {
                if (inode.i_block[j] != -1) {
                    if (inode.i_type == '0') {
                        blockType[inode.i_block[j]] = 'F';
                        BlockFolder fb = Ext2Utils::readBlockFolder(disk, sb, inode.i_block[j]);
                        std::string content;
                        for (int k = 0; k < 4; k++) {
                            std::string name = trimNulls(fb.b_content[k].b_name, 12);
                            if (!name.empty()) content += name + " ";
                        }
                        blockContent[inode.i_block[j]] = content.empty() ? "(vacío)" : content;
                    } else if (inode.i_type == '1' && j < 12) {
                        blockType[inode.i_block[j]] = 'A';
                        BlockFile fb = Ext2Utils::readBlockFile(disk, sb, inode.i_block[j]);
                        std::string content(fb.b_content);
                        content = content.c_str();
                        blockContent[inode.i_block[j]] = content.empty() ? "(vacío)" : content.substr(0, 64);
                    } else if (inode.i_type == '1' && j >= 12) {
                        blockType[inode.i_block[j]] = 'P';
                        BlockPointer pb = Ext2Utils::readBlockPointer(disk, sb, inode.i_block[j]);
                        std::string content;
                        for (int k = 0; k < 16; k++) {
                            if (pb.b_pointer[k] != -1) content += std::to_string(pb.b_pointer[k]) + " ";
                        }
                        blockContent[inode.i_block[j]] = content.empty() ? "(sin apuntadores)" : content;
                    }
                }
            }
        }
        
        std::ostringstream dot;
        dot << "digraph G {\n";
        dot << "  labelloc=\"t\";\n";
        dot << "  label = \"Reporte BLOQUES (Bloques usados)\";\n";
        dot << "  node [shape=box];\n";
        dot << "  rankdir=TB;\n\n";
        
        int count = 0;
        for (int i = 0; i < sb.s_blocks_count; i++) {
            int byteIndex = i / 8;
            int bitIndex = i % 8;
            if (!(bmBlock[byteIndex] & (1 << bitIndex))) continue;
            
            count++;
            std::string typeLabel = "Desconocido";
            if (blockType.count(i)) {
                if (blockType[i] == 'F') typeLabel = "Carpeta";
                else if (blockType[i] == 'A') typeLabel = "Archivo";
                else if (blockType[i] == 'P') typeLabel = "Apuntador";
            }
            std::string content = blockContent.count(i) ? blockContent[i] : "(sin información)";
            
            dot << "  bloque" << i << " [label=\"Bloque " << i << "\\n";
            dot << "Tipo: " << typeLabel << "\\n";
            dot << "Contenido: " << content.substr(0, 50) << "\"];\n";
        }
        
        if (count == 0) {
            dot << "  vacio [label=\"No hay bloques usados\", shape=plaintext];\n";
        }
        
        dot << "}\n";
        
        disk.close();
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

    // ============================================================
    // REPORTE LS 
    // ============================================================
    bool ReportLS(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, const std::string& dirPath, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        if (sb.s_magic != 0xEF53) {
            errMsg = "La partición no está formateada como EXT2 (magic inválido)";
            return false;
        }
        
        int dirInode = Ext2Utils::findInodeByPath(diskPath, dirPath, sb, mbr, partitionIndex);
        
        if (dirInode == -1) {
            errMsg = "Directorio no encontrado: " + dirPath;
            return false;
        }
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            errMsg = "Error al abrir el disco";
            return false;
        }
        
        Inode inode = Ext2Utils::readInode(disk, sb, dirInode);
        
        std::vector<std::string> userLines = Ext2Utils::readUsersFile(diskPath, "", mbr, partitionIndex);
        std::map<int, std::string> userNames;
        std::map<int, std::string> groupNames;
        
        for (const auto& line : userLines) {
            if (line.find(", U, ") != std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, ',')) {
                    parts.push_back(token);
                }
                if (parts.size() >= 5) {
                    for (auto& p : parts) {
                        p.erase(0, p.find_first_not_of(" "));
                        p.erase(p.find_last_not_of(" ") + 1);
                    }
                    int uid = std::stoi(parts[0]);
                    if (uid != 0) {
                        userNames[uid] = parts[3];
                    }
                }
            } else if (line.find(", G, ") != std::string::npos) {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, ',')) {
                    parts.push_back(token);
                }
                if (parts.size() >= 3) {
                    for (auto& p : parts) {
                        p.erase(0, p.find_first_not_of(" "));
                        p.erase(p.find_last_not_of(" ") + 1);
                    }
                    int gid = std::stoi(parts[0]);
                    if (gid != 0) {
                        groupNames[gid] = parts[2];
                    }
                }
            }
        }
        
        std::ostringstream dot;
        dot << "digraph G {\n";
        dot << "  labelloc=\"t\";\n";
        dot << "  label = \"Reporte LS - Directorio: " << dirPath << "\";\n";
        dot << "  node [shape=plaintext];\n\n";
        dot << "  tabla [label=<\n";
        dot << "  <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#F9F9F9\">\n";
        
        dot << "    <tr><td colspan=\"8\" bgcolor=\"#9C27B0\" color=\"white\"><b>LISTADO DE ARCHIVOS Y CARPETAS</b></td></tr>\n";
        dot << "    <tr><td><b>Permisos</b></td><td><b>Owner</b></td><td><b>Grupo</b></td><td><b>Size (en Bytes)</b></td><td><b>Fecha</b></td><td><b>Hora</b></td><td><b>Tipo</b></td><td><b>Name</b></td></tr>\n";
        
        for (int i = 0; i < 12; i++) {
            if (inode.i_block[i] == -1) continue;
            BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, inode.i_block[i]);
            for (int j = 0; j < 4; j++) {
                std::string name = trimNulls(block.b_content[j].b_name, 12);
                if (name.empty() || name == "." || name == "..") continue;
                if (block.b_content[j].b_inodo == -1) continue;
                
                int childInode = block.b_content[j].b_inodo;
                Inode child = Ext2Utils::readInode(disk, sb, childInode);
                
                std::string perms = std::string(child.i_perm, 3);
                std::string type = (child.i_type == '0' ? "d" : "-");
                std::string size = std::to_string(child.i_s);
                
                std::string dateTime = ctime(&child.i_mtime);
                std::string date = dateTime.substr(4, 6);
                std::string hora = dateTime.substr(11, 8);
                
                std::string owner = "User" + std::to_string(child.i_uid);
                std::string grupo = "Group" + std::to_string(child.i_gid);
                if (userNames.count(child.i_uid)) {
                    owner = userNames[child.i_uid];
                }
                if (groupNames.count(child.i_gid)) {
                    grupo = groupNames[child.i_gid];
                }
                
                dot << "    <tr><td>" << perms << "</td>";
                dot << "<td>" << owner << "</td>";
                dot << "<td>" << grupo << "</td>";
                dot << "<td>" << size << "</td>";
                dot << "<td>" << date << "</td>";
                dot << "<td>" << hora << "</td>";
                dot << "<td>" << type << "</td>";
                dot << "<td>" << name << "</td></tr>\n";
            }
        }
        
        dot << "  </table>>]; }\n";
        
        disk.close();
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

    // ============================================================
    // REPORTE DISK 
    // ============================================================
    bool ReportDISK(const MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        const double mbrSize = static_cast<double>(sizeof(MBR));
        double totalSize = static_cast<double>(mbr.mbr_tamano);
        std::string diskName = std::filesystem::path(diskPath).filename().string();
        double mbrPercent = (mbrSize / totalSize) * 100.0;
        
        bool extendedFound = false;
        int64_t extendedStart = 0;
        double extendedSize = 0;
        double extendedPercent = 0;
        std::vector<EBR> ebrs;
        double freeExtended = 0;
        double freeExtendedPercent = 0;
        
        for (const auto& part : mbr.mbr_partitions) {
            if (part.part_s == 0 || part.part_type != 'E') continue;
            extendedFound = true;
            extendedStart = part.part_start;
            extendedSize = static_cast<double>(part.part_s);
            extendedPercent = (extendedSize / totalSize) * 100.0;
            break;
        }
        
        if (extendedFound) {
            std::ifstream file(diskPath, std::ios::binary);
            if (file.is_open()) {
                EBR ebr{};
                file.seekg(extendedStart, std::ios::beg);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                if (file) {
                    if (ebr.part_s == 0 && ebr.part_next != -1) {
                        file.seekg(ebr.part_next, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                    }
                    
                    double logicalTotalSize = 0;
                    while (file && ebr.part_s != 0) {
                        logicalTotalSize += static_cast<double>(ebr.part_s);
                        ebrs.push_back(ebr);
                        if (ebr.part_next == -1) break;
                        file.seekg(ebr.part_next, std::ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(EBR));
                    }

                    double ebrSpace = static_cast<double>((ebrs.size() + 1) * sizeof(EBR));
                    freeExtended = extendedSize - logicalTotalSize - ebrSpace;
                    if (freeExtended < 0) freeExtended = 0;
                    freeExtendedPercent = (freeExtended / totalSize) * 100.0;
                }
                file.close();
            }
        }
        
        bool hasFreeExtendedCell = (freeExtended > 0 && freeExtendedPercent > 0);
        int extendedColspan = static_cast<int>(ebrs.size()) + (hasFreeExtendedCell ? 1 : 0);
        if (extendedColspan < 1) extendedColspan = 1;
        
        std::ostringstream dot;
        dot << "digraph G {\n"
            << "  labelloc=\"t\";\n"
            << "  label = \"Reporte de Disco: " << diskName << " (Size: "
            << static_cast<long long>(totalSize) << " bytes)\";\n"
            << "  node [shape=plaintext];\n\n"
            << "  tabla [label=<\n"
            << "  <table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"10\" bgcolor=\"#F9F9F9\">\n";
        
        dot << "    <tr><td rowspan=\"2\" bgcolor=\"#A95C68\" border=\"1\" color=\"black\"><b>MBR</b><br/>"
            << static_cast<long long>(mbrSize) << " bytes<br/>"
            << fmt2(mbrPercent) << "% del Disco</td>";
        
        double freeSpace = totalSize - mbrSize;
        
        for (const auto& part : mbr.mbr_partitions) {
            if (part.part_s == 0) continue;
            
            std::string partName = trimNulls(part.part_name, 16);
            char partType = part.part_type;
            double partSize = static_cast<double>(part.part_s);
            double partPercent = (partSize / totalSize) * 100.0;
            freeSpace -= partSize;
            
            if (partType == 'E') {
                dot << "\n        <td colspan=\"" << extendedColspan
                    << "\" bgcolor=\"#F0E68C\" border=\"1\" color=\"black\"><b>EXTENDIDA</b><br/>"
                    << fmt2(partPercent) << "% del Disco</td>";
            } else {
                dot << "\n        <td rowspan=\"2\" bgcolor=\"#DAA06D\" border=\"1\" color=\"black\"><b>"
                    << partName << "</b><br/>" << static_cast<long long>(partSize)
                    << " bytes<br/>" << fmt2(partPercent) << "% del Disco</td>";
            }
        }
        
        if (freeSpace > 0) {
            double freePercent = (freeSpace / totalSize) * 100.0;
            dot << "\n        <td rowspan=\"2\" bgcolor=\"#E0E0E0\" border=\"1\" color=\"black\">Espacio Libre<br/>"
                << fmt2(freePercent) << "% del Disco</td>";
        }
        
        dot << "</tr>\n";
        
        if (extendedFound && !ebrs.empty()) {
            dot << "    <tr>";
            
            for (const auto& e : ebrs) {
                std::string ebrName = trimNulls(e.part_name, 16);
                double ebrSize = static_cast<double>(e.part_s);
                double ebrPercent = (ebrSize / totalSize) * 100.0;
                
                dot << "\n        <td bgcolor=\"#C2B280\" border=\"1\" color=\"black\"><b>Lógica</b><br/>"
                    << ebrName << "<br/>" << fmt2(ebrPercent) << "% del Disco</td>";
            }
            
            if (hasFreeExtendedCell) {
                dot << "\n        <td bgcolor=\"#E0E0E0\" border=\"1\" color=\"black\">Espacio Libre<br/>en extendida<br/>"
                    << fmt2(freeExtendedPercent) << "% del Disco</td>";
            }
            
            dot << "\n    </tr>\n";
        }
        
        dot << "  </table>>];\n"
            << "}";
        
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

    // ============================================================
    // REPORTE TREE
    // ============================================================
    bool ReportTREE(const std::string& diskPath, const MBR& mbr, int partitionIndex, const std::string& path, std::string& errMsg) {
        if (!createReportDirectories(path, errMsg)) return false;
        
        Superblock sb = Ext2Utils::readSuperblock(diskPath, mbr, partitionIndex);
        
        if (sb.s_magic != 0xEF53) {
            errMsg = "Superblock inválido o partición no formateada";
            return false;
        }
        
        std::fstream disk(diskPath, std::ios::in | std::ios::binary);
        if (!disk.is_open()) {
            errMsg = "Error al abrir el disco";
            return false;
        }
        
        Inode rootInode = Ext2Utils::readInode(disk, sb, 0);
        if (rootInode.i_type != '0') {
            errMsg = "Inodo raíz no encontrado o no es una carpeta";
            disk.close();
            return false;
        }
        
        std::ostringstream dot;
        dot << "digraph EXT2Tree {\n"
            << "  node [shape=box];\n"
            << "  rankdir=TB;\n\n";
        
        // ============================================================
        // Función para formatear inodo con TODA la información
        // ============================================================
        auto formatInodeLabel = [](const Inode& inode, const std::string& name) -> std::string {
            std::ostringstream label;
            std::string type = (inode.i_type == '0' ? "Carpeta" : "Archivo");
            std::string perms = std::string(inode.i_perm, 3);
            
            std::string atime = ctime(&inode.i_atime);
            std::string ctime_str = ctime(&inode.i_ctime);
            std::string mtime = ctime(&inode.i_mtime);
            if (!atime.empty() && atime.back() == '\n') atime.pop_back();
            if (!ctime_str.empty() && ctime_str.back() == '\n') ctime_str.pop_back();
            if (!mtime.empty() && mtime.back() == '\n') mtime.pop_back();
            
            label << name << "\\n";
            label << "UID: " << inode.i_uid << " | GID: " << inode.i_gid << "\\n";
            label << "Tamaño: " << inode.i_s << " bytes\\n";
            label << "Tipo: " << type << " | Permisos: " << perms << "\\n";
            label << "i_atime: " << atime << "\\n";
            label << "i_ctime: " << ctime_str << "\\n";
            label << "i_mtime: " << mtime << "\\n";
            
            bool hasBlocks = false;
            label << "Bloques: ";
            for (int j = 0; j < 16; j++) {
                if (inode.i_block[j] != -1) {
                    label << inode.i_block[j] << " ";
                    hasBlocks = true;
                }
            }
            if (!hasBlocks) label << "Ninguno";
            
            return label.str();
        };
        
        std::set<int> visitedInodes;

        std::function<void(std::fstream&, const Superblock&, const BlockFolder&, int, const std::string&, std::function<void(int, int, const std::string&)>&)> processFolderBlock;
        
        processFolderBlock = [&](std::fstream& diskRef, const Superblock& sbRef, const BlockFolder& block, int blockIndex, const std::string& parent, std::function<void(int, int, const std::string&)>& traverseRef) {
            std::string blockId = "bloque_" + std::to_string(blockIndex);
            dot << "  " << blockId << " [label=\"Bloque " << blockIndex << "\\n(Carpeta)\", shape=box, style=filled, fillcolor=\"#E8F5E9\"];\n";
            dot << "  " << parent << " -> " << blockId << " [label=\"bloque\"];\n";
            
            for (int j = 0; j < 4; j++) {
                std::string name = trimNulls(block.b_content[j].b_name, 12);
                if (name.empty() || name == "." || name == "..") continue;
                if (block.b_content[j].b_inodo == -1) continue;
                if (block.b_content[j].b_inodo >= sbRef.s_inodes_count) continue;
                
                int childInode = block.b_content[j].b_inodo;
                Inode child = Ext2Utils::readInode(diskRef, sbRef, childInode);
                if (child.i_type != '0' && child.i_type != '1') continue;
                
                std::string nodeId = "inodo" + std::to_string(childInode);
                std::string label = name;
                if (child.i_type == '0') {
                    label += " (Carpeta)";
                } else {
                    label += " (Archivo)";
                }
                
                dot << "  " << nodeId << " [label=\"" << formatInodeLabel(child, label) << "\"];\n";
                dot << "  " << blockId << " -> " << nodeId << " [label=\"" << name << "\"];\n";
                
                if (child.i_type == '1') {
                    for (int k = 0; k < 16; k++) {
                        if (child.i_block[k] == -1) continue;
                        if (child.i_block[k] >= sbRef.s_blocks_count) continue;
                        
                        if (k < 12) {
                            BlockFile fileBlock = Ext2Utils::readBlockFile(diskRef, sbRef, child.i_block[k]);
                            std::string content(fileBlock.b_content);
                            content = content.c_str();
                            
                            std::string fileBlockId = "bloque_archivo_" + std::to_string(child.i_block[k]);
                            std::string fileLabel = "Bloque " + std::to_string(child.i_block[k]) + " (Directo)";
                            if (!content.empty()) {
                                fileLabel += "\\nContenido: " + content.substr(0, 30);
                            }
                            dot << "  " << fileBlockId << " [label=\"" << fileLabel << "\", shape=box, style=filled, fillcolor=\"#E3F2FD\"];\n";
                            dot << "  " << nodeId << " -> " << fileBlockId << " [label=\"i_block[" << k << "]\"];\n";
                        } else {
                            BlockPointer pointer = Ext2Utils::readBlockPointer(diskRef, sbRef, child.i_block[k]);
                            
                            std::string ptrNodeId = "ptr_archivo_" + std::to_string(child.i_block[k]);
                            std::string ptrLabel = "Bloque " + std::to_string(child.i_block[k]);
                            if (k == 12) ptrLabel += " (Simple Indirecto)";
                            else if (k == 13) ptrLabel += " (Doble Indirecto)";
                            else if (k == 14) ptrLabel += " (Triple Indirecto)";
                            
                            dot << "  " << ptrNodeId << " [label=\"" << ptrLabel << "\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                            dot << "  " << nodeId << " -> " << ptrNodeId << " [label=\"i_block[" << k << "]\"];\n";
                            
                            for (int l = 0; l < 16; l++) {
                                if (pointer.b_pointer[l] == -1) continue;
                                if (pointer.b_pointer[l] >= sbRef.s_blocks_count) continue;
                                
                                if (k == 12) {
                                    BlockFile fileBlock = Ext2Utils::readBlockFile(diskRef, sbRef, pointer.b_pointer[l]);
                                    std::string content2(fileBlock.b_content);
                                    content2 = content2.c_str();
                                    
                                    std::string fileBlockId = "bloque_archivo_" + std::to_string(pointer.b_pointer[l]);
                                    std::string fileLabel = "Bloque " + std::to_string(pointer.b_pointer[l]);
                                    if (!content2.empty()) {
                                        fileLabel += "\\nContenido: " + content2.substr(0, 30);
                                    }
                                    dot << "  " << fileBlockId << " [label=\"" << fileLabel << "\", shape=box, style=filled, fillcolor=\"#E3F2FD\"];\n";
                                    dot << "  " << ptrNodeId << " -> " << fileBlockId << " [label=\"b_pointer[" << l << "]\"];\n";
                                } else if (k == 13) {
                                    BlockPointer pointer2 = Ext2Utils::readBlockPointer(diskRef, sbRef, pointer.b_pointer[l]);
                                    std::string ptr2NodeId = "ptr_archivo2_" + std::to_string(pointer.b_pointer[l]);
                                    dot << "  " << ptr2NodeId << " [label=\"Bloque " << pointer.b_pointer[l] << " (Doble Indirecto Nivel 2)\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                                    dot << "  " << ptrNodeId << " -> " << ptr2NodeId << " [label=\"b_pointer[" << l << "]\"];\n";
                                    
                                    for (int m = 0; m < 16; m++) {
                                        if (pointer2.b_pointer[m] == -1) continue;
                                        if (pointer2.b_pointer[m] >= sbRef.s_blocks_count) continue;
                                        
                                        BlockFile fileBlock = Ext2Utils::readBlockFile(diskRef, sbRef, pointer2.b_pointer[m]);
                                        std::string content3(fileBlock.b_content);
                                        content3 = content3.c_str();
                                        
                                        std::string fileBlockId = "bloque_archivo_" + std::to_string(pointer2.b_pointer[m]);
                                        std::string fileLabel = "Bloque " + std::to_string(pointer2.b_pointer[m]);
                                        if (!content3.empty()) {
                                            fileLabel += "\\nContenido: " + content3.substr(0, 30);
                                        }
                                        dot << "  " << fileBlockId << " [label=\"" << fileLabel << "\", shape=box, style=filled, fillcolor=\"#E3F2FD\"];\n";
                                        dot << "  " << ptr2NodeId << " -> " << fileBlockId << " [label=\"b_pointer[" << m << "]\"];\n";
                                    }
                                } else if (k == 14) {
                                    BlockPointer pointer2 = Ext2Utils::readBlockPointer(diskRef, sbRef, pointer.b_pointer[l]);
                                    std::string ptr2NodeId = "ptr_archivo2_" + std::to_string(pointer.b_pointer[l]);
                                    dot << "  " << ptr2NodeId << " [label=\"Bloque " << pointer.b_pointer[l] << " (Triple Indirecto Nivel 2)\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                                    dot << "  " << ptrNodeId << " -> " << ptr2NodeId << " [label=\"b_pointer[" << l << "]\"];\n";
                                    
                                    for (int m = 0; m < 16; m++) {
                                        if (pointer2.b_pointer[m] == -1) continue;
                                        if (pointer2.b_pointer[m] >= sbRef.s_blocks_count) continue;
                                        
                                        BlockPointer pointer3 = Ext2Utils::readBlockPointer(diskRef, sbRef, pointer2.b_pointer[m]);
                                        std::string ptr3NodeId = "ptr_archivo3_" + std::to_string(pointer2.b_pointer[m]);
                                        dot << "  " << ptr3NodeId << " [label=\"Bloque " << pointer2.b_pointer[m] << " (Triple Indirecto Nivel 3)\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                                        dot << "  " << ptr2NodeId << " -> " << ptr3NodeId << " [label=\"b_pointer[" << m << "]\"];\n";
                                        
                                        for (int n = 0; n < 16; n++) {
                                            if (pointer3.b_pointer[n] == -1) continue;
                                            if (pointer3.b_pointer[n] >= sbRef.s_blocks_count) continue;
                                            
                                            BlockFile fileBlock = Ext2Utils::readBlockFile(diskRef, sbRef, pointer3.b_pointer[n]);
                                            std::string content4(fileBlock.b_content);
                                            content4 = content4.c_str();
                                            
                                            std::string fileBlockId = "bloque_archivo_" + std::to_string(pointer3.b_pointer[n]);
                                            std::string fileLabel = "Bloque " + std::to_string(pointer3.b_pointer[n]);
                                            if (!content4.empty()) {
                                                fileLabel += "\\nContenido: " + content4.substr(0, 30);
                                            }
                                            dot << "  " << fileBlockId << " [label=\"" << fileLabel << "\", shape=box, style=filled, fillcolor=\"#E3F2FD\"];\n";
                                            dot << "  " << ptr3NodeId << " -> " << fileBlockId << " [label=\"b_pointer[" << n << "]\"];\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                if (child.i_type == '0') {
                    traverseRef(childInode, 0, nodeId);
                }
            }
        };
        
        // ============================================================
        // traverseTree con límite de profundidad y control de ciclos
        // ============================================================
        std::function<void(int, int, const std::string&)> traverseTree = [&](int inodeIndex, int depth, const std::string& parent) {
            if (depth > 50) return;
            if (visitedInodes.count(inodeIndex)) return;
            visitedInodes.insert(inodeIndex);
            
            Inode inode = Ext2Utils::readInode(disk, sb, inodeIndex);
            if (inode.i_type != '0') return;
            
            for (int i = 0; i < 16; i++) {
                if (inode.i_block[i] == -1) continue;
                if (inode.i_block[i] >= sb.s_blocks_count) continue;
                
                if (i >= 12) {
                    BlockPointer pointer = Ext2Utils::readBlockPointer(disk, sb, inode.i_block[i]);
                    
                    std::string ptrNodeId = "ptr_" + std::to_string(inode.i_block[i]);
                    std::string ptrLabel = "Bloque " + std::to_string(inode.i_block[i]);
                    if (i == 12) ptrLabel += " (Simple Indirecto)";
                    else if (i == 13) ptrLabel += " (Doble Indirecto)";
                    else if (i == 14) ptrLabel += " (Triple Indirecto)";
                    
                    dot << "  " << ptrNodeId << " [label=\"" << ptrLabel << "\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                    dot << "  " << parent << " -> " << ptrNodeId << " [label=\"i_block[" << i << "]\"];\n";
                    
                    for (int k = 0; k < 16; k++) {
                        if (pointer.b_pointer[k] == -1) continue;
                        if (pointer.b_pointer[k] >= sb.s_blocks_count) continue;
                        
                        if (i == 12) {
                            BlockFolder childBlock = Ext2Utils::readBlockFolder(disk, sb, pointer.b_pointer[k]);
                            processFolderBlock(disk, sb, childBlock, pointer.b_pointer[k], ptrNodeId, traverseTree);
                        } else if (i == 13) {
                            BlockPointer pointer2 = Ext2Utils::readBlockPointer(disk, sb, pointer.b_pointer[k]);
                            std::string ptr2NodeId = "ptr2_" + std::to_string(pointer.b_pointer[k]);
                            dot << "  " << ptr2NodeId << " [label=\"Bloque " << pointer.b_pointer[k] << " (Doble Indirecto Nivel 2)\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                            dot << "  " << ptrNodeId << " -> " << ptr2NodeId << " [label=\"b_pointer[" << k << "]\"];\n";
                            
                            for (int l = 0; l < 16; l++) {
                                if (pointer2.b_pointer[l] == -1) continue;
                                if (pointer2.b_pointer[l] >= sb.s_blocks_count) continue;
                                
                                BlockFolder childBlock = Ext2Utils::readBlockFolder(disk, sb, pointer2.b_pointer[l]);
                                processFolderBlock(disk, sb, childBlock, pointer2.b_pointer[l], ptr2NodeId, traverseTree);
                            }
                        } else if (i == 14) {
                            BlockPointer pointer2 = Ext2Utils::readBlockPointer(disk, sb, pointer.b_pointer[k]);
                            std::string ptr2NodeId = "ptr2_" + std::to_string(pointer.b_pointer[k]);
                            dot << "  " << ptr2NodeId << " [label=\"Bloque " << pointer.b_pointer[k] << " (Triple Indirecto Nivel 2)\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                            dot << "  " << ptrNodeId << " -> " << ptr2NodeId << " [label=\"b_pointer[" << k << "]\"];\n";
                            
                            for (int l = 0; l < 16; l++) {
                                if (pointer2.b_pointer[l] == -1) continue;
                                if (pointer2.b_pointer[l] >= sb.s_blocks_count) continue;
                                
                                BlockPointer pointer3 = Ext2Utils::readBlockPointer(disk, sb, pointer2.b_pointer[l]);
                                std::string ptr3NodeId = "ptr3_" + std::to_string(pointer2.b_pointer[l]);
                                dot << "  " << ptr3NodeId << " [label=\"Bloque " << pointer2.b_pointer[l] << " (Triple Indirecto Nivel 3)\", shape=box, style=filled, fillcolor=\"#FFF3E0\"];\n";
                                dot << "  " << ptr2NodeId << " -> " << ptr3NodeId << " [label=\"b_pointer[" << l << "]\"];\n";
                                
                                for (int m = 0; m < 16; m++) {
                                    if (pointer3.b_pointer[m] == -1) continue;
                                    if (pointer3.b_pointer[m] >= sb.s_blocks_count) continue;
                                    
                                    BlockFolder childBlock = Ext2Utils::readBlockFolder(disk, sb, pointer3.b_pointer[m]);
                                    processFolderBlock(disk, sb, childBlock, pointer3.b_pointer[m], ptr3NodeId, traverseTree);
                                }
                            }
                        }
                    }
                } else {
                    BlockFolder block = Ext2Utils::readBlockFolder(disk, sb, inode.i_block[i]);
                    processFolderBlock(disk, sb, block, inode.i_block[i], parent, traverseTree);
                }
            }
        };
        
        // Inicio del recorrido
        dot << "  inodo0 [label=\"" << formatInodeLabel(rootInode, "Inodo 0 (Raiz)") << "\"];\n";
        traverseTree(0, 0, "inodo0");
        dot << "}\n";
        
        disk.close();
        return generateGraphvizWithDot(dot.str(), path, errMsg);
    }

} // namespace Reports