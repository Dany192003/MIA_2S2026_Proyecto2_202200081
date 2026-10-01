<template>
  <div id="app">
    <AppHeader />
    <main class="main-content">
      <router-view />
    </main>
    <AppFooter />
  </div>
</template>

<script>
import AppHeader from './components/Layout/AppHeader.vue'
import AppFooter from './components/Layout/AppFooter.vue'
import { getSessionStatus } from './services/api.js'

export default {
  name: 'App',
  components: {
    AppHeader,
    AppFooter
  },
  data() {
    return {
      sessionCheckInterval: null
    }
  },
  mounted() {
    this.checkSession()
    // Verificar sesión cada 5 segundos
    this.sessionCheckInterval = setInterval(this.checkSession, 5000)
  },
  beforeUnmount() {
    if (this.sessionCheckInterval) {
      clearInterval(this.sessionCheckInterval)
    }
  },
  methods: {
    async checkSession() {
      try {
        const result = await getSessionStatus()
        if (result.success && result.data) {
          this.$store?.actions?.setSession(result.data)
        }
      } catch (error) {
        // Silenciar errores
      }
    }
  }
}
</script>

<style scoped>
* {
  box-sizing: border-box;
}

html, body {
  margin: 0;
  padding: 0;
  height: 100%;
  overflow: hidden;
  background: #0d1117;
}

#app {
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: #0d1117;
  color: #e6edf3;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  overflow: hidden;
}

.main-content {
  flex: 1;
  padding: 10px 16px;
  display: flex;
  flex-direction: column;
  gap: 10px;
  overflow-y: auto;
  min-height: 0;
}

@media (max-width: 768px) {
  .main-content {
    padding: 8px 12px;
    gap: 8px;
  }
}

@media (max-width: 480px) {
  .main-content {
    padding: 6px 8px;
    gap: 6px;
  }
}
</style>