<template>
  <div class="disk-list">
    <div class="disk-header">
      <span class="disk-title">Discos</span>
      <span class="disk-count">{{ disks.length }}</span>
      <button class="btn-refresh" @click="refresh" title="Actualizar">↻</button>
    </div>

    <div v-if="loading" class="loading-state">
      <span>Cargando...</span>
    </div>

    <div v-else-if="disks.length === 0" class="empty-state">
      <p>Sin discos</p>
      <span class="hint">Usa mkdisk para crear uno</span>
    </div>

    <div v-else class="disk-items">
      <div v-for="disk in disks" :key="disk.path" class="disk-item">
        <div class="disk-info">
          <span class="disk-name">{{ disk.name }}</span>
          <span class="disk-size">{{ formatSize(disk.size) }}</span>
        </div>
        <div class="disk-path" :title="disk.path">{{ disk.path }}</div>
        <div class="disk-partitions" v-if="disk.partitions && disk.partitions.length > 0">
          <span 
            v-for="part in disk.partitions" 
            :key="disk.path + '-' + part.name" 
            class="partition-tag" 
            :class="{
              'tag-p': (part.type || '').toLowerCase() === 'p',
              'tag-e': (part.type || '').toLowerCase() === 'e',
              'tag-l': (part.type || '').toLowerCase() === 'l',
              'mounted': part.status === '1'
            }"
            @click="selectPartition(disk, part)"
            :title="`${part.name} | Tipo ${part.type} | ${formatSize(part.size)} | ${part.status === '1' ? 'Montada ID: ' + (part.id || '?') : 'No montada'}`"
          >
            <span class="part-name">{{ part.name }}</span>
            <span class="part-type">[{{ (part.type || '').toUpperCase() }}]</span>
            <span class="part-size">{{ formatSize(part.size) }}</span>
            <span v-if="part.status === '1' && part.id" class="part-id">🔗 {{ part.id }}</span>
            <span v-else-if="part.status === '1'" class="part-mount">🔗</span>
          </span>
        </div>
        <div v-else class="disk-no-partitions">
          <span class="partition-empty">Sin particiones</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { analyzeCommand } from '../../services/api.js'

export default {
  name: 'DiskList',
  emits: ['partition-selected'],
  data() {
    return {
      disks: [],
      loading: false
    }
  },
  mounted() {
    this.refresh()
  },
  methods: {
    async refresh() {
      this.loading = true
      try {
        const result = await analyzeCommand('lsdisk')
        
        if (result.success && result.data?.data?.disks) {
          this.disks = result.data.data.disks.map(disk => ({
            name: disk.name,
            path: disk.path,
            size: disk.size,
            partitions: disk.partitions || [],
            partition_count: disk.partition_count || 0
          }))
        } else {
          this.disks = []
          console.warn('lsdisk no devolvió discos:', result)
        }
      } catch (error) {
        console.error('Error actualizando lista de discos:', error)
        this.disks = []
      }
      this.loading = false
    },
    formatSize(bytes) {
      if (!bytes || bytes === 0) return '0 B'
      const k = 1024
      const sizes = ['B', 'KB', 'MB', 'GB']
      const i = Math.floor(Math.log(bytes) / Math.log(k))
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
    },
    selectPartition(disk, part) {
      // ✅ CORREGIDO: emitir la partición con el disco correcto y toda la info
      this.$emit('partition-selected', {
        id: part.id || '',
        name: part.name,
        disk: disk.path || '',
        status: part.status || '0',
        type: part.type || 'P',
        size: part.size || 0
      })
    }
  }
}
</script>

<style scoped>
.disk-list {
  background: #161b22;
  border-radius: 8px;
  border: 1px solid #30363d;
  padding: 8px 12px;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.disk-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
  flex-shrink: 0;
}

.disk-title {
  font-size: 10px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.disk-count {
  font-size: 10px;
  color: #58a6ff;
  background: rgba(88, 166, 255, 0.1);
  padding: 0 8px;
  border-radius: 8px;
  border: 1px solid rgba(88, 166, 255, 0.2);
  font-weight: 600;
  min-width: 24px;
  text-align: center;
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

.loading-state, .empty-state {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #8b949e;
  font-size: 12px;
}

.hint {
  font-size: 9px;
  color: #30363d;
  margin-top: 4px;
}

.disk-items {
  flex: 1;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.disk-item {
  background: #0d1117;
  border-radius: 6px;
  padding: 6px 8px;
  border: 1px solid #30363d;
  transition: border-color 0.2s ease;
}

.disk-item:hover {
  border-color: #58a6ff;
}

.disk-info {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 2px;
}

.disk-name {
  font-size: 11px;
  font-weight: 600;
  color: #e6edf3;
}

.disk-size {
  font-size: 9px;
  color: #8b949e;
  font-family: monospace;
}

.disk-path {
  font-size: 8px;
  color: #484f58;
  font-family: monospace;
  margin-bottom: 4px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.disk-partitions {
  display: flex;
  flex-wrap: wrap;
  gap: 3px;
}

.disk-no-partitions {
  padding: 2px 0;
}

.partition-tag {
  font-size: 8px;
  padding: 2px 6px;
  border-radius: 3px;
  background: #21262d;
  color: #8b949e;
  border: 1px solid #30363d;
  cursor: pointer;
  transition: all 0.2s ease;
  display: inline-flex;
  align-items: center;
  gap: 3px;
  white-space: nowrap;
}

.partition-tag:hover {
  border-color: #58a6ff;
  background: #1c2333;
  transform: scale(1.03);
}

.part-name {
  font-weight: 600;
  color: #e6edf3;
}

.part-type {
  font-size: 7px;
  font-weight: 700;
  opacity: 0.8;
}

.part-size {
  font-size: 7px;
  opacity: 0.7;
}

.part-id {
  font-size: 7px;
  font-weight: 700;
  color: #3fb950;
  background: rgba(63, 185, 80, 0.15);
  padding: 0 3px;
  border-radius: 2px;
  font-family: monospace;
}

.part-mount {
  font-size: 8px;
}

.partition-tag.tag-p {
  border-color: #58a6ff;
}

.partition-tag.tag-p .part-type {
  color: #58a6ff;
}

.partition-tag.tag-e {
  border-color: #d29922;
}

.partition-tag.tag-e .part-type {
  color: #d29922;
}

.partition-tag.tag-l {
  border-color: #3fb950;
}

.partition-tag.tag-l .part-type {
  color: #3fb950;
}

.partition-tag.mounted {
  border-color: #3fb950;
  background: rgba(63, 185, 80, 0.1);
}

.partition-empty {
  font-size: 8px;
  color: #484f58;
  font-style: italic;
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