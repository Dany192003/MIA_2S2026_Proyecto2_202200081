<template>
  <header class="app-header">
    <div class="header-content">
      <div class="logo">
        <h1>EXT2 <span>Analyzer</span></h1>
        <span class="subtitle">Sistema de Archivos</span>
      </div>
    </div>
  </header>
</template>

<script>
export default {
  name: 'AppHeader'
}
</script>

<style scoped>
.app-header {
  background: #0d1117;
  border-bottom: 1px solid #30363d;
  padding: 8px 20px;
  flex-shrink: 0;
}

.header-content {
  max-width: 100%;
  margin: 0 auto;
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 8px;
}

.logo {
  display: flex;
  align-items: baseline;
  gap: 8px;
}

.logo h1 {
  font-size: 16px;
  font-weight: 300;
  color: #e6edf3;
  margin: 0;
}

.logo h1 span {
  color: #58a6ff;
  font-weight: 600;
}

.subtitle {
  font-size: 10px;
  color: #8b949e;
  letter-spacing: 0.3px;
}

@media (max-width: 480px) {
  .app-header {
    padding: 6px 12px;
  }
  .logo h1 {
    font-size: 13px;
  }
  .subtitle {
    display: none;
  }
}
</style>