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

    <!-- PASO 1: ID -->
    <div class="step-box">
      <div class="step-header">
        <span class="step-num">1</span>
        <span class="step-title">Especificar partición</span>
      </div>
      <div class="step-content">
        <input
          v-model="mountId"
          type="text"
          placeholder="ID Partición (ej: 811A)"
          class="input-id"
          :disabled="executed"
        />
      </div>
    </div>

    <!-- PASO 2: Reportes ANTES -->
    <div class="step-box" :class="{ 'disabled': !mountId || executed }">
      <div class="step-header">
        <span class="step-num">2</span>
        <span class="step-title">Generar reportes ANTES del LOSS</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Genera los reportes de bitmaps para comparar después.</p>
        <button
          class="btn-report"
          :disabled="!mountId || loadingReports || executed"
          @click="generateReportsBefore"
        >
          {{ loadingReports ? 'Generando...' : '📊 Generar reportes ANTES' }}
        </button>

        <div v-if="reportsBefore.length > 0" class="reports-list">
          <div v-for="report in reportsBefore" :key="report.path" class="report-item">
            <span class="report-icon">✅</span>
            <div class="report-info">
              <span class="report-name">{{ report.name }}</span>
              <span class="report-path">{{ report.path }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- PASO 3: Ejecutar LOSS -->
    <div class="step-box danger" :class="{ 'disabled': !mountId || executed }">
      <div class="step-header">
        <span class="step-num">3</span>
        <span class="step-title">Ejecutar LOSS</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Esta acción es irreversible.</p>
        <button
          class="btn-loss"
          :disabled="!mountId || loading || executed"
          @click="executeLoss"
        >
          {{ loading ? 'Ejecutando...' : '💥 Ejecutar LOSS' }}
        </button>

        <div v-if="message" class="result-box" :class="success ? 'success' : 'error'">
          {{ message }}
        </div>
      </div>
    </div>

    <!-- PASO 4: Reportes DESPUÉS -->
    <div class="step-box" :class="{ 'disabled': !executed }">
      <div class="step-header">
        <span class="step-num">4</span>
        <span class="step-title">Generar reportes DESPUÉS del LOSS</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Compara con los reportes generados antes.</p>
        <button
          class="btn-report"
          :disabled="!executed || loadingReports"
          @click="generateReportsAfter"
        >
          {{ loadingReports ? 'Generando...' : '📊 Generar reportes DESPUÉS' }}
        </button>

        <div v-if="reportsAfter.length > 0" class="reports-list">
          <div v-for="report in reportsAfter" :key="report.path" class="report-item">
            <span class="report-icon">✅</span>
            <div class="report-info">
              <span class="report-name">{{ report.name }}</span>
              <span class="report-path">{{ report.path }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- PASO 5: Comparación -->
    <div v-if="reportsBefore.length > 0 && reportsAfter.length > 0" class="step-box success">
      <div class="step-header">
        <span class="step-num">✓</span>
        <span class="step-title">Comparación completa</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Abre los reportes para ver la diferencia. Los bitmaps deberían estar vacíos después del LOSS.</p>
        <button class="btn-reset" @click="reset">🔄 Realizar otra prueba</button>
      </div>
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
      loadingReports: false,
      message: null,
      success: false,
      executed: false,
      reportsBefore: [],
      reportsAfter: []
    }
  },
  methods: {
    async generateReportsBefore() {
      if (!this.mountId) return
      this.loadingReports = true
      this.reportsBefore = []

      const timestamp = Date.now()

      // ✅ CAMBIO: Rutas relativas (el backend las resuelve contra EXT2_REPORTS_DIR)
      const reports = [
        { name: 'bm_inode', file: `loss_antes_bm_inode_${timestamp}.txt` },
        { name: 'bm_block', file: `loss_antes_bm_block_${timestamp}.txt` },
        { name: 'inode', file: `loss_antes_inode_${timestamp}.jpg` },
        { name: 'block', file: `loss_antes_block_${timestamp}.jpg` }
      ]

      for (const r of reports) {
        // ✅ CAMBIO: Enviar solo el nombre del archivo, no la ruta absoluta
        const result = await analyzeCommand(`rep -id=${this.mountId} -path=${r.file} -name=${r.name}`)
        if (result.success) {
          // ✅ Mostrar la ruta resuelta que devuelve el backend
          const resolvedPath = result.data?.data?.report?.path || r.file
          this.reportsBefore.push({ name: r.name, path: resolvedPath })
        }
      }

      this.loadingReports = false
    },

    async generateReportsAfter() {
      if (!this.mountId) return
      this.loadingReports = true
      this.reportsAfter = []

      const timestamp = Date.now()

      // ✅ CAMBIO: Rutas relativas
      const reports = [
        { name: 'bm_inode', file: `loss_despues_bm_inode_${timestamp}.txt` },
        { name: 'bm_block', file: `loss_despues_bm_block_${timestamp}.txt` },
        { name: 'inode', file: `loss_despues_inode_${timestamp}.jpg` },
        { name: 'block', file: `loss_despues_block_${timestamp}.jpg` }
      ]

      for (const r of reports) {
        // ✅ CAMBIO: Enviar solo el nombre del archivo
        const result = await analyzeCommand(`rep -id=${this.mountId} -path=${r.file} -name=${r.name}`)
        if (result.success) {
          const resolvedPath = result.data?.data?.report?.path || r.file
          this.reportsAfter.push({ name: r.name, path: resolvedPath })
        }
      }

      this.loadingReports = false
    },

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
    },

    reset() {
      this.mountId = ''
      this.message = null
      this.success = false
      this.executed = false
      this.reportsBefore = []
      this.reportsAfter = []
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

.warning-box p strong {
  display: inline;
  color: #d29922;
}

/* Steps */
.step-box {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  margin-bottom: 16px;
  overflow: hidden;
  transition: all 0.3s ease;
}

.step-box.disabled {
  opacity: 0.5;
}

.step-box.danger {
  border-color: rgba(248, 81, 73, 0.3);
}

.step-box.success {
  border-color: rgba(63, 185, 80, 0.3);
  background: rgba(63, 185, 80, 0.05);
}

.step-header {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  background: #0d1117;
  border-bottom: 1px solid #30363d;
}

.step-box.danger .step-header {
  background: rgba(248, 81, 73, 0.1);
}

.step-box.success .step-header {
  background: rgba(63, 185, 80, 0.1);
}

.step-num {
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: #58a6ff;
  color: #0d1117;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 700;
  font-size: 12px;
  flex-shrink: 0;
}

.step-box.danger .step-num {
  background: #f85149;
  color: #ffffff;
}

.step-box.success .step-num {
  background: #3fb950;
  color: #ffffff;
}

.step-title {
  font-size: 14px;
  font-weight: 600;
  color: #e6edf3;
}

.step-content {
  padding: 16px;
}

.step-hint {
  font-size: 12px;
  color: #8b949e;
  margin: 0 0 12px 0;
}

.input-id {
  width: 100%;
  padding: 10px 14px;
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #e6edf3;
  font-size: 14px;
}

.input-id:focus {
  outline: none;
  border-color: #58a6ff;
}

.input-id:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-report,
.btn-loss,
.btn-reset {
  padding: 10px 20px;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
  font-size: 13px;
}

.btn-report {
  background: #58a6ff;
  color: #0d1117;
}

.btn-report:hover:not(:disabled) {
  background: #79c0ff;
}

.btn-loss {
  background: #f85149;
  color: #ffffff;
}

.btn-loss:hover:not(:disabled) {
  background: #ff6b64;
}

.btn-reset {
  background: transparent;
  border: 1px solid #30363d;
  color: #8b949e;
}

.btn-reset:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.btn-report:disabled,
.btn-loss:disabled,
.btn-reset:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.reports-list {
  margin-top: 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.report-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 12px;
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 6px;
}

.report-icon {
  font-size: 14px;
  flex-shrink: 0;
}

.report-info {
  display: flex;
  flex-direction: column;
  gap: 2px;
  flex: 1;
  min-width: 0;
}

.report-name {
  font-size: 12px;
  font-weight: 600;
  color: #e6edf3;
}

.report-path {
  font-size: 10px;
  color: #484f58;
  font-family: 'Courier New', monospace;
  word-break: break-all;
}

.result-box {
  padding: 12px 16px;
  border-radius: 6px;
  font-size: 13px;
  margin-top: 12px;
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
</style>