<template>
  <div class="loss-view">
    <div class="view-header">
      <h1>💥 Simulate System Loss</h1>
      <p>Simula una pérdida del sistema de archivos EXT3</p>
    </div>

    <div class="warning-box">
      <strong>⚠️ ADVERTENCIA</strong>
      <p>Esta operación <strong>corromperá</strong> los bitmaps, inodos y bloques de la partición. Los datos serán irrecuperables.</p>
      <p>Solo funciona en particiones formateadas como <strong>EXT3</strong>.</p>
    </div>

    <div class="controls">
      <input
        v-model="mountId"
        type="text"
        placeholder="ID Partición (ej: 811A)"
        class="input-id"
      />
      <button class="btn-loss" :disabled="loading || !mountId || executed" @click="executeLoss">
        {{ loading ? 'Ejecutando...' : 'Ejecutar LOSS' }}
      </button>
    </div>

    <div v-if="message" class="result-box" :class="success ? 'success' : 'error'">
      {{ message }}
    </div>

    <div v-if="executed && success" class="reports-section">
      <h3>📊 Reportes</h3>
      <p>Genere los reportes de bitmap antes y después del LOSS desde la terminal con:</p>
      <pre>rep -id={{ mountId }} -path=/ruta/antes.jpg -name=bm_inode
rep -id={{ mountId }} -path=/ruta/despues.jpg -name=bm_inode</pre>
    </div>
  </div>
</template>

<script>
import { analyzeCommand } from '../services/api.js'

export default {
  name: 'LossView',
  data() {
    return {
      mountId: '',
      loading: false,
      message: null,
      success: false,
      executed: false
    }
  },
  methods: {
    async executeLoss() {
      if (!confirm('¿Está seguro? Esta operación corromperá el sistema de archivos.')) {
        return
      }

      this.loading = true
      this.message = null

      try {
        const result = await analyzeCommand(`loss -id=${this.mountId}`)
        if (result.success && result.data?.data?.loss) {
          this.success = true
          this.message = result.data.data.message || 'LOSS ejecutado exitosamente'
          this.executed = true
        } else {
          this.success = false
          this.message = result.data?.message || result.error || 'Error al ejecutar LOSS'
        }
      } catch (err) {
        this.success = false
        this.message = 'Error de conexión: ' + err.message
      }
      this.loading = false
    }
  }
}
</script>

<style scoped>
.loss-view {
  padding: 20px;
  max-width: 800px;
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

.warning-box {
  background: rgba(210, 153, 34, 0.1);
  border: 1px solid #d29922;
  border-radius: 8px;
  padding: 16px 20px;
  margin-bottom: 24px;
  color: #d29922;
}

.warning-box strong {
  display: block;
  margin-bottom: 8px;
  font-size: 14px;
}

.warning-box p {
  margin: 4px 0;
  font-size: 13px;
  color: #e6edf3;
}

.controls {
  display: flex;
  gap: 12px;
  margin-bottom: 20px;
  justify-content: center;
}

.input-id {
  padding: 12px 16px;
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

.btn-loss {
  padding: 12px 24px;
  background: #f85149;
  color: #ffffff;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-loss:hover:not(:disabled) {
  background: #ff6b64;
}

.btn-loss:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.result-box {
  padding: 16px 20px;
  border-radius: 8px;
  font-size: 14px;
  margin-bottom: 20px;
}

.result-box.success {
  background: rgba(63, 185, 80, 0.1);
  border: 1px solid #3fb950;
  color: #3fb950;
}

.result-box.error {
  background: rgba(248, 81, 73, 0.1);
  border: 1px solid #f85149;
  color: #f85149;
}

.reports-section {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 20px;
}

.reports-section h3 {
  margin: 0 0 12px 0;
  color: #e6edf3;
  font-size: 16px;
}

.reports-section p {
  color: #8b949e;
  font-size: 13px;
  margin-bottom: 12px;
}

.reports-section pre {
  background: #0d1117;
  padding: 12px;
  border-radius: 6px;
  border: 1px solid #30363d;
  font-family: 'Courier New', monospace;
  font-size: 12px;
  color: #a6e3a1;
  overflow-x: auto;
}
</style>