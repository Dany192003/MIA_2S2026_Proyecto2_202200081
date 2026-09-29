<template>
  <div class="command-input">
    <div class="input-header">
      <span class="input-label">Entrada</span>
      <div class="connection-status">
        <span class="status-dot" :class="backendOnline ? 'online' : 'offline'"></span>
        <span class="status-text" :class="backendOnline ? 'online' : 'offline'">
          {{ backendOnline ? 'Conectado' : 'Desconectado' }}
        </span>
      </div>
    </div>

    <div class="input-group">
      <textarea
        v-model="command"
        placeholder="Ej: mkdisk -size=100 -path=discos/disco.mia"
        class="command-textarea"
        @keydown.ctrl.enter="submitCommand"
        ref="textarea"
      ></textarea>
    </div>

    <div class="button-group">
      <button class="btn-primary" @click="submitCommand" :disabled="loading || !backendOnline">
        <span v-if="!loading">Ejecutar</span>
        <span v-else>Procesando...</span>
      </button>
      <button class="btn-secondary" @click="clearCommand">Limpiar</button>
    </div>

    <FileUpload @file-loaded="onFileLoaded" />

    <div class="shortcuts">
      <span>Ctrl + Enter para ejecutar</span>
    </div>
  </div>
</template>

<script>
import { analyzeCommand, checkBackendStatus } from '../services/api.js'
import FileUpload from './FileUpload.vue'

export default {
  name: 'CommandInput',
  components: {
    FileUpload
  },
  data() {
    return {
      command: '',
      loading: false,
      backendOnline: false,
      batchResults: [],
      batchIndex: 0
    }
  },
  mounted() {
    this.checkConnection()
    setInterval(this.checkConnection, 10000)
  },
  methods: {
    async checkConnection() {
      const status = await checkBackendStatus()
      this.backendOnline = status.online
    },

    async submitCommand() {
      // ✅ GUARD DE REENTRADA: Evita ejecuciones concurrentes
      if (this.loading) return

      if (!this.command.trim()) {
        this.$emit('command-submitted', {
          success: false,
          message: 'Error: Ingrese un comando',
          errors: [{ message: 'Comando vacio' }]
        })
        return
      }

      if (!this.backendOnline) {
        this.$emit('command-submitted', {
          success: false,
          message: 'Error: Backend no disponible',
          errors: [{ message: 'Servidor no esta corriendo en http://localhost:8080' }]
        })
        return
      }

      this.loading = true
      this.batchResults = []
      this.batchIndex = 0

      // Dividir en líneas y filtrar vacías
      const lines = this.command.split('\n')
        .map(line => line.trim())
        .filter(line => line.length > 0 && !line.startsWith('#'))

      // Si es una sola línea, ejecutar normalmente
      if (lines.length === 1) {
        const result = await analyzeCommand(lines[0])
        if (result.success) {
          this.$emit('command-submitted', result.data)
        } else {
          this.$emit('command-submitted', {
            success: false,
            message: 'Error en la ejecucion',
            errors: [{ message: result.error || 'Error desconocido' }]
          })
        }
        this.loading = false
        return
      }

      // ✅ Ejecutar SECUENCIALMENTE (uno por uno)
      for (let i = 0; i < lines.length; i++) {
        const line = lines[i]
        this.batchIndex = i + 1
        
        const result = await analyzeCommand(line)
        this.batchResults.push({
          index: i + 1,
          command: line,
          result: result.data || { success: false, message: result.error }
        })

        // Emitir cada resultado individualmente
        this.$emit('command-submitted', {
          ...(result.data || { success: false, message: result.error }),
          _batch: {
            index: i + 1,
            command: line,
            total: lines.length
          }
        })
      }

      // ✅ NUEVO: Emitir evento de fin de batch
      this.$emit('batch-completed', this.batchResults)

      this.loading = false
    },

    clearCommand() {
      this.command = ''
      this.batchResults = []
      this.batchIndex = 0
      this.$emit('command-submitted', null)
    },

    onFileLoaded(data) {
      if (data.commands && data.commands.length > 0) {
        this.command = data.commands.join('\n')
      }
    }
  }
}
</script>

<style scoped>
.command-input {
  display: flex;
  flex-direction: column;
  gap: 6px;
  height: 100%;
}

.input-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 4px;
  border-bottom: 1px solid #21262d;
  flex-shrink: 0;
}

.input-label {
  font-size: 10px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.connection-status {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 10px;
}

.status-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  display: inline-block;
}

.status-dot.online {
  background: #3fb950;
}

.status-dot.offline {
  background: #f85149;
}

.status-text.online {
  color: #3fb950;
}

.status-text.offline {
  color: #f85149;
}

.input-group {
  flex: 1;
  min-height: 0;
}

.command-textarea {
  width: 100%;
  height: 100%;
  min-height: 60px;
  padding: 8px 12px;
  background: #0d1117;
  color: #e6edf3;
  border: 1px solid #30363d;
  border-radius: 6px;
  font-family: 'Courier New', monospace;
  font-size: 13px;
  resize: none;
  transition: border-color 0.3s ease;
  line-height: 1.5;
}

.command-textarea:focus {
  outline: none;
  border-color: #58a6ff;
}

.command-textarea::placeholder {
  color: #30363d;
}

.button-group {
  display: flex;
  gap: 6px;
  flex-shrink: 0;
}

.btn-primary {
  flex: 1;
  padding: 6px 16px;
  background: #58a6ff;
  color: #0d1117;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-primary:hover:not(:disabled) {
  background: #79c0ff;
}

.btn-primary:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-secondary {
  padding: 6px 14px;
  background: transparent;
  color: #8b949e;
  border: 1px solid #30363d;
  border-radius: 6px;
  font-weight: 500;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-secondary:hover {
  border-color: #58a6ff;
  color: #e6edf3;
}

.shortcuts {
  text-align: center;
  font-size: 9px;
  color: #30363d;
  flex-shrink: 0;
  padding-top: 2px;
}

@media (max-width: 480px) {
  .command-textarea {
    font-size: 12px;
    padding: 6px 10px;
    min-height: 50px;
  }
  .btn-primary, .btn-secondary {
    font-size: 11px;
    padding: 4px 10px;
  }
}
</style>