<template>
  <div class="file-system-view">
    <div class="view-header">
      <h1>📂 Visualizador de Archivos</h1>
      <p>ID Partición: <strong>{{ mountId }}</strong></p>
    </div>

    <div class="path-bar">
      <button class="btn-up" :disabled="currentPath === '/'" @click="goUp">↑</button>
      <span class="path-display">{{ currentPath }}</span>
      <button class="btn-refresh" @click="refresh">↻</button>
    </div>

    <div class="content-area">
      <div v-if="loading" class="loading-state">Cargando...</div>
      <div v-else-if="error" class="error-state">{{ error }}</div>
      <div v-else-if="items.length === 0" class="empty-state">Carpeta vacía</div>
      <div v-else class="item-grid">
        <div
          v-for="item in items"
          :key="item.name"
          class="item-card"
          :class="{ 'is-folder': item.isFolder }"
          @click="openItem(item)"
        >
          <div class="item-icon">{{ item.isFolder ? '📁' : '📄' }}</div>
          <div class="item-name">{{ item.name }}</div>
          <div class="item-info">
            <span v-if="!item.isFolder">{{ formatSize(item.size) }}</span>
            <span class="item-perms">{{ item.perms || '---' }}</span>
          </div>
        </div>
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
          <pre v-if="fileContent" class="file-content">{{ fileContent }}</pre>
          <div v-else class="loading-content">Cargando contenido...</div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { analyzeCommand } from '../services/api.js'

export default {
  name: 'FileSystemView',
  data() {
    return {
      mountId: '',
      currentPath: '/',
      items: [],
      loading: false,
      error: null,
      showFileModal: false,
      selectedFileName: '',
      fileContent: ''
    }
  },
  mounted() {
    this.mountId = this.$route.query.id || ''
    if (!this.mountId) {
      this.$router.push('/visualizer')
      return
    }
    this.refresh()
  },
  methods: {
    async refresh() {
      if (!this.mountId) return
      this.loading = true
      this.error = null
      try {
        const pathArg = this.currentPath.includes(' ') ? `"${this.currentPath}"` : this.currentPath
        const result = await analyzeCommand(`lsjson -path=${pathArg} -id=${this.mountId}`)
        if (result.success && result.data?.data?.files) {
          this.items = result.data.data.files
        } else {
          this.error = result.data?.message || 'Error listando archivos'
          this.items = []
        }
      } catch (err) {
        this.error = 'Error al cargar archivos'
        this.items = []
      }
      this.loading = false
    },

    goUp() {
      if (this.currentPath === '/') return
      const parts = this.currentPath.split('/').filter(p => p)
      parts.pop()
      this.currentPath = '/' + parts.join('/')
      if (this.currentPath === '') this.currentPath = '/'
      this.refresh()
    },

    openItem(item) {
      if (item.isFolder) {
        this.currentPath = this.currentPath === '/' ? '/' + item.name : this.currentPath + '/' + item.name
        this.refresh()
      } else {
        this.viewFile(item)
      }
    },

    async viewFile(item) {
      this.selectedFileName = item.name
      this.showFileModal = true
      this.fileContent = ''

      const filePath = this.currentPath === '/' ? '/' + item.name : this.currentPath + '/' + item.name
      const pathArg = filePath.includes(' ') ? `"${filePath}"` : filePath

      try {
        const result = await analyzeCommand(`cat -file1=${pathArg}`)
        if (result.success && result.data?.data?.content !== undefined) {
          this.fileContent = result.data.data.content || '(Archivo vacío)'
        } else {
          this.fileContent = '⚠️ No se pudo leer el archivo'
        }
      } catch (err) {
        this.fileContent = '❌ Error al leer el archivo'
      }
    },

    closeFileModal() {
      this.showFileModal = false
      this.selectedFileName = ''
      this.fileContent = ''
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
.file-system-view {
  padding: 20px;
  max-width: 1200px;
  margin: 0 auto;
  width: 100%;
}

.view-header {
  text-align: center;
  margin-bottom: 24px;
}

.view-header h1 {
  font-size: 24px;
  font-weight: 700;
  color: #e6edf3;
  margin: 0 0 8px 0;
}

.view-header p {
  font-size: 13px;
  color: #8b949e;
  margin: 0;
}

.path-bar {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 20px;
  padding: 8px;
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
}

.btn-up,
.btn-refresh {
  padding: 6px 12px;
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #8b949e;
  cursor: pointer;
  font-size: 14px;
  transition: all 0.3s ease;
}

.btn-up:hover:not(:disabled),
.btn-refresh:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.btn-up:disabled {
  opacity: 0.3;
  cursor: not-allowed;
}

.path-display {
  flex: 1;
  padding: 6px 12px;
  background: #0d1117;
  border-radius: 6px;
  font-family: 'Courier New', monospace;
  font-size: 13px;
  color: #8b949e;
}

.content-area {
  min-height: 400px;
}

.item-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
  gap: 16px;
}

.item-card {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 16px;
  cursor: pointer;
  transition: all 0.3s ease;
  text-align: center;
}

.item-card:hover {
  border-color: #58a6ff;
  transform: translateY(-2px);
}

.item-card.is-folder {
  cursor: pointer;
}

.item-icon {
  font-size: 32px;
  margin-bottom: 8px;
}

.item-name {
  font-size: 13px;
  font-weight: 600;
  color: #e6edf3;
  margin-bottom: 6px;
  word-break: break-all;
}

.item-info {
  display: flex;
  flex-direction: column;
  gap: 2px;
  font-size: 11px;
  color: #8b949e;
}

.item-perms {
  font-family: monospace;
  font-size: 10px;
  color: #484f58;
}

.loading-state,
.empty-state,
.error-state {
  text-align: center;
  padding: 60px 20px;
  color: #8b949e;
  font-size: 14px;
}

.error-state {
  color: #f85149;
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
  max-width: 700px;
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
}

.modal-title {
  font-size: 14px;
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
}

.modal-close:hover {
  color: #f85149;
}

.modal-body {
  flex: 1;
  overflow: auto;
  padding: 16px;
}

.file-content {
  font-family: 'Courier New', monospace;
  font-size: 12px;
  color: #a6e3a1;
  white-space: pre-wrap;
  word-break: break-all;
  margin: 0;
  background: #0d1117;
  padding: 12px;
  border-radius: 6px;
  border: 1px solid #30363d;
}

.loading-content {
  color: #8b949e;
  text-align: center;
  padding: 20px;
}
</style>