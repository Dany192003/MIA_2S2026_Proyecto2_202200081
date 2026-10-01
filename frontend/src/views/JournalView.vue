<template>
  <div class="journal-view">
    <div class="view-header">
      <h1>📓 Journaling</h1>
      <p>Bitácora de transacciones de la partición</p>
    </div>

    <div class="controls">
      <input
        v-model="mountId"
        type="text"
        placeholder="ID Partición (ej: 811A)"
        class="input-id"
        @keyup.enter="loadJournal"
      />
      <button class="btn-load" :disabled="loading || !mountId" @click="loadJournal">
        {{ loading ? 'Cargando...' : 'Cargar Journal' }}
      </button>
    </div>

    <div v-if="error" class="error-state">{{ error }}</div>

    <div v-if="entries.length > 0" class="journal-table">
      <table>
        <thead>
          <tr>
            <th>#</th>
            <th>Operación</th>
            <th>Path</th>
            <th>Contenido</th>
            <th>Fecha</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="entry in entries" :key="entry.count">
            <td class="td-count">{{ entry.count }}</td>
            <td class="td-op">
              <span class="op-badge" :class="'op-' + entry.operation">
                {{ entry.operation }}
              </span>
            </td>
            <td class="td-path">{{ entry.path }}</td>
            <td class="td-content">{{ entry.content }}</td>
            <td class="td-date">{{ entry.date }}</td>
          </tr>
        </tbody>
      </table>
    </div>

    <div v-else-if="!loading && !error" class="empty-state">
      <p>No hay transacciones registradas</p>
      <small>Realice operaciones (mkdir, mkfile, etc.) y vuelva a cargar</small>
    </div>
  </div>
</template>

<script>
import { analyzeCommand } from '../services/api.js'

export default {
  name: 'JournalView',
  data() {
    return {
      mountId: '',
      entries: [],
      loading: false,
      error: null
    }
  },
  mounted() {
    // Intentar cargar la sesión actual
    const session = this.$store?.state
    if (session?.mountId) {
      this.mountId = session.mountId
      this.loadJournal()
    }
  },
  methods: {
    async loadJournal() {
      if (!this.mountId) return
      this.loading = true
      this.error = null
      this.entries = []

      try {
        const result = await analyzeCommand(`journaling -id=${this.mountId}`)
        if (result.success && result.data?.data?.journaling) {
          this.entries = result.data.data.journaling.entries || []
        } else {
          this.error = result.data?.message || 'Error al cargar journal'
        }
      } catch (err) {
        this.error = 'Error de conexión: ' + err.message
      }
      this.loading = false
    }
  }
}
</script>

<style scoped>
.journal-view {
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

.controls {
  display: flex;
  gap: 12px;
  margin-bottom: 20px;
  justify-content: center;
}

.input-id {
  padding: 10px 16px;
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #e6edf3;
  font-size: 14px;
  width: 250px;
}

.input-id:focus {
  outline: none;
  border-color: #58a6ff;
}

.btn-load {
  padding: 10px 24px;
  background: #58a6ff;
  color: #0d1117;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-load:hover:not(:disabled) {
  background: #79c0ff;
}

.btn-load:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.journal-table {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  overflow: hidden;
}

table {
  width: 100%;
  border-collapse: collapse;
}

th {
  background: #0d1117;
  padding: 12px 16px;
  font-size: 11px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  text-align: left;
  border-bottom: 1px solid #30363d;
}

td {
  padding: 10px 16px;
  font-size: 13px;
  color: #e6edf3;
  border-bottom: 1px solid #21262d;
}

tr:last-child td {
  border-bottom: none;
}

tr:hover td {
  background: #1c2128;
}

.td-count {
  color: #8b949e;
  font-family: monospace;
  text-align: center;
  width: 50px;
}

.td-op {
  width: 120px;
}

.op-badge {
  display: inline-block;
  padding: 2px 10px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
}

.op-mkdir { background: rgba(88, 166, 255, 0.15); color: #58a6ff; }
.op-mkfile { background: rgba(63, 185, 80, 0.15); color: #3fb950; }
.op-remove { background: rgba(248, 81, 73, 0.15); color: #f85149; }
.op-rename { background: rgba(210, 153, 34, 0.15); color: #d29922; }
.op-copy { background: rgba(163, 113, 247, 0.15); color: #a371f7; }
.op-move { background: rgba(219, 109, 40, 0.15); color: #db6d28; }
.op-chown { background: rgba(236, 72, 153, 0.15); color: #ec4899; }

.td-path {
  font-family: 'Courier New', monospace;
  color: #58a6ff;
  word-break: break-all;
}

.td-content {
  color: #8b949e;
  font-size: 12px;
  max-width: 300px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.td-date {
  color: #8b949e;
  font-size: 12px;
  white-space: nowrap;
}

.error-state {
  text-align: center;
  padding: 40px;
  color: #f85149;
}

.empty-state {
  text-align: center;
  padding: 60px 20px;
  color: #8b949e;
}

.empty-state small {
  display: block;
  margin-top: 8px;
  color: #484f58;
}
</style>