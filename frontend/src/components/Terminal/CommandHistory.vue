<template>
  <div class="command-history">
    <div class="history-header">
      <h3>📋 Historial de Comandos</h3>
      <div class="history-actions">
        <button class="btn-filter" @click="filterType = 'all'" :class="{ active: filterType === 'all' }">
          Todos ({{ allCount }})
        </button>
        <button class="btn-filter" @click="filterType = 'success'" :class="{ active: filterType === 'success' }">
          ✅ Éxitos ({{ successCount }})
        </button>
        <button class="btn-filter" @click="filterType = 'error'" :class="{ active: filterType === 'error' }">
          ❌ Errores ({{ errorCount }})
        </button>
        <button class="btn-clear-history" @click="clearHistory" title="Limpiar historial">
          🗑
        </button>
      </div>
    </div>

    <div v-if="filteredHistory.length === 0" class="empty-history">
      <span class="empty-icon">📭</span>
      <p>No hay comandos en el historial</p>
      <span class="empty-hint">Ejecuta comandos en la terminal para verlos aquí</span>
    </div>

    <div v-else class="history-list">
      <div 
        v-for="(item, index) in filteredHistory" 
        :key="index" 
        class="history-item"
        :class="{ 'history-success': item.success, 'history-error': !item.success }"
      >
        <div class="history-index">#{{ index + 1 }}</div>
        <div class="history-command">{{ item.command }}</div>
        <div class="history-status">
          <span :class="item.success ? 'status-success' : 'status-error'">
            {{ item.success ? '✅' : '❌' }}
          </span>
        </div>
        <div class="history-message">{{ item.message }}</div>
        <div class="history-time">{{ item.time }}</div>
        <button class="btn-rerun" @click="rerunCommand(item.command)" title="Re-ejecutar">
          ▶
        </button>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'CommandHistory',
  data() {
    return {
      filterType: 'all',
      history: [
        { command: 'mkdisk -size=20 -unit=M -path=/tmp/disco1.mia', success: true, message: 'Disco creado exitosamente', time: '14:23:15' },
        { command: 'fdisk -size=10 -unit=M -path=/tmp/disco1.mia -name=Part1', success: true, message: 'Partición creada exitosamente', time: '14:23:20' },
        { command: 'mount -path=/tmp/disco1.mia -name=Part1', success: true, message: 'Partición montada exitosamente. ID: 811A', time: '14:23:25' },
        { command: 'mkfs -id=811A -type=full', success: true, message: 'Partición formateada exitosamente como EXT2', time: '14:23:30' },
        { command: 'login -user=user1 -pass=1234 -id=811A', success: false, message: 'Error: Usuario no encontrado: user1', time: '14:23:35' },
        { command: 'mkgrp -name=usuarios', success: true, message: 'Grupo creado exitosamente: usuarios', time: '14:23:40' },
        { command: 'mkusr -user=user1 -pass=1234 -grp=usuarios', success: true, message: 'Usuario creado exitosamente: user1', time: '14:23:45' }
      ]
    }
  },
  computed: {
    allCount() {
      return this.history.length
    },
    successCount() {
      return this.history.filter(h => h.success).length
    },
    errorCount() {
      return this.history.filter(h => !h.success).length
    },
    filteredHistory() {
      if (this.filterType === 'success') {
        return this.history.filter(h => h.success)
      } else if (this.filterType === 'error') {
        return this.history.filter(h => !h.success)
      }
      return this.history
    }
  },
  methods: {
    clearHistory() {
      this.history = []
    },
    rerunCommand(command) {
      console.log('▶ Re-ejecutando:', command)
      this.$emit('rerun-command', command)
    },
    addCommand(command, success, message) {
      const now = new Date()
      const time = now.toTimeString().slice(0, 8)
      this.history.unshift({
        command: command,
        success: success,
        message: message,
        time: time
      })
    }
  }
}
</script>

<style scoped>
.command-history {
  background: #161b22;
  border-radius: 12px;
  border: 1px solid #30363d;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  max-height: 320px;
}

.history-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 10px 16px;
  border-bottom: 1px solid #30363d;
  flex-wrap: wrap;
  gap: 8px;
}

.history-header h3 {
  margin: 0;
  font-size: 14px;
  font-weight: 600;
  color: #e6edf3;
}

.history-actions {
  display: flex;
  align-items: center;
  gap: 4px;
  flex-wrap: wrap;
}

.btn-filter {
  background: transparent;
  border: 1px solid #30363d;
  color: #8b949e;
  padding: 2px 10px;
  border-radius: 4px;
  font-size: 11px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-filter:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.btn-filter.active {
  background: #58a6ff;
  color: #0d1117;
  border-color: #58a6ff;
}

.btn-clear-history {
  background: transparent;
  border: none;
  color: #8b949e;
  cursor: pointer;
  font-size: 14px;
  padding: 2px 6px;
  border-radius: 4px;
  transition: all 0.3s ease;
}

.btn-clear-history:hover {
  color: #f85149;
  background: rgba(248, 81, 73, 0.1);
}

.empty-history {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 30px 20px;
  color: #8b949e;
  text-align: center;
}

.empty-icon {
  font-size: 32px;
  opacity: 0.4;
  margin-bottom: 8px;
}

.empty-history p {
  margin: 0;
  font-size: 14px;
  color: #e6edf3;
}

.empty-hint {
  font-size: 12px;
  color: #8b949e;
  margin-top: 4px;
}

.history-list {
  flex: 1;
  overflow-y: auto;
  padding: 4px 0;
}

.history-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 6px 14px;
  border-bottom: 1px solid #21262d;
  font-size: 12px;
  transition: background 0.15s ease;
}

.history-item:hover {
  background: #21262d;
}

.history-item.history-success {
  border-left: 3px solid #3fb950;
}

.history-item.history-error {
  border-left: 3px solid #f85149;
}

.history-index {
  color: #8b949e;
  font-size: 10px;
  min-width: 30px;
  font-weight: 600;
}

.history-command {
  flex: 1;
  color: #e6edf3;
  font-family: 'Courier New', monospace;
  font-size: 12px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.history-status {
  font-size: 14px;
}

.history-message {
  color: #8b949e;
  font-size: 11px;
  flex: 0.7;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.history-time {
  color: #30363d;
  font-size: 10px;
  min-width: 60px;
  text-align: right;
}

.btn-rerun {
  background: transparent;
  border: none;
  color: #8b949e;
  cursor: pointer;
  font-size: 12px;
  padding: 2px 6px;
  border-radius: 4px;
  transition: all 0.3s ease;
}

.btn-rerun:hover {
  color: #58a6ff;
  background: rgba(88, 166, 255, 0.1);
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

@media (max-width: 600px) {
  .command-history {
    max-height: 250px;
  }
  .history-item {
    flex-wrap: wrap;
    gap: 4px;
    padding: 4px 10px;
  }
  .history-command {
    flex-basis: 100%;
    white-space: normal;
  }
  .history-message {
    flex-basis: 100%;
    white-space: normal;
  }
  .history-index, .history-time {
    font-size: 9px;
  }
  .history-actions {
    gap: 2px;
  }
  .btn-filter {
    font-size: 9px;
    padding: 1px 6px;
  }
}
</style>