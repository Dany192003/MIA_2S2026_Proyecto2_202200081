<template>
  <div class="command-terminal">
    <div class="terminal-header">
      <div class="terminal-dots">
        <span class="dot"></span>
        <span class="dot"></span>
        <span class="dot"></span>
      </div>
      <span class="terminal-title">TERMINAL DE COMANDOS</span>
      <button class="btn-clear" @click="clearTerminal">Limpiar</button>
    </div>

    <div class="terminal-grid">
      <div class="terminal-left">
        <CommandInput 
          @command-submitted="handleCommand" 
          @batch-completed="handleBatchCompleted"
        />
      </div>
      <div class="terminal-right">
        <CommandOutput :result="currentResult" />
      </div>
    </div>

    <div class="terminal-footer">
      <span>Ctrl + Enter para ejecutar | Carga archivos .smia o .txt</span>
    </div>
  </div>
</template>

<script>
import CommandInput from '../CommandInput.vue'
import CommandOutput from '../CommandOutput.vue'

export default {
  name: 'CommandTerminal',
  components: {
    CommandInput,
    CommandOutput
  },
  data() {
    return {
      currentResult: null
    }
  },
  methods: {
    handleCommand(result) {
      this.currentResult = result
      this.$emit('command-executed', result)
    },
    handleBatchCompleted(results) {
      this.$emit('batch-completed', results)
    },
    clearTerminal() {
      this.currentResult = null
    }
  }
}
</script>

<style scoped>
.command-terminal {
  background: #0d1117;
  border-radius: 8px;
  border: 1px solid #30363d;
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 350px;
  overflow: hidden;
}

.terminal-header {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 6px 14px;
  background: #161b22;
  border-bottom: 1px solid #30363d;
  flex-shrink: 0;
}

.terminal-dots {
  display: flex;
  gap: 5px;
}

.terminal-dots .dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  display: inline-block;
}

.terminal-dots .dot:nth-child(1) { background: #f85149; }
.terminal-dots .dot:nth-child(2) { background: #d29922; }
.terminal-dots .dot:nth-child(3) { background: #3fb950; }

.terminal-title {
  flex: 1;
  font-size: 11px;
  color: #8b949e;
  font-weight: 600;
  letter-spacing: 1px;
  text-align: center;
}

.btn-clear {
  background: transparent;
  border: 1px solid #30363d;
  color: #8b949e;
  padding: 2px 12px;
  border-radius: 4px;
  font-size: 11px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-clear:hover {
  border-color: #f85149;
  color: #f85149;
}

.terminal-grid {
  display: grid;
  grid-template-columns: 1fr 1.2fr;
  gap: 0;
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

.terminal-left {
  padding: 10px 14px;
  border-right: 1px solid #30363d;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  min-height: 0;
}

.terminal-left .command-input {
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.terminal-left .command-input .input-group {
  flex: 1;
  min-height: 0;
}

.terminal-left .command-input .command-textarea {
  height: 100%;
  min-height: 80px;
  resize: none;
}

.terminal-right {
  padding: 10px 14px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  min-height: 0;
}

.terminal-right .command-output {
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.terminal-right .command-output .results,
.terminal-right .command-output .batch-results {
  flex: 1;
  overflow-y: auto;
  padding-right: 4px;
}

.terminal-footer {
  padding: 4px 14px;
  background: #161b22;
  border-top: 1px solid #30363d;
  text-align: center;
  flex-shrink: 0;
  font-size: 10px;
  color: #8b949e;
  letter-spacing: 0.3px;
}

@media (max-width: 768px) {
  .command-terminal {
    min-height: 280px;
  }
  .terminal-grid {
    grid-template-columns: 1fr;
  }
  .terminal-left {
    border-right: none;
    border-bottom: 1px solid #30363d;
    max-height: 45%;
    padding: 8px 12px;
  }
  .terminal-right {
    max-height: 55%;
    padding: 8px 12px;
  }
}

@media (max-width: 480px) {
  .command-terminal {
    min-height: 220px;
  }
  .terminal-left {
    max-height: 40%;
    padding: 6px 10px;
  }
  .terminal-right {
    max-height: 60%;
    padding: 6px 10px;
  }
  .terminal-header {
    padding: 4px 10px;
  }
  .terminal-title {
    font-size: 9px;
  }
}
</style>