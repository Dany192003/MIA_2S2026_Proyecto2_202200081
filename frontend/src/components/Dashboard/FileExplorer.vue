<template>
  <div class="file-explorer">
    <div class="explorer-header">
      <span class="explorer-title">Archivos</span>
      <span class="mount-info" v-if="activeMountId">ID: {{ activeMountId }}</span>
      <span class="mount-info" v-else>sin montar</span>
    </div>

    <div class="explorer-path">
      <button class="btn-up" @click="goUp" :disabled="currentPath === '/'">↑</button>
      <span class="path-text">{{ currentPath }}</span>
      <button class="btn-refresh" @click="refresh" title="Actualizar">↻</button>
    </div>

    <div class="explorer-content">
      <div v-if="loading" class="loading-state">
        <span>Cargando...</span>
      </div>
      <div v-else-if="error" class="error-state">
        <span class="error-icon">⚠️</span>
        <span class="error-text">{{ error }}</span>
        <span v-if="error.includes('sesión')" class="error-hint">Usa LOGIN para acceder a los archivos</span>
      </div>
      <div v-else-if="files.length === 0" class="empty-state">
        <p>Vacío</p>
        <span class="hint">La carpeta está vacía</span>
      </div>
      <div v-for="file in files" :key="file.name" class="file-item" 
           :class="{ 'is-folder': file.isFolder }" 
           @dblclick="openItem(file)">
        <span class="file-icon">{{ file.isFolder ? '📁' : '📄' }}</span>
        <span class="file-name">{{ file.name }}</span>
        <span class="file-size">{{ file.isFolder ? '-' : formatSize(file.size) }}</span>
        <span class="file-perms">{{ file.perms || '---' }}</span>
        <button v-if="!file.isFolder" class="btn-view" @click.stop="viewFile(file)" title="Ver contenido">👁️</button>
      </div>
    </div>

    <!-- Modal para ver contenido de archivo -->
    <div v-if="showFileModal" class="file-modal" @click.self="closeFileModal">
      <div class="modal-content">
        <div class="modal-header">
          <span class="modal-title">📄 {{ selectedFileName }}</span>
          <button class="modal-close" @click="closeFileModal">×</button>
        </div>
        <div class="modal-body">
          <div v-if="fileLoading" class="loading-content">
            <span>Cargando contenido...</span>
          </div>
          <pre v-else-if="fileContent" class="file-content">{{ fileContent }}</pre>
          <div v-else class="loading-content">
            <span>No se pudo cargar el contenido</span>
          </div>
        </div>
        <div class="modal-footer">
          <button class="btn-copy-content" @click="copyFileContent" :disabled="!fileContent">📋 Copiar</button>
          <button class="btn-close-modal" @click="closeFileModal">Cerrar</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { analyzeCommand } from '../../services/api.js'

export default {
  name: 'FileExplorer',
  props: {
    activePartition: {
      type: Object,
      default: () => ({ id: '', name: '', disk: '', status: '' })
    }
  },
  watch: {
    'activePartition.id': {
      handler(newVal) {
        if (newVal) {
          this.currentPath = '/'
          this.refresh()
        } else {
          this.files = []
          this.error = '⚠️ No hay partición seleccionada'
        }
      },
      immediate: true
    }
  },
  data() {
    return {
      currentPath: '/',
      files: [],
      loading: false,
      error: null,
      showFileModal: false,
      selectedFileName: '',
      fileContent: '',
      fileLoading: false,
      selectedFilePath: ''
    }
  },
  computed: {
    activeMountId() {
      return this.activePartition.id || ''
    }
  },
  methods: {
    async refresh() {
      const mountId = this.activePartition.id
      
      if (!mountId) {
        this.error = '⚠️ No hay partición seleccionada'
        this.files = []
        return
      }

      this.loading = true
      this.error = null
      
      try {
        // ✅ CORREGIDO: si la ruta tiene espacios, encerrarla en comillas
        const pathArg = this.currentPath.includes(' ') ? `"${this.currentPath}"` : this.currentPath
        const result = await analyzeCommand(`lsjson -path=${pathArg} -id=${mountId}`)
        
        if (result.success && result.data?.data?.files) {
          this.files = result.data.data.files
        } else if (result.data?.message && result.data.message.includes('No hay sesión')) {
          this.error = '⚠️ No hay sesión activa. Use LOGIN primero.'
          this.files = []
        } else if (result.data?.message && result.data.message.includes('no existe')) {
          this.error = '❌ La ruta no existe: ' + this.currentPath
          this.files = []
        } else {
          this.error = result.data?.message || result.error || 'Error al listar archivos'
          this.files = []
        }
      } catch (error) {
        console.error('Error listando archivos:', error)
        this.error = 'Error al cargar archivos'
        this.files = []
      }
      this.loading = false
    },
    goUp() {
      if (this.currentPath !== '/') {
        const parts = this.currentPath.split('/').filter(p => p)
        parts.pop()
        this.currentPath = '/' + parts.join('/')
        if (this.currentPath === '') this.currentPath = '/'
        this.refresh()
      }
    },
    openItem(file) {
      if (file.isFolder) {
        this.currentPath = this.currentPath === '/' ? '/' + file.name : this.currentPath + '/' + file.name
        this.refresh()
      } else {
        this.viewFile(file)
      }
    },
    async viewFile(file) {
      if (!file || file.isFolder) return
      
      this.selectedFileName = file.name
      this.selectedFilePath = this.currentPath === '/' ? '/' + file.name : this.currentPath + '/' + file.name
      this.showFileModal = true
      this.fileContent = ''
      this.fileLoading = true
      
      try {
        const pathArg = this.selectedFilePath.includes(' ') 
          ? `"${this.selectedFilePath}"` 
          : this.selectedFilePath
        const result = await analyzeCommand(`cat -file1=${pathArg}`)
        
        if (result.success && result.data?.data?.content !== undefined) {
          this.fileContent = result.data.data.content
        } else if (result.data?.data?.content === '') {
          this.fileContent = '(Archivo vacío)'
        } else {
          this.fileContent = '⚠️ No se pudo leer el contenido del archivo\n' + (result.data?.message || '')
        }
      } catch (error) {
        console.error('Error leyendo archivo:', error)
        this.fileContent = '❌ Error al leer el archivo'
      }
      this.fileLoading = false
    },
    closeFileModal() {
      this.showFileModal = false
      this.selectedFileName = ''
      this.fileContent = ''
      this.selectedFilePath = ''
      this.fileLoading = false
    },
    copyFileContent() {
      if (this.fileContent) {
        navigator.clipboard.writeText(this.fileContent)
          .then(() => alert('📋 Contenido copiado al portapapeles'))
          .catch(() => alert('📋 Contenido:\n' + this.fileContent))
      }
    },
    formatSize(bytes) {
      if (!bytes || bytes === 0) return '0 B'
      const k = 1024
      const sizes = ['B', 'KB', 'MB', 'GB']
      const i = Math.floor(Math.log(bytes) / Math.log(k))
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
    }
  }
}
</script>

<style scoped>
.file-explorer {
  background: #161b22;
  border-radius: 8px;
  border: 1px solid #30363d;
  padding: 8px 12px;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.explorer-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 4px;
  flex-shrink: 0;
}

.explorer-title {
  font-size: 10px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.mount-info {
  font-size: 9px;
  color: #3fb950;
  background: rgba(63, 185, 80, 0.1);
  padding: 0 8px;
  border-radius: 8px;
  border: 1px solid rgba(63, 185, 80, 0.2);
}

.explorer-path {
  display: flex;
  align-items: center;
  gap: 4px;
  margin-bottom: 4px;
  flex-shrink: 0;
}

.btn-up, .btn-refresh {
  background: #0d1117;
  border: 1px solid #30363d;
  color: #8b949e;
  border-radius: 4px;
  padding: 0 6px;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-up:hover:not(:disabled), .btn-refresh:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.btn-up:disabled {
  opacity: 0.3;
  cursor: not-allowed;
}

.path-text {
  font-size: 10px;
  color: #8b949e;
  font-family: 'Courier New', monospace;
  background: #0d1117;
  padding: 0 6px;
  border-radius: 3px;
  border: 1px solid #30363d;
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.explorer-content {
  flex: 1;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.loading-state, .empty-state, .error-state {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #8b949e;
  font-size: 12px;
}

.error-state {
  color: #f85149;
}

.error-icon {
  font-size: 20px;
  margin-bottom: 4px;
}

.error-text {
  font-size: 11px;
  text-align: center;
}

.error-hint {
  font-size: 9px;
  color: #8b949e;
  margin-top: 4px;
}

.hint {
  font-size: 9px;
  color: #30363d;
  margin-top: 2px;
}

.file-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 11px;
  color: #8b949e;
  transition: background 0.2s ease;
}

.file-item:hover {
  background: #21262d;
}

.file-item.is-folder {
  cursor: pointer;
  color: #e6edf3;
}

.file-item.is-folder:hover {
  background: #1c2333;
}

.file-icon {
  font-size: 12px;
}

.file-name {
  flex: 1;
  color: #e6edf3;
}

.file-size {
  font-size: 9px;
  color: #8b949e;
  min-width: 50px;
  text-align: right;
}

.file-perms {
  font-size: 8px;
  color: #30363d;
  font-family: monospace;
  min-width: 30px;
  text-align: center;
}

.btn-view {
  background: transparent;
  border: none;
  color: #8b949e;
  cursor: pointer;
  font-size: 10px;
  padding: 0 4px;
  border-radius: 3px;
  transition: all 0.2s ease;
}

.btn-view:hover {
  background: #30363d;
  color: #e6edf3;
}

/* Modal */
.file-modal {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: rgba(0, 0, 0, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  backdrop-filter: blur(4px);
}

.modal-content {
  background: #161b22;
  border-radius: 12px;
  border: 1px solid #30363d;
  max-width: 600px;
  width: 90%;
  max-height: 80vh;
  display: flex;
  flex-direction: column;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  border-bottom: 1px solid #30363d;
  flex-shrink: 0;
}

.modal-title {
  font-size: 13px;
  font-weight: 600;
  color: #e6edf3;
}

.modal-close {
  background: transparent;
  border: none;
  color: #8b949e;
  font-size: 20px;
  cursor: pointer;
  padding: 0 4px;
  transition: color 0.3s ease;
}

.modal-close:hover {
  color: #f85149;
}

.modal-body {
  flex: 1;
  overflow: auto;
  padding: 12px 16px;
  min-height: 100px;
  max-height: 400px;
}

.file-content {
  font-family: 'Courier New', monospace;
  font-size: 12px;
  color: #a6e3a1;
  white-space: pre-wrap;
  word-break: break-all;
  margin: 0;
  background: #0d1117;
  padding: 8px 12px;
  border-radius: 6px;
  border: 1px solid #30363d;
}

.loading-content {
  color: #8b949e;
  text-align: center;
  padding: 20px;
}

.modal-footer {
  display: flex;
  gap: 8px;
  padding: 10px 16px;
  border-top: 1px solid #30363d;
  flex-shrink: 0;
  justify-content: flex-end;
}

.btn-copy-content, .btn-close-modal {
  padding: 4px 14px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.3s ease;
  border: 1px solid #30363d;
}

.btn-copy-content {
  background: #58a6ff;
  color: #0d1117;
  border-color: #58a6ff;
}

.btn-copy-content:hover:not(:disabled) {
  background: #79c0ff;
}

.btn-copy-content:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.btn-close-modal {
  background: transparent;
  color: #8b949e;
}

.btn-close-modal:hover {
  border-color: #f85149;
  color: #f85149;
}

::-webkit-scrollbar {
  width: 4px;
}

::-webkit-scrollbar-track {
  background: #0d1117;
}

::-webkit-scrollbar-thumb {
  background: #30363d;
  border-radius: 2px;
}

::-webkit-scrollbar-thumb:hover {
  background: #58a6ff;
}
</style>