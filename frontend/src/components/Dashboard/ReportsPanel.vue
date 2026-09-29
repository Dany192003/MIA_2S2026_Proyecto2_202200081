<template>
  <div class="reports-panel">
    <div class="reports-header">
      <span class="reports-title">Reportes</span>
      <span class="reports-badge">{{ reportFiles.length }}</span>
      <button class="btn-refresh" @click="refresh" title="Actualizar">↻</button>
    </div>

    <!-- Tabs -->
    <div class="reports-tabs">
      <button 
        v-for="tab in reportTabs" 
        :key="tab.key" 
        class="tab-btn" 
        :class="{ active: activeTab === tab.key }" 
        @click="activeTab = tab.key"
      >
        {{ tab.label }}
        <span class="tab-count">{{ getTabCount(tab.key) }}</span>
      </button>
    </div>

    <!-- Lista de reportes -->
    <div class="reports-list">
      <div v-if="filteredReports.length === 0" class="empty-state">
        <p>No hay reportes generados</p>
        <span class="hint">Usa el comando <kbd>rep</kbd> para generar reportes</span>
      </div>
      <div 
        v-for="report in filteredReports" 
        :key="report.path" 
        class="report-item"
        @click="viewReport(report)"
        :class="{ 'selected': selectedReport && selectedReport.path === report.path }"
      >
        <div class="report-info">
          <span class="report-icon">{{ report.type === 'image' ? '🖼️' : '📄' }}</span>
          <span class="report-name">{{ report.displayName }}</span>
          <span class="report-size">{{ formatSize(report.size) }}</span>
        </div>
        <div class="report-status" :title="report.path">
          <span class="status-ok">{{ report.name }}</span>
        </div>
      </div>
    </div>

    <!-- Vista previa -->
    <div v-if="selectedReport" class="report-preview">
      <div class="preview-header">
        <span class="preview-title">{{ selectedReport.displayName }}</span>
        <button class="preview-close" @click="closePreview">×</button>
      </div>
      <div class="preview-content">
        <img 
          v-if="selectedReport.type === 'image'" 
          :src="'file://' + selectedReport.path" 
          alt="Reporte" 
          @error="handleImageError" 
        />
        <div v-else class="preview-info">
          📄 {{ selectedReport.name }}
          <p class="preview-hint">Este es un reporte de texto. Ábrelo con el botón de abajo.</p>
        </div>
      </div>
      <div class="preview-actions">
        <button class="btn-copy" @click="copyPath">📋 Copiar ruta</button>
        <button class="btn-open" @click="openInSystem">📂 Abrir archivo</button>
      </div>
      <p class="preview-path-full">{{ selectedReport.path }}</p>
    </div>
  </div>
</template>

<script>
import { analyzeCommand } from '../../services/api.js'

export default {
  name: 'ReportsPanel',
  props: {
    activePartition: {
      type: Object,
      default: () => ({ id: '', name: '', disk: '', status: '' })
    }
  },
  watch: {
    'activePartition.id': {
      handler() {
        this.refresh()
      },
      immediate: true
    }
  },
  data() {
    return {
      activeTab: 'sistema',
      selectedReport: null,
      reportFiles: [],
      reportTabs: [
        { key: 'sistema', label: 'Sistema' },
        { key: 'archivos', label: 'Archivos' },
        { key: 'bitmaps', label: 'Bitmaps' }
      ],
      // Tipos de reporte clasificados por tab
      reportTypes: {
        'mbr': 'sistema',
        'disk': 'sistema',
        'sb': 'sistema',
        'inode': 'archivos',
        'block': 'archivos',
        'tree': 'archivos',
        'ls': 'archivos',
        'file': 'archivos',
        'bm_inode': 'bitmaps',
        'bm_block': 'bitmaps'
      },
      reportDisplayNames: {
        'mbr': 'MBR',
        'disk': 'DISK',
        'sb': 'SUPERBLOQUE',
        'inode': 'INODOS',
        'block': 'BLOQUES',
        'tree': 'ÁRBOL',
        'ls': 'LISTADO',
        'file': 'ARCHIVO',
        'bm_inode': 'BITMAP INODOS',
        'bm_block': 'BITMAP BLOQUES'
      }
    }
  },
  computed: {
    // ✅ CORREGIDO: cada reporte ya tiene .reportType calculado en refresh()
    filteredReports() {
      return this.reportFiles
        .filter(r => r.reportType && this.reportTypes[r.reportType] === this.activeTab)
        .map(r => ({
          ...r,
          displayName: this.reportDisplayNames[r.reportType] || r.reportType.toUpperCase(),
          type: r.extension === '.png' ? 'image' : 'text'
        }))
        .sort((a, b) => a.displayName.localeCompare(b.displayName))
    }
  },
  mounted() {
    this.refresh()
  },
  methods: {
    // ✅ NUEVO: extraer el tipo de reporte del nombre del archivo
    extractReportType(filename) {
      // Quitar extensión
      let name = filename.replace(/\.(png|txt|jpg|dot)$/i, '').toLowerCase()
      
      // Buscar en el nombre si contiene alguna de las claves conocidas
      // Ordenamos por longitud descendente para que "bm_inode" se detecte antes que "inode"
      const keys = Object.keys(this.reportTypes).sort((a, b) => b.length - a.length)
      
      for (const key of keys) {
        // Buscar el key como palabra completa (con _ antes y después, o al inicio/final)
        const regex = new RegExp(`(^|_)${key}($|_)`, 'i')
        if (regex.test(name)) {
          return key
        }
      }
      
      return null
    },
    
    getTabCount(tabKey) {
      return this.reportFiles
        .filter(r => r.reportType && this.reportTypes[r.reportType] === tabKey)
        .length
    },
    
    async refresh() {
      try {
        const result = await analyzeCommand('lsreports')
        if (result.success && result.data?.data?.reports) {
          // ✅ CORREGIDO: procesar cada reporte y asignarle su tipo
          this.reportFiles = result.data.data.reports
            .filter(r => r.extension !== '.dot')
            .map(r => ({
              ...r,
              reportType: this.extractReportType(r.name)
            }))
            .filter(r => r.reportType !== null) // solo los que tienen tipo válido
        } else {
          this.reportFiles = []
        }
      } catch (error) {
        console.error('Error escaneando reportes:', error)
        this.reportFiles = []
      }
    },
    
    formatSize(bytes) {
      if (!bytes || bytes === 0) return '0 B'
      const k = 1024
      const sizes = ['B', 'KB', 'MB', 'GB']
      const i = Math.floor(Math.log(bytes) / Math.log(k))
      return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i]
    },
    
    viewReport(report) {
      this.selectedReport = report
    },
    
    closePreview() {
      this.selectedReport = null
    },
    
    handleImageError() {
      console.warn('Error cargando imagen del reporte (puede que el navegador bloquee file://)')
    },
    
    copyPath() {
      if (this.selectedReport) {
        navigator.clipboard.writeText(this.selectedReport.path)
          .then(() => alert('📋 Ruta copiada:\n' + this.selectedReport.path))
          .catch(() => alert('📋 Ruta:\n' + this.selectedReport.path))
      }
    },
    
    openInSystem() {
      if (this.selectedReport) {
        const fileUrl = 'file://' + this.selectedReport.path
        window.open(fileUrl, '_blank')
      }
    }
  }
}
</script>

<style scoped>
.reports-panel {
  background: #161b22;
  border-radius: 8px;
  border: 1px solid #30363d;
  padding: 8px 12px;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.reports-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 4px;
  flex-shrink: 0;
}

.reports-title {
  font-size: 10px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.reports-badge {
  font-size: 9px;
  color: #3fb950;
  background: rgba(63, 185, 80, 0.1);
  padding: 0 8px;
  border-radius: 8px;
  border: 1px solid rgba(63, 185, 80, 0.2);
}

.btn-refresh {
  background: transparent;
  border: 1px solid #30363d;
  color: #8b949e;
  border-radius: 4px;
  padding: 0 6px;
  cursor: pointer;
  font-size: 12px;
  transition: all 0.3s ease;
}

.btn-refresh:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

/* TABS */
.reports-tabs {
  display: flex;
  gap: 4px;
  margin-bottom: 6px;
  border-bottom: 1px solid #30363d;
  padding-bottom: 6px;
  flex-shrink: 0;
}

.tab-btn {
  background: transparent;
  border: none;
  color: #8b949e;
  padding: 4px 12px;
  font-size: 10px;
  font-weight: 500;
  cursor: pointer;
  border-radius: 4px;
  transition: all 0.3s ease;
  display: flex;
  align-items: center;
  gap: 4px;
}

.tab-btn:hover {
  color: #e6edf3;
  background: #21262d;
}

.tab-btn.active {
  color: #58a6ff;
  background: rgba(88, 166, 255, 0.1);
}

.tab-count {
  font-size: 8px;
  color: #8b949e;
  background: #0d1117;
  padding: 0 6px;
  border-radius: 8px;
  border: 1px solid #30363d;
}

.tab-btn.active .tab-count {
  color: #58a6ff;
  border-color: rgba(88, 166, 255, 0.3);
}

/* LISTA */
.reports-list {
  flex: 1;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #8b949e;
  font-size: 11px;
  padding: 20px 10px;
  text-align: center;
}

.empty-state kbd {
  background: #0d1117;
  padding: 0 6px;
  border-radius: 3px;
  border: 1px solid #30363d;
  font-family: monospace;
  font-size: 10px;
  color: #e6edf3;
}

.hint {
  font-size: 9px;
  color: #30363d;
  margin-top: 4px;
}

.report-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 4px 10px;
  border-radius: 4px;
  background: #0d1117;
  border: 1px solid #30363d;
  cursor: pointer;
  transition: all 0.3s ease;
  min-height: 28px;
}

.report-item:hover {
  border-color: #58a6ff;
  background: #161b22;
}

.report-item.selected {
  border-color: #58a6ff;
  background: rgba(88, 166, 255, 0.1);
}

.report-info {
  display: flex;
  align-items: center;
  gap: 8px;
  flex: 1;
  min-width: 0;
}

.report-icon {
  font-size: 14px;
  flex-shrink: 0;
}

.report-name {
  font-size: 11px;
  font-weight: 500;
  color: #e6edf3;
}

.report-size {
  font-size: 9px;
  color: #8b949e;
  flex-shrink: 0;
}

.report-status {
  font-size: 8px;
  max-width: 180px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  flex-shrink: 0;
}

.status-ok {
  color: #3fb950;
  font-size: 8px;
  font-family: monospace;
}

/* PREVIEW */
.report-preview {
  margin-top: 6px;
  border-top: 1px solid #30363d;
  padding-top: 6px;
  flex-shrink: 0;
  max-height: 200px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

.preview-header {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-bottom: 4px;
}

.preview-title {
  font-size: 10px;
  font-weight: 600;
  color: #e6edf3;
  flex: 1;
}

.preview-close {
  background: transparent;
  border: none;
  color: #8b949e;
  font-size: 16px;
  cursor: pointer;
  padding: 0 4px;
}

.preview-close:hover {
  color: #f85149;
}

.preview-content {
  background: #0d1117;
  border-radius: 4px;
  border: 1px solid #30363d;
  overflow: auto;
  min-height: 50px;
  max-height: 100px;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 4px;
}

.preview-content img {
  max-width: 100%;
  max-height: 90px;
  object-fit: contain;
}

.preview-info {
  font-size: 10px;
  color: #8b949e;
  text-align: center;
  padding: 8px;
}

.preview-hint {
  font-size: 9px;
  color: #30363d;
  margin: 4px 0 0 0;
}

.preview-actions {
  display: flex;
  gap: 6px;
  margin-top: 4px;
}

.btn-copy, .btn-open {
  background: transparent;
  border: 1px solid #30363d;
  color: #8b949e;
  padding: 3px 10px;
  border-radius: 3px;
  font-size: 9px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-copy:hover, .btn-open:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.preview-path-full {
  font-size: 8px;
  color: #30363d;
  font-family: monospace;
  margin: 4px 0 0 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

::-webkit-scrollbar {
  width: 3px;
}

::-webkit-scrollbar-track {
  background: #0d1117;
}

::-webkit-scrollbar-thumb {
  background: #30363d;
  border-radius: 2px;
}
</style>