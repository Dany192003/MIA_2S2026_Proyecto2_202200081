<template>
  <div class="file-upload">
    <div class="upload-area" @click="$refs.fileInput.click()">
      <input type="file" ref="fileInput" @change="handleFile" accept=".txt,.smia" style="display: none" />
      <div v-if="!fileLoaded" class="upload-placeholder">
        <span>Cargar script</span>
        <span class="upload-hint">.txt o .smia</span>
      </div>
      <div v-else class="file-info">
        <span class="file-name">{{ fileName }}</span>
        <span class="file-lines">{{ totalLines }} cmd</span>
        <button @click.stop="clearFile" class="btn-remove">×</button>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'FileUpload',
  data() {
    return {
      fileLoaded: false,
      fileName: '',
      totalLines: 0,
      commands: []
    }
  },
  methods: {
    handleFile(event) {
      const file = event.target.files[0]
      if (file) this.processFile(file)
    },
    processFile(file) {
      const reader = new FileReader()
      reader.onload = (e) => {
        const lines = e.target.result.split('\n')
          .map(line => line.trim())
          .filter(line => line.length > 0 && !line.startsWith('#'))
        this.commands = lines
        this.totalLines = lines.length
        this.fileName = file.name
        this.fileLoaded = true
        this.$emit('file-loaded', { count: this.totalLines, commands: this.commands })
      }
      reader.readAsText(file)
    },
    clearFile() {
      this.fileLoaded = false
      this.fileName = ''
      this.totalLines = 0
      this.commands = []
      this.$refs.fileInput.value = ''
      this.$emit('file-cleared')
    }
  }
}
</script>

<style scoped>
.file-upload {
  flex-shrink: 0;
}

.upload-area {
  border: 1px dashed #30363d;
  border-radius: 4px;
  padding: 4px 8px;
  text-align: center;
  cursor: pointer;
  transition: all 0.3s ease;
  background: #0d1117;
  min-height: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.upload-area:hover {
  border-color: #58a6ff;
  background: #161b22;
}

.upload-placeholder {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 10px;
  color: #8b949e;
}

.upload-hint {
  font-size: 8px;
  color: #30363d;
  border: 1px solid #30363d;
  border-radius: 3px;
  padding: 0 6px;
}

.file-info {
  display: flex;
  align-items: center;
  gap: 6px;
  width: 100%;
  font-size: 10px;
}

.file-name {
  color: #e6edf3;
  font-weight: 500;
  flex: 1;
  text-align: left;
}

.file-lines {
  color: #8b949e;
  font-size: 9px;
  background: #21262d;
  padding: 0 6px;
  border-radius: 3px;
}

.btn-remove {
  background: transparent;
  border: none;
  color: #8b949e;
  font-size: 14px;
  cursor: pointer;
  padding: 0 2px;
}

.btn-remove:hover {
  color: #f85149;
}
</style>