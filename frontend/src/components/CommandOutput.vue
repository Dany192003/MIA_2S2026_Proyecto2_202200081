<template>
  <div class="command-output">
    <!-- Toolbar -->
    <div class="output-toolbar">
      <div class="toolbar-left">
        <span class="toolbar-icon">▶</span>
        <span class="toolbar-title">OUTPUT</span>
        <span class="toolbar-badge" :class="result?.success ? 'success' : 'error'">
          {{ result?.success ? 'OK' : 'ERR' }}
        </span>
      </div>
      <div class="toolbar-right">
        <button v-if="hasResults" @click="exportResults" class="btn-export">Export</button>
        <button @click="clearOutput" class="btn-clear">Clear</button>
      </div>
    </div>

    <!-- Contenido -->
    <div class="output-content">
      <div v-if="!result && batchResults.length === 0" class="empty-state">
        <span class="empty-icon">⏎</span>
        <span class="empty-text">Waiting for command...</span>
        <span class="empty-hint">Enter a command in the terminal panel</span>
      </div>

      <div v-else class="output-scroll">
        <!-- Resultado individual -->
        <div v-if="result && !result._batch" class="result-item">
          <div class="result-header">
            <span class="result-status" :class="result.success ? 'status-ok' : 'status-err'">
              {{ result.success ? '✔' : '✘' }}
            </span>
            <span class="result-command">{{ result.command }}</span>
            <span class="result-time">{{ currentTime }}</span>
          </div>
          <div class="result-message" :class="result.success ? 'msg-ok' : 'msg-err'">
            {{ result.message }}
          </div>

          <div v-if="result.errors && result.errors.length > 0" class="result-errors">
            <div v-for="(err, idx) in result.errors" :key="idx" class="error-line">
              <span class="error-type">{{ err.type }}</span>
              <span class="error-text">{{ err.message }}</span>
              <span class="error-pos">at {{ err.line }}:{{ err.column }}</span>
            </div>
          </div>

          <div v-if="result.data && Object.keys(result.data).length > 0" class="result-data">
            <div class="data-header" @click="toggleData">
              <span class="data-toggle" :class="{ open: !showData }">▼</span>
              <span class="data-label">DATA</span>
              <span class="data-count">{{ Object.keys(result.data).length }} fields</span>
            </div>
            <pre v-show="showData" class="data-content">{{ JSON.stringify(result.data, null, 2) }}</pre>
          </div>

          <div v-if="result.tokens && result.tokens.length > 0" class="result-tokens">
            <div class="tokens-header" @click="toggleTokens">
              <span class="tokens-toggle" :class="{ open: !showTokens }">▼</span>
              <span class="tokens-label">TOKENS ({{ result.tokens.length }})</span>
            </div>
            <div v-show="showTokens" class="tokens-grid">
              <div class="tokens-row header-row">
                <span>#</span>
                <span>Type</span>
                <span>Value</span>
                <span>Pos</span>
              </div>
              <div v-for="(t, i) in result.tokens" :key="i" class="tokens-row">
                <span>{{ i + 1 }}</span>
                <span><span class="token-tag">{{ t.type }}</span></span>
                <span class="token-value">{{ t.value }}</span>
                <span class="token-pos">{{ t.line }}:{{ t.column }}</span>
              </div>
            </div>
          </div>
        </div>

        <!-- Resultados por lote - DOS LÍNEAS POR COMANDO -->
        <div v-if="batchResults.length > 0" class="batch-results">
          <div class="batch-summary">
            <span>Batch: {{ batchSuccess }}/{{ batchTotal }} OK</span>
            <span class="batch-bar">
              <span class="batch-fill" :style="{ width: (batchSuccess/batchTotal*100) + '%' }"></span>
            </span>
          </div>
          <div v-for="item in batchResults" :key="item.index" class="batch-item" :class="item.result.success ? 'batch-ok' : 'batch-err'">
            <div class="batch-line1">
              <span class="batch-idx">[{{ item.index }}]</span>
              <span class="batch-status">{{ item.result.success ? '✔' : '✘' }}</span>
              <span class="batch-cmd">{{ item.command }}</span>
            </div>
            <div class="batch-line2">
              <span class="batch-msg">{{ item.result.message }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'CommandOutput',
  props: {
    result: {
      type: Object,
      default: null
    }
  },
  data() {
    return {
      batchResults: [],
      batchTotal: 0,
      batchSuccess: 0,
      batchFailed: 0,
      showData: true,
      showTokens: false,
      currentTime: ''
    }
  },
  computed: {
    hasResults() {
      return this.result || this.batchResults.length > 0
    }
  },
  watch: {
    result: {
      handler(newResult) {
        if (!newResult) return
        this.currentTime = new Date().toLocaleTimeString()

        if (newResult._batch) {
          const existing = this.batchResults.findIndex(
            item => item.index === newResult._batch.index
          )
          if (existing !== -1) {
            this.batchResults[existing] = {
              command: newResult._batch.command,
              result: newResult,
              index: newResult._batch.index
            }
          } else {
            this.batchResults.push({
              command: newResult._batch.command,
              result: newResult,
              index: newResult._batch.index
            })
          }
          this.batchResults.sort((a, b) => a.index - b.index)
          this.batchTotal = this.batchResults.length
          this.batchSuccess = this.batchResults.filter(r => r.result.success).length
          this.batchFailed = this.batchResults.filter(r => !r.result.success).length
        } else {
          if (this.batchResults.length > 0) {
            this.batchResults = []
            this.batchTotal = 0
            this.batchSuccess = 0
            this.batchFailed = 0
          }
        }
      },
      deep: true,
      immediate: true
    }
  },
  methods: {
    toggleData() {
      this.showData = !this.showData
    },
    toggleTokens() {
      this.showTokens = !this.showTokens
    },
    clearOutput() {
      this.$emit('clear')
    },
    exportResults() {
      const now = new Date()
      const ts = now.toISOString().replace(/[:.]/g, '-').slice(0, 19)
      let content = []
      content.push('='.repeat(60))
      content.push('EXT2 ANALYZER - OUTPUT')
      content.push('='.repeat(60))
      content.push(`Date: ${now.toLocaleString()}`)
      content.push('='.repeat(60))

      if (this.batchResults.length > 0) {
        content.push(`Batch: ${this.batchSuccess}/${this.batchTotal} OK`)
        this.batchResults.forEach(item => {
          content.push(`[${item.index}] ${item.result.success ? 'OK' : 'ERR'}`)
          content.push(`  ${item.command}`)
          content.push(`  ${item.result.message}`)
        })
      } else if (this.result) {
        content.push(`Status: ${this.result.success ? 'OK' : 'ERR'}`)
        content.push(`Command: ${this.result.command}`)
        content.push(`Message: ${this.result.message}`)
        if (this.result.data) {
          content.push('Data:')
          content.push(JSON.stringify(this.result.data, null, 2))
        }
      }

      content.push('='.repeat(60))
      const blob = new Blob([content.join('\n')], { type: 'text/plain' })
      const url = URL.createObjectURL(blob)
      const link = document.createElement('a')
      link.href = url
      link.download = `output_${ts}.txt`
      link.click()
      URL.revokeObjectURL(url)
    }
  }
}
</script>

<style scoped>
/* ============================================================
   CONTAINER
   ============================================================ */
.command-output {
  display: flex;
  flex-direction: column;
  height: 100%;
  background: #1e1e2e;
  color: #cdd6f4;
  font-family: 'JetBrains Mono', 'Fira Code', monospace;
  font-size: 12px;
  overflow: hidden;
  border-radius: 6px;
  border: 1px solid #313244;
}

/* ============================================================
   TOOLBAR
   ============================================================ */
.output-toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 4px 12px;
  background: #313244;
  border-bottom: 1px solid #45475a;
  flex-shrink: 0;
  min-height: 28px;
}

.toolbar-left {
  display: flex;
  align-items: center;
  gap: 8px;
}

.toolbar-icon {
  color: #89b4fa;
  font-size: 10px;
}

.toolbar-title {
  font-size: 10px;
  font-weight: 600;
  color: #a6adc8;
  letter-spacing: 0.5px;
}

.toolbar-badge {
  font-size: 9px;
  font-weight: 700;
  padding: 0 8px;
  border-radius: 10px;
  background: #45475a;
  color: #a6adc8;
}

.toolbar-badge.success {
  background: #a6e3a1;
  color: #1e1e2e;
}

.toolbar-badge.error {
  background: #f38ba8;
  color: #1e1e2e;
}

.toolbar-right {
  display: flex;
  gap: 6px;
}

.btn-export, .btn-clear {
  background: transparent;
  border: none;
  color: #a6adc8;
  font-size: 10px;
  font-family: inherit;
  cursor: pointer;
  padding: 2px 10px;
  border-radius: 4px;
  transition: all 0.2s ease;
}

.btn-export:hover {
  background: #45475a;
  color: #cdd6f4;
}

.btn-clear:hover {
  background: #45475a;
  color: #f38ba8;
}

/* ============================================================
   CONTENT
   ============================================================ */
.output-content {
  flex: 1;
  overflow: hidden;
  padding: 6px 10px;
}

.output-scroll {
  height: 100%;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

/* ============================================================
   EMPTY STATE
   ============================================================ */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: #45475a;
}

.empty-icon {
  font-size: 28px;
  margin-bottom: 6px;
  opacity: 0.4;
}

.empty-text {
  font-size: 13px;
  font-weight: 500;
  color: #a6adc8;
}

.empty-hint {
  font-size: 10px;
  color: #45475a;
  margin-top: 2px;
}

/* ============================================================
   RESULT ITEM
   ============================================================ */
.result-item {
  background: #1e1e2e;
  border: 1px solid #313244;
  border-radius: 4px;
  padding: 8px 10px;
  margin-bottom: 2px;
}

.result-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding-bottom: 4px;
  border-bottom: 1px solid #313244;
}

.result-status {
  font-size: 13px;
  font-weight: 700;
}

.status-ok {
  color: #a6e3a1;
}

.status-err {
  color: #f38ba8;
}

.result-command {
  font-size: 11px;
  color: #89b4fa;
  font-weight: 500;
  flex: 1;
  word-break: break-all;
}

.result-time {
  font-size: 9px;
  color: #45475a;
  font-weight: 300;
}

.result-message {
  padding: 4px 0 4px 24px;
  font-size: 12px;
}

.msg-ok {
  color: #a6e3a1;
}

.msg-err {
  color: #f38ba8;
}

/* ============================================================
   ERRORS
   ============================================================ */
.result-errors {
  padding: 4px 0 4px 24px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.error-line {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 11px;
  background: #313244;
  padding: 2px 8px;
  border-radius: 3px;
  border-left: 2px solid #f38ba8;
}

.error-type {
  font-size: 8px;
  font-weight: 700;
  text-transform: uppercase;
  color: #f38ba8;
  background: #1e1e2e;
  padding: 0 6px;
  border-radius: 3px;
}

.error-text {
  color: #cdd6f4;
  flex: 1;
}

.error-pos {
  font-size: 9px;
  color: #45475a;
}

/* ============================================================
   DATA
   ============================================================ */
.result-data {
  padding: 4px 0 4px 24px;
}

.data-header {
  display: flex;
  align-items: center;
  gap: 6px;
  cursor: pointer;
  user-select: none;
  padding: 2px 0;
}

.data-toggle {
  font-size: 9px;
  color: #45475a;
  transition: transform 0.2s ease;
  display: inline-block;
}

.data-toggle.open {
  transform: rotate(-90deg);
}

.data-label {
  font-size: 9px;
  font-weight: 600;
  color: #45475a;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.data-count {
  font-size: 9px;
  color: #45475a;
  margin-left: auto;
  font-weight: 400;
}

.data-content {
  margin: 4px 0 0 0;
  padding: 6px 10px;
  background: #181825;
  border-radius: 4px;
  font-size: 10px;
  color: #a6e3a1;
  font-family: 'JetBrains Mono', monospace;
  overflow-x: auto;
  max-height: 120px;
  overflow-y: auto;
  border: 1px solid #313244;
}

/* ============================================================
   TOKENS
   ============================================================ */
.result-tokens {
  padding: 4px 0 4px 24px;
}

.tokens-header {
  display: flex;
  align-items: center;
  gap: 6px;
  cursor: pointer;
  user-select: none;
  padding: 2px 0;
}

.tokens-toggle {
  font-size: 9px;
  color: #45475a;
  transition: transform 0.2s ease;
  display: inline-block;
}

.tokens-toggle.open {
  transform: rotate(-90deg);
}

.tokens-label {
  font-size: 9px;
  font-weight: 600;
  color: #45475a;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.tokens-grid {
  margin: 4px 0 0 0;
  background: #181825;
  border-radius: 4px;
  border: 1px solid #313244;
  overflow: hidden;
  font-size: 10px;
}

.tokens-row {
  display: grid;
  grid-template-columns: 28px 70px 1fr 50px;
  padding: 2px 8px;
  border-bottom: 1px solid #313244;
  align-items: center;
  gap: 4px;
}

.tokens-row:last-child {
  border-bottom: none;
}

.tokens-row.header-row {
  background: #313244;
  color: #45475a;
  font-size: 8px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.3px;
}

.tokens-row.header-row span {
  color: #a6adc8;
}

.token-tag {
  display: inline-block;
  padding: 0 6px;
  border-radius: 3px;
  font-size: 8px;
  font-weight: 600;
  background: #45475a;
  color: #a6adc8;
}

.token-value {
  color: #89b4fa;
  font-family: 'JetBrains Mono', monospace;
  font-size: 10px;
  word-break: break-all;
}

.token-pos {
  color: #45475a;
  font-size: 9px;
  text-align: center;
}

/* ============================================================
   BATCH RESULTS - DOS LÍNEAS POR COMANDO
   ============================================================ */
.batch-results {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.batch-summary {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 11px;
  color: #a6adc8;
  padding: 4px 8px;
  background: #313244;
  border-radius: 4px;
  flex-shrink: 0;
}

.batch-bar {
  flex: 1;
  height: 4px;
  background: #45475a;
  border-radius: 2px;
  overflow: hidden;
}

.batch-fill {
  display: block;
  height: 100%;
  background: #89b4fa;
  border-radius: 2px;
  transition: width 0.5s ease;
}

.batch-item {
  display: flex;
  flex-direction: column;
  padding: 3px 8px;
  border-radius: 3px;
  background: #1e1e2e;
  border-left: 2px solid #45475a;
  gap: 1px;
}

.batch-item.batch-ok {
  border-left-color: #a6e3a1;
}

.batch-item.batch-err {
  border-left-color: #f38ba8;
}

.batch-line1 {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-wrap: wrap;
}

.batch-idx {
  font-size: 9px;
  color: #45475a;
  min-width: 32px;
  flex-shrink: 0;
}

.batch-status {
  font-size: 11px;
  flex-shrink: 0;
}

.batch-ok .batch-status {
  color: #a6e3a1;
}

.batch-err .batch-status {
  color: #f38ba8;
}

.batch-cmd {
  color: #89b4fa;
  font-size: 10px;
  word-break: break-all;
  flex: 1;
}

.batch-line2 {
  padding-left: 40px;
}

.batch-msg {
  font-size: 10px;
  color: #a6adc8;
  word-break: break-word;
}

.batch-ok .batch-msg {
  color: #a6e3a1;
}

.batch-err .batch-msg {
  color: #f38ba8;
}

/* ============================================================
   SCROLLBAR
   ============================================================ */
::-webkit-scrollbar {
  width: 4px;
  height: 4px;
}

::-webkit-scrollbar-track {
  background: #1e1e2e;
}

::-webkit-scrollbar-thumb {
  background: #45475a;
  border-radius: 2px;
}

::-webkit-scrollbar-thumb:hover {
  background: #89b4fa;
}
</style>