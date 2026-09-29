<template>
  <div id="app">
    <AppHeader />
    <main class="main-content">
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
    </main>
    <AppFooter />
  </div>
</template>

<script>
import AppHeader from './components/Layout/AppHeader.vue'
import AppFooter from './components/Layout/AppFooter.vue'
import SystemSummary from './components/Dashboard/SystemSummary.vue'
import DiskList from './components/Dashboard/DiskList.vue'
import FileExplorer from './components/Dashboard/FileExplorer.vue'
import ReportsPanel from './components/Dashboard/ReportsPanel.vue'
import CommandTerminal from './components/Terminal/CommandTerminal.vue'
import { analyzeCommand } from './services/api.js'

export default {
  name: 'App',
  components: {
    AppHeader,
    AppFooter,
    SystemSummary,
    DiskList,
    FileExplorer,
    ReportsPanel,
    CommandTerminal
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
      console.log('📀 Partición seleccionada:', this.activePartition)
    },
    handleCommandExecuted(result) {
      if (!result || !result._batch) {
        this.refreshAll()
      }
    },
    handleBatchCompleted(results) {
      this.refreshAll()
    },
    refreshAll() {
      this.$refs.systemSummary?.refresh()
      this.$refs.diskList?.refresh()
      this.$refs.fileExplorer?.refresh()
    }
  }
}
</script>

<style scoped>
* {
  box-sizing: border-box;
}

html, body {
  margin: 0;
  padding: 0;
  height: 100%;
  overflow: hidden;
  background: #0d1117;
}

#app {
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: #0d1117;
  color: #e6edf3;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  overflow: hidden;
}

.main-content {
  flex: 1;
  padding: 10px 16px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  overflow-y: auto;
  min-height: 0;
}

.terminal-section {
  flex: 0 0 auto;
  min-height: 350px;
  max-height: 55vh;
  overflow: hidden;
}

.terminal-section .command-terminal {
  height: 100%;
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

.app-footer {
  flex-shrink: 0;
}

@media (max-width: 1024px) {
  .panels-section {
    grid-template-columns: 1fr 1fr;
    max-height: 260px;
  }
  .terminal-section {
    min-height: 300px;
    max-height: 50vh;
  }
}

@media (max-width: 768px) {
  .main-content {
    padding: 8px 12px;
    gap: 8px;
  }
  .terminal-section {
    min-height: 250px;
    max-height: 45vh;
  }
  .panels-section {
    grid-template-columns: 1fr 1fr;
    max-height: 280px;
    min-height: 160px;
  }
}

@media (max-width: 480px) {
  .main-content {
    padding: 6px 8px;
    gap: 6px;
  }
  .terminal-section {
    min-height: 200px;
    max-height: 40vh;
  }
  .panels-section {
    grid-template-columns: 1fr;
    max-height: 350px;
    min-height: 180px;
  }
}
</style>