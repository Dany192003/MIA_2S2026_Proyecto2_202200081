<template>
  <div class="visualizer-view">
    <div class="view-header">
      <h1>📁 Visualizador del Sistema de Archivos</h1>
      <p v-if="!step">Seleccione el disco que desea visualizar</p>
      <p v-else-if="step === 'partition'">Seleccione la partición que desea visualizar</p>
    </div>

    <!-- ✅ NUEVO: Banner modo solo lectura -->
    <div class="readonly-banner">
      <span class="banner-icon">📖</span>
      <span class="banner-text">
        <strong>Modo solo lectura.</strong> Puede explorar discos, particiones, carpetas y archivos. Para crear o modificar, use los comandos desde el Home.
      </span>
    </div>

    <!-- PASO 1: Selección de disco -->
    <div v-if="!step" class="disk-grid">
      <div v-if="loading" class="loading-state">Cargando discos...</div>
      <div v-else-if="disks.length === 0" class="empty-state">
        <span class="empty-icon">💽</span>
        <p>No hay discos creados</p>
        <small>Use el comando <code>mkdisk</code> para crear uno</small>
      </div>
      <div
        v-for="disk in disks"
        :key="disk.path"
        class="disk-card"
        @click="selectDisk(disk)"
      >
        <div class="disk-icon">💽</div>
        <div class="disk-name">{{ disk.name }}</div>

        <div class="disk-info">
          <div class="info-row">
            <span class="info-label">💾 Capacidad:</span>
            <span class="info-value">{{ formatSize(disk.size) }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">⚙️ Fit:</span>
            <span class="info-value">{{ disk.fit }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">📊 Particiones:</span>
            <span class="info-value">{{ disk.partition_count }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">🔗 Montadas:</span>
            <span class="info-value" :class="{ 'has-mounted': disk.mounted_count > 0 }">
              {{ disk.mounted_count }}
            </span>
          </div>
          <div class="info-row">
            <span class="info-label">📅 Fecha:</span>
            <span class="info-value">{{ disk.date || '—' }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">🔑 Signature:</span>
            <span class="info-value mono">{{ disk.signature || '—' }}</span>
          </div>
        </div>

        <div class="disk-details">
          <small>{{ disk.path }}</small>
        </div>

        <div class="disk-action">
          <span class="action-text">Clic para ver particiones →</span>
        </div>
      </div>
    </div>

    <!-- PASO 2: Selección de partición -->
    <div v-else-if="step === 'partition'" class="partition-grid">
      <button class="btn-back" @click="goBack">← Volver a discos</button>
      <div class="selected-disk-info">
        <span>Disco seleccionado:</span>
        <strong>{{ selectedDisk.name }}</strong>
        <span class="disk-meta">({{ formatSize(selectedDisk.size) }} · {{ selectedDisk.fit }})</span>
      </div>

      <div v-if="selectedDisk.partitions.length === 0" class="empty-state">
        <span class="empty-icon">📂</span>
        <p>Este disco no tiene particiones</p>
        <small>Use <code>fdisk</code> desde el Home para crear una</small>
      </div>

      <div
        v-for="part in selectedDisk.partitions"
        :key="part.name"
        class="partition-card"
        :class="{
          'mounted': part.status === '1',
          'not-mounted': part.status !== '1'
        }"
        @click="selectPartition(part)"
      >
        <div class="partition-icon">
          {{ part.type === 'P' ? '🔵' : part.type === 'E' ? '🟡' : '🟢' }}
        </div>
        <div class="partition-name">{{ part.name }}</div>

        <div class="partition-info">
          <div class="info-row">
            <span class="info-label">📋 Tipo:</span>
            <span class="info-value">
              {{ part.type === 'P' ? 'Primaria' : part.type === 'E' ? 'Extendida' : 'Lógica' }}
            </span>
          </div>
          <div class="info-row">
            <span class="info-label">💾 Tamaño:</span>
            <span class="info-value">{{ formatSize(part.size) }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">⚙️ Fit:</span>
            <span class="info-value">{{ part.fit }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">🔌 Estado:</span>
            <span class="info-value" :class="part.status === '1' ? 'status-mounted' : 'status-free'">
              {{ part.status === '1' ? 'Montada' : 'No montada' }}
            </span>
          </div>
          <div v-if="part.status === '1' && part.id" class="info-row">
            <span class="info-label">🔗 ID:</span>
            <span class="info-value mono id-badge">{{ part.id }}</span>
          </div>
        </div>

        <div v-if="part.status === '1'" class="partition-action success">
          <span class="action-text">Clic para explorar archivos →</span>
        </div>
        <div v-else class="partition-action warning">
          <span class="action-text">⚠️ No montada — Use MOUNT</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { getDisks } from '../services/api.js'

export default {
  name: 'VisualizerView',
  data() {
    return {
      disks: [],
      loading: false,
      step: null,
      selectedDisk: null
    }
  },
  mounted() {
    this.loadDisks()
  },
  methods: {
    async loadDisks() {
      this.loading = true
      try {
        const result = await getDisks()
        if (result.success && result.data?.disks) {
          this.disks = result.data.disks
        }
      } catch (error) {
        console.error('Error cargando discos:', error)
      }
      this.loading = false
    },

    selectDisk(disk) {
      this.selectedDisk = disk
      this.step = 'partition'
    },

    goBack() {
      this.step = null
      this.selectedDisk = null
    },

    selectPartition(part) {
      if (part.status !== '1') {
        alert('La partición no está montada.\nUse el comando MOUNT primero desde el Home.')
        return
      }
      this.$router.push({
        path: '/files',
        query: { id: part.id }
      })
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
.visualizer-view {
  padding: 20px;
  max-width: 1200px;
  margin: 0 auto;
  width: 100%;
}

.view-header {
  text-align: center;
  margin-bottom: 20px;
}

.view-header h1 {
  font-size: 24px;
  font-weight: 700;
  color: #e6edf3;
  margin: 0 0 8px 0;
}

.view-header p {
  font-size: 14px;
  color: #8b949e;
  margin: 0;
}

/* ✅ NUEVO: Banner modo lectura */
.readonly-banner {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 10px 16px;
  background: rgba(88, 166, 255, 0.08);
  border: 1px solid rgba(88, 166, 255, 0.3);
  border-radius: 8px;
  margin-bottom: 24px;
  font-size: 13px;
  color: #e6edf3;
}

.readonly-banner .banner-icon {
  font-size: 18px;
  flex-shrink: 0;
}

.readonly-banner .banner-text strong {
  color: #58a6ff;
}

/* DISK GRID */
.disk-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 20px;
}

.disk-card {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 12px;
  padding: 20px;
  cursor: pointer;
  transition: all 0.3s ease;
  text-align: center;
}

.disk-card:hover {
  border-color: #58a6ff;
  transform: translateY(-4px);
  box-shadow: 0 10px 30px rgba(88, 166, 255, 0.1);
}

.disk-icon {
  font-size: 48px;
  margin-bottom: 12px;
}

.disk-name {
  font-size: 16px;
  font-weight: 600;
  color: #e6edf3;
  margin-bottom: 16px;
  word-break: break-all;
}

.disk-info {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-bottom: 16px;
  text-align: left;
}

.info-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 8px;
  font-size: 12px;
}

.info-label {
  color: #8b949e;
  flex-shrink: 0;
}

.info-value {
  color: #e6edf3;
  font-weight: 500;
  text-align: right;
  word-break: break-word;
}

.info-value.mono {
  font-family: 'Courier New', monospace;
  font-size: 11px;
  color: #8b949e;
}

.info-value.has-mounted {
  color: #3fb950;
  font-weight: 700;
}

.status-mounted {
  color: #3fb950;
  font-weight: 600;
}

.status-free {
  color: #8b949e;
}

.disk-details {
  font-size: 10px;
  color: #484f58;
  word-break: break-all;
  padding-top: 12px;
  border-top: 1px solid #21262d;
}

/* ✅ NUEVO: Acciones */
.disk-action,
.partition-action {
  margin-top: 12px;
  padding-top: 12px;
  border-top: 1px solid #21262d;
  font-size: 11px;
}

.disk-action .action-text {
  color: #58a6ff;
  font-weight: 600;
}

.partition-action.success .action-text {
  color: #3fb950;
  font-weight: 600;
}

.partition-action.warning .action-text {
  color: #d29922;
}

/* PARTITION GRID */
.partition-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 20px;
}

.selected-disk-info {
  grid-column: 1 / -1;
  padding: 14px 18px;
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  font-size: 14px;
  color: #8b949e;
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 8px;
}

.selected-disk-info strong {
  color: #e6edf3;
}

.disk-meta {
  font-size: 12px;
  color: #484f58;
}

.btn-back {
  grid-column: 1 / -1;
  width: fit-content;
  padding: 8px 16px;
  background: transparent;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #8b949e;
  cursor: pointer;
  transition: all 0.3s ease;
  margin-bottom: 8px;
  font-size: 13px;
}

.btn-back:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.partition-card {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 12px;
  padding: 20px;
  cursor: pointer;
  transition: all 0.3s ease;
  text-align: center;
}

.partition-card:hover {
  border-color: #58a6ff;
  transform: translateY(-4px);
}

.partition-card.mounted {
  border-color: #3fb950;
  background: linear-gradient(135deg, #161b22 0%, rgba(63, 185, 80, 0.05) 100%);
}

.partition-card.mounted:hover {
  box-shadow: 0 10px 30px rgba(63, 185, 80, 0.15);
}

.partition-card.not-mounted {
  opacity: 0.75;
}

.partition-card.not-mounted:hover {
  opacity: 1;
}

.partition-icon {
  font-size: 36px;
  margin-bottom: 12px;
}

.partition-name {
  font-size: 16px;
  font-weight: 600;
  color: #e6edf3;
  margin-bottom: 16px;
  word-break: break-all;
}

.partition-info {
  display: flex;
  flex-direction: column;
  gap: 8px;
  text-align: left;
}

.id-badge {
  background: rgba(88, 166, 255, 0.15);
  color: #58a6ff !important;
  padding: 2px 8px;
  border-radius: 4px;
  font-weight: 700;
}

/* ESTADOS */
.loading-state,
.empty-state {
  grid-column: 1 / -1;
  text-align: center;
  padding: 40px;
  color: #8b949e;
}

.empty-state .empty-icon {
  font-size: 48px;
  display: block;
  margin-bottom: 12px;
  opacity: 0.5;
}

.empty-state code {
  background: #0d1117;
  padding: 2px 6px;
  border-radius: 3px;
  border: 1px solid #30363d;
  font-family: monospace;
  color: #e6edf3;
}

.empty-state small {
  display: block;
  margin-top: 8px;
  color: #484f58;
}

@media (max-width: 480px) {
  .visualizer-view {
    padding: 12px;
  }
  .disk-grid,
  .partition-grid {
    grid-template-columns: 1fr;
  }
}
</style>