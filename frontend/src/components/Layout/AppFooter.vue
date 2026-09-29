<template>
  <footer class="app-footer">
    <div class="footer-content">
      <span>MIA - 2S 2026</span>
      <span class="footer-divider">|</span>
      <span>Proyecto 1</span>
      <span class="footer-divider">|</span>
      <span>Carnet: 202200081</span>
      <span class="footer-spacer"></span>
      <span class="footer-version">v2.0</span>
    </div>
  </footer>
</template>

<script>
export default {
  name: 'AppFooter'
}
</script>

<style scoped>
.app-footer {
  background: #0d1117;
  border-top: 1px solid #30363d;
  padding: 6px 20px;
  flex-shrink: 0;
}

.footer-content {
  max-width: 100%;
  margin: 0 auto;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 4px 10px;
  font-size: 10px;
  color: #8b949e;
}

.footer-divider {
  color: #30363d;
}

.footer-spacer {
  flex: 1;
}

.footer-version {
  color: #30363d;
  font-family: monospace;
}

@media (max-width: 480px) {
  .app-footer {
    padding: 4px 12px;
  }
  .footer-content {
    font-size: 9px;
    gap: 2px 6px;
  }
  .footer-divider {
    display: none;
  }
  .footer-version {
    display: none;
  }
}
</style>