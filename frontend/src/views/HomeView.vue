<template>
  <div class="home-view">
    <!-- ✅ NUEVO: Banner de sesión -->
    <div v-if="!store.state.isLoggedIn" class="session-banner">
      <div class="banner-content">
        <span class="banner-icon">⚠️</span>
        <span class="banner-text">
          <strong>No hay sesión activa.</strong> Los comandos de archivos (mkdir, mkfile, cat, etc.) requieren autenticación.
        </span>
        <router-link to="/login" class="banner-btn">Iniciar Sesión</router-link>
      </div>
    </div>
    <div v-else class="session-banner active">
      <div class="banner-content">
        <span class="banner-icon">✅</span>
        <span class="banner-text">
          Sesión activa como <strong>{{ store.state.currentUser }}</strong> en <strong>{{ store.state.mountId }}</strong>
        </span>
      </div>
    </div>

    <div class="terminal-section">
      <CommandTerminal
        @command-executed="handleCommandExecuted"
        @batch-completed="handleBatchCompleted"
      />
    </div>
    <div class="dashboard-section">
      <SystemSummary
        ref="systemSummary"
        :active-partition="activePartition"
      />
    </div>
    <div class="panels-section">
      <div class="panel-item">
        <DiskList
          ref="diskList"
          @partition-selected="handlePartitionSelected"
        />
      </div>
      <div class="panel-item">
        <FileExplorer
          ref="fileExplorer"
          :active-partition="activePartition"
        />
      </div>
      <div class="panel-item">
        <ReportsPanel
          :active-partition="activePartition"
        />
      </div>
    </div>
  </div>
</template>

<script>
import { inject } from 'vue'
import SystemSummary from '../components/Dashboard/SystemSummary.vue'
import DiskList from '../components/Dashboard/DiskList.vue'
import FileExplorer from '../components/Dashboard/FileExplorer.vue'
import ReportsPanel from '../components/Dashboard/ReportsPanel.vue'
import CommandTerminal from '../components/Terminal/CommandTerminal.vue'
import { analyzeCommand } from '../services/api.js'

export default {
  name: 'HomeView',
  components: {
    SystemSummary,
    DiskList,
    FileExplorer,
    ReportsPanel,
    CommandTerminal
  },
  setup() {
    const store = inject('store')
    return { store }
  },
  data() {
    return {
      activePartition: {
        id: '',
        name: '',
        disk: '',
        status: ''
      }
    }
  },
  mounted() {
    this.loadDefaultPartition()
  },
  methods: {
    async loadDefaultPartition() {
      try {
        const result = await analyzeCommand('mounted')
        if (result.success && result.data?.data?.mounted) {
          const mounted = result.data.data.mounted
          if (mounted.length > 0) {
            this.activePartition = {
              id: mounted[0].id || '',
              name: mounted[0].name || '',
              disk: mounted[0].disk || '',
              status: '1'
            }
          }
        }
      } catch (error) {
        console.error('Error cargando partición por defecto:', error)
      }
    },
    handlePartitionSelected(partition) {
      this.activePartition = {
        id: partition.id || '',
        name: partition.name || '',
        disk: partition.disk || '',
        status: partition.status || '0'
      }
    },
    handleCommandExecuted(result) {
      if (!result || !result._batch) {
        this.refreshAll()
      }
    },
    handleBatchCompleted() {
      this.refreshAll()
    },
    refreshAll() {
      this.$refs.systemSummary?.refresh?.()
      this.$refs.diskList?.refresh?.()
      this.$refs.fileExplorer?.refresh?.()
    }
  }
}
</script>

<style scoped>
.home-view {
  display: flex;
  flex-direction: column;
  gap: 10px;
  height: 100%;
}

/* ✅ NUEVO: Banner de sesión */
.session-banner {
  background: rgba(210, 153, 34, 0.1);
  border: 1px solid #d29922;
  border-radius: 8px;
  padding: 10px 16px;
  flex-shrink: 0;
}

.session-banner.active {
  background: rgba(63, 185, 80, 0.1);
  border-color: #3fb950;
}

.banner-content {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}

.banner-icon {
  font-size: 18px;
  flex-shrink: 0;
}

.banner-text {
  flex: 1;
  font-size: 13px;
  color: #e6edf3;
}

.banner-text strong {
  color: #58a6ff;
}

.banner-btn {
  padding: 6px 16px;
  background: #58a6ff;
  color: #0d1117;
  border-radius: 6px;
  text-decoration: none;
  font-weight: 600;
  font-size: 12px;
  transition: all 0.3s ease;
  flex-shrink: 0;
}

.banner-btn:hover {
  background: #79c0ff;
  text-decoration: none;
}

.terminal-section {
  flex: 0 0 auto;
  min-height: 350px;
  max-height: 55vh;
  overflow: hidden;
}

.dashboard-section {
  flex: 0 0 auto;
}

.panels-section {
  display: grid;
  grid-template-columns: 1fr 1fr 1fr;
  gap: 10px;
  flex: 0 0 auto;
  min-height: 140px;
  max-height: 220px;
}

.panel-item {
  min-height: 0;
  max-height: 100%;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

.panel-item > * {
  flex: 1;
  overflow-y: auto;
  min-height: 0;
}

@media (max-width: 1024px) {
  .panels-section {
    grid-template-columns: 1fr 1fr;
    max-height: 260px;
  }
}

@media (max-width: 768px) {
  .panels-section {
    grid-template-columns: 1fr 1fr;
    max-height: 280px;
  }
}

@media (max-width: 480px) {
  .panels-section {
    grid-template-columns: 1fr;
    max-height: 350px;
  }
}
</style>