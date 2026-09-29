<template>
  <div class="system-summary">
    <div class="summary-header">
      <span class="summary-title">Sistema</span>
      <span v-if="isLoggedIn" class="session-badge">✅ Sesión activa</span>
      <span v-else-if="hasMountedPartitions" class="session-badge warning">⚠️ Sin sesión</span>
      <span v-else class="session-badge">⏳ Sin montar</span>
      <button class="btn-refresh" @click="refresh" title="Actualizar">↻</button>
    </div>
    <div v-if="loading" class="loading-state">
      <span>Cargando...</span>
    </div>
    <div v-else class="summary-cards">
      <div class="summary-card">
        <span class="card-number">{{ stats.discos }}</span>
        <span class="card-label">Discos</span>
      </div>
      <div class="summary-card">
        <span class="card-number">{{ stats.particiones }}</span>
        <span class="card-label">Particiones</span>
      </div>
      <div class="summary-card">
        <span class="card-number">{{ stats.archivos }}</span>
        <span class="card-label">Archivos</span>
      </div>
      <div class="summary-card">
        <span class="card-number">{{ stats.usuarios }}</span>
        <span class="card-label">Usuarios</span>
      </div>
      <div class="summary-card">
        <span class="card-number">{{ stats.montadas }}</span>
        <span class="card-label">Montadas</span>
      </div>
    </div>
  </div>
</template>

<script>
import { analyzeCommand, getSessionStatus } from '../../services/api.js'

export default {
  name: 'SystemSummary',
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
      stats: {
        discos: 0,
        particiones: 0,
        archivos: 0,
        usuarios: 0,
        montadas: 0
      },
      loading: false,
      sessionId: '',
      isLoggedIn: false,
      hasMountedPartitions: false
    }
  },
  mounted() {
    this.refresh()
  },
  methods: {
    async countFilesRecursive(path, depth = 0, visited = new Set()) {
      if (depth > 20) return 0
      if (visited.has(path)) return 0
      visited.add(path)
      
      let count = 0
      try {
        const mountId = this.activePartition.id || this.sessionId
        if (!mountId) return 0
        
        const pathArg = path.includes(' ') ? `"${path}"` : path
        const result = await analyzeCommand(`lsjson -path=${pathArg} -id=${mountId}`)
        if (result.success && result.data?.data?.files) {
          for (const file of result.data.data.files) {
            if (!file.isFolder) {
              count++
            } else {
              const subPath = path === '/' ? '/' + file.name : path + '/' + file.name
              count += await this.countFilesRecursive(subPath, depth + 1, visited)
            }
          }
        }
      } catch (error) {
        console.error('Error contando archivos:', error)
      }
      return count
    },
    
    async refresh() {
      this.loading = true
      try {
        // 1. Discos y particiones
        const lsdiskResult = await analyzeCommand('lsdisk')
        if (lsdiskResult.success && lsdiskResult.data?.data?.disks) {
          const disks = lsdiskResult.data.data.disks
          this.stats.discos = disks.length
          
          let totalPartitions = 0
          for (const disk of disks) {
            totalPartitions += disk.partitions?.length || 0
          }
          this.stats.particiones = totalPartitions
        } else {
          this.stats.discos = 0
          this.stats.particiones = 0
        }
        
        // 2. Estado de sesión
        const sessionResult = await getSessionStatus()
        let mountId = ''
        
        if (sessionResult.success && sessionResult.data) {
          const session = sessionResult.data
          this.isLoggedIn = session.active === true
          this.stats.montadas = session.mountedCount || 0
          this.hasMountedPartitions = this.stats.montadas > 0
          
          if (session.active) {
            mountId = session.mountId || ''
            this.sessionId = mountId
          } else if (this.hasMountedPartitions) {
            mountId = this.activePartition.id || 
                     (session.mounted && session.mounted[0]?.id) || ''
            this.sessionId = mountId
          }
        } else {
          this.isLoggedIn = false
          this.hasMountedPartitions = false
          this.stats.montadas = 0
        }
        
        // Si activePartition tiene ID y estamos logueados, respetarlo
        if (this.isLoggedIn && this.activePartition.id) {
          mountId = this.activePartition.id
          this.sessionId = mountId
        }
        
        // 3. Usuarios (solo si hay sesión)
        if (this.isLoggedIn && mountId) {
          const usersResult = await analyzeCommand('cat -file1=/users.txt')
          if (usersResult.success && usersResult.data?.data?.content) {
            const lines = usersResult.data.data.content.split('\n').filter(line => line.trim())
            let userCount = 0
            for (const line of lines) {
              if (line.includes(', U, ')) {
                const parts = line.split(',').map(p => p.trim())
                if (parts[0] !== '0') {
                  userCount++
                }
              }
            }
            this.stats.usuarios = userCount
          } else {
            this.stats.usuarios = 0
          }
        } else {
          this.stats.usuarios = 0
        }
        
        // 4. Archivos (solo si hay sesión)
        if (this.isLoggedIn && mountId) {
          try {
            const fileCount = await this.countFilesRecursive('/')
            this.stats.archivos = fileCount
          } catch (error) {
            console.error('Error contando archivos:', error)
            this.stats.archivos = 0
          }
        } else {
          this.stats.archivos = 0
        }
        
      } catch (error) {
        console.error('Error en SystemSummary.refresh:', error)
      }
      this.loading = false
    }
  }
}
</script>

<style scoped>
.system-summary {
  background: #161b22;
  border-radius: 8px;
  border: 1px solid #30363d;
  padding: 8px 12px;
  height: 100%;
  display: flex;
  flex-direction: column;
}

.summary-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
  flex-shrink: 0;
}

.summary-title {
  font-size: 10px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.session-badge {
  font-size: 8px;
  padding: 0 6px;
  border-radius: 8px;
  background: rgba(63, 185, 80, 0.15);
  color: #3fb950;
  border: 1px solid rgba(63, 185, 80, 0.2);
}

.session-badge.warning {
  background: rgba(210, 153, 34, 0.15);
  color: #d29922;
  border-color: rgba(210, 153, 34, 0.2);
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

.loading-state {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #8b949e;
  font-size: 12px;
}

.summary-cards {
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  gap: 6px;
  flex: 1;
}

.summary-card {
  background: #0d1117;
  border-radius: 6px;
  padding: 4px 6px;
  text-align: center;
  border: 1px solid #30363d;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

.card-number {
  font-size: 18px;
  font-weight: 700;
  color: #e6edf3;
  line-height: 1.2;
}

.card-label {
  font-size: 8px;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.3px;
}

@media (max-width: 640px) {
  .summary-cards {
    grid-template-columns: repeat(3, 1fr);
  }
}

@media (max-width: 480px) {
  .system-summary {
    padding: 6px 8px;
  }
  .card-number {
    font-size: 14px;
  }
  .card-label {
    font-size: 7px;
  }
}
</style>