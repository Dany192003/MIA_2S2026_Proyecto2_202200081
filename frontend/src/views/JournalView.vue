<template>
  <div class="journal-view">
    <div class="view-header">
      <h1>📓 Journaling</h1>
      <p>Bitácora de transacciones de la partición EXT3</p>
    </div>

    <!-- Controles -->
    <div class="controls">
      <input
        v-model="mountId"
        type="text"
        placeholder="ID Partición (ej: 811A)"
        class="input-id"
        @keyup.enter="loadJournal"
      />
      <button class="btn-load" :disabled="loading || !mountId" @click="loadJournal">
        {{ loading ? 'Cargando...' : '🔄 Cargar Journal' }}
      </button>
    </div>

    <!-- Error -->
    <div v-if="error" class="error-state">
      <span class="error-icon">⚠️</span>
      <span class="error-text">{{ error }}</span>
    </div>

    <!-- Filtros -->
    <div v-if="entries.length > 0" class="filters">
      <button
        class="filter-btn"
        :class="{ active: activeFilter === 'all' }"
        @click="activeFilter = 'all'"
      >
        Todas ({{ entries.length }})
      </button>
      <button
        v-for="op in availableOperations"
        :key="op"
        class="filter-btn"
        :class="['op-' + op, { active: activeFilter === op }]"
        @click="activeFilter = op"
      >
        {{ op }} ({{ countByOperation(op) }})
      </button>
    </div>

    <!-- Tabla -->
    <div v-if="filteredEntries.length > 0" class="journal-table">
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
          <tr v-for="entry in filteredEntries" :key="entry.count">
            <td class="td-count">{{ entry.count }}</td>
            <td class="td-op">
              <span class="op-badge" :class="'op-' + entry.operation">
                {{ entry.operation }}
              </span>
            </td>
            <td class="td-path">{{ entry.path }}</td>
            <td class="td-content" :title="entry.content">{{ entry.content }}</td>
            <td class="td-date">{{ entry.date }}</td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Sin resultados -->
    <div v-else-if="entries.length > 0 && filteredEntries.length === 0" class="empty-state">
      <p>No hay transacciones con la operación "{{ activeFilter }}"</p>
      <button class="btn-reset-filter" @click="activeFilter = 'all'">Ver todas</button>
    </div>

    <!-- Sin datos -->
    <div v-else-if="!loading && !error && hasLoaded" class="empty-state">
      <span class="empty-icon">📭</span>
      <p>No hay transacciones registradas</p>
      <small>Realice operaciones (mkdir, mkfile, etc.) y vuelva a cargar</small>
    </div>

    <!-- Estado inicial -->
    <div v-else-if="!loading && !error && !hasLoaded" class="empty-state">
      <span class="empty-icon">📓</span>
      <p>Ingrese un ID de partición y presione "Cargar Journal"</p>
      <small>Solo funciona en particiones formateadas como EXT3</small>
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
      error: null,
      activeFilter: 'all',
      hasLoaded: false
    }
  },
  computed: {
    availableOperations() {
      const ops = new Set()
      for (const entry of this.entries) {
        ops.add(entry.operation)
      }
      return Array.from(ops).sort()
    },
    filteredEntries() {
      if (this.activeFilter === 'all') return this.entries
      return this.entries.filter(e => e.operation === this.activeFilter)
    }
  },
  mounted() {
    const session = this.$store?.state
    if (session?.mountId) {
      this.mountId = session.mountId
      this.loadJournal()
    }
  },
  methods: {
    countByOperation(op) {
      return this.entries.filter(e => e.operation === op).length
    },

    async loadJournal() {
      if (!this.mountId) return
      this.loading = true
      this.error = null
      this.entries = []
      this.activeFilter = 'all'

      try {
        const result = await analyzeCommand(`journaling -id=${this.mountId}`)
        if (result.success && result.data?.data?.journaling) {
          this.entries = result.data.data.journaling.entries || []
          this.hasLoaded = true
        } else {
          this.error = result.data?.message || 'Error al cargar journal'
          this.hasLoaded = true
        }
      } catch (err) {
        this.error = 'Error de conexión: ' + err.message
        this.hasLoaded = true
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

/* ✅ NUEVO: Filtros */
.filters {
  display: flex;
  gap: 6px;
  margin-bottom: 16px;
  flex-wrap: wrap;
  justify-content: center;
}

.filter-btn {
  padding: 6px 14px;
  background: transparent;
  border: 1px solid #30363d;
  border-radius: 20px;
  color: #8b949e;
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s ease;
}

.filter-btn:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.filter-btn.active {
  background: #58a6ff;
  border-color: #58a6ff;
  color: #0d1117;
  font-weight: 600;
}

.filter-btn.active.op-mkdir { background: #58a6ff; border-color: #58a6ff; }
.filter-btn.active.op-mkfile { background: #3fb950; border-color: #3fb950; color: #0d1117; }
.filter-btn.active.op-remove { background: #f85149; border-color: #f85149; color: #ffffff; }
.filter-btn.active.op-rename { background: #d29922; border-color: #d29922; color: #0d1117; }
.filter-btn.active.op-copy { background: #a371f7; border-color: #a371f7; color: #ffffff; }
.filter-btn.active.op-move { background: #db6d28; border-color: #db6d28; color: #ffffff; }
.filter-btn.active.op-chown { background: #ec4899; border-color: #ec4899; color: #ffffff; }

/* Tabla */
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
  cursor: help;
}

.td-date {
  color: #8b949e;
  font-size: 12px;
  white-space: nowrap;
}

.error-state {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  padding: 20px;
  background: rgba(248, 81, 73, 0.1);
  border: 1px solid #f85149;
  border-radius: 8px;
  color: #f85149;
  font-size: 13px;
  margin-bottom: 16px;
}

.empty-state {
  text-align: center;
  padding: 60px 20px;
  color: #8b949e;
}

.empty-state .empty-icon {
  font-size: 48px;
  display: block;
  margin-bottom: 12px;
  opacity: 0.5;
}

.empty-state small {
  display: block;
  margin-top: 8px;
  color: #484f58;
}

.btn-reset-filter {
  margin-top: 12px;
  padding: 6px 16px;
  background: transparent;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #8b949e;
  cursor: pointer;
  font-size: 12px;
}

.btn-reset-filter:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}
</style>