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

    <!-- PASO 2: Ejecutar LOSS -->
    <div class="step-box danger" :class="{ 'disabled': !mountId || executed }">
      <div class="step-header">
        <span class="step-num">2</span>
        <span class="step-title">Ejecutar LOSS</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Esta acción es irreversible. Los reportes se generarán automáticamente antes y después.</p>
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

    <!-- PASO 3: Reportes ANTES -->
    <div v-if="reportsAntes.length > 0" class="step-box">
      <div class="step-header">
        <span class="step-num">3</span>
        <span class="step-title">Reportes ANTES del LOSS</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Estado del sistema de archivos antes del fallo.</p>
        <div class="reports-list">
          <div v-for="report in reportsAntes" :key="report.path" class="report-item">
            <span class="report-icon">{{ report.name.includes('bm_') ? '📄' : '🖼️' }}</span>
            <div class="report-info">
              <span class="report-name">{{ report.name }}</span>
              <span class="report-path">{{ report.path }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- PASO 4: Reportes DESPUÉS -->
    <div v-if="reportsDespues.length > 0" class="step-box success">
      <div class="step-header">
        <span class="step-num">4</span>
        <span class="step-title">Reportes DESPUÉS del LOSS</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Estado del sistema de archivos después del fallo. Los bitmaps deberían estar vacíos.</p>
        <div class="reports-list">
          <div v-for="report in reportsDespues" :key="report.path" class="report-item">
            <span class="report-icon">{{ report.name.includes('bm_') ? '📄' : '🖼️' }}</span>
            <div class="report-info">
              <span class="report-name">{{ report.name }}</span>
              <span class="report-path">{{ report.path }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- PASO 5: Comparación -->
    <div v-if="reportsAntes.length > 0 && reportsDespues.length > 0" class="step-box success">
      <div class="step-header">
        <span class="step-num">✓</span>
        <span class="step-title">Comparación completa</span>
      </div>
      <div class="step-content">
        <p class="step-hint">Compara los reportes. Los bitmaps ANTES deben tener bits en 1; los DESPUÉS deben estar en 0.</p>
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
      message: null,
      success: false,
      executed: false,
      reportsAntes: [],
      reportsDespues: []
    }
  },
  methods: {
    async executeLoss() {
      if (!confirm('¿Está seguro? Esta operación corromperá el sistema de archivos.')) {
        return
      }

      this.loading = true
      this.message = null
      this.reportsAntes = []
      this.reportsDespues = []

      try {
        const result = await analyzeCommand(`loss -id=${this.mountId}`)
        
        if (result.success && result.data?.data?.loss) {
          const lossData = result.data.data.loss
          this.success = true
          this.message = result.data.message || 'LOSS ejecutado exitosamente'
          this.executed = true
          
          // ✅ Extraer reportes antes/después del backend
          if (lossData.reports) {
            this.reportsAntes = lossData.reports.antes || []
            this.reportsDespues = lossData.reports.despues || []
          }
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
      this.reportsAntes = []
      this.reportsDespues = []
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