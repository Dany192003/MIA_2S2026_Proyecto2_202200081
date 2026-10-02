<template>
  <header class="app-header">
    <div class="header-content">
      <div class="logo" @click="$router.push('/')">
        <h1>EXT2/3 <span>Analyzer</span></h1>
        <span class="subtitle">C++ Disk Online</span>
      </div>

      <nav class="nav-links">
        <router-link to="/" class="nav-link" :class="{ active: $route.path === '/' }">
          🏠 Home
        </router-link>
        <router-link to="/visualizer" class="nav-link" :class="{ active: $route.path === '/visualizer' }">
          📁 Visualizador
        </router-link>
        <router-link to="/journal" class="nav-link" :class="{ active: $route.path === '/journal' }">
          📓 Journal
        </router-link>
        <router-link to="/loss" class="nav-link" :class="{ active: $route.path === '/loss' }">
          💥 LOSS
        </router-link>
      </nav>

      <div class="session-info">
        <template v-if="store.state.isLoggedIn">
          <span class="user-badge">👤 {{ store.state.currentUser }}</span>
          <span class="mount-badge">ID: {{ store.state.mountId }}</span>
          <button class="btn-logout" @click="handleLogout">Cerrar Sesión</button>
        </template>
        <template v-else>
          <router-link to="/login" class="btn-login">Iniciar Sesión</router-link>
        </template>
      </div>
    </div>
  </header>
</template>

<script>
import { inject } from 'vue'
import { getSessionStatus, logoutUser } from '../../services/api.js'

export default {
  name: 'AppHeader',
  data() {
    return {
      checkInterval: null
    }
  },
  setup() {
    const store = inject('store')
    return { store }
  },
  mounted() {
    this.checkSession()
    // Reducir intervalo a 2 segundos para mejor respuesta
    this.checkInterval = setInterval(this.checkSession, 2000)
  },
  beforeUnmount() {
    if (this.checkInterval) {
      clearInterval(this.checkInterval)
    }
  },
  methods: {
    async checkSession() {
      const result = await getSessionStatus()
      if (result.success && result.data) {
        // ✅ Actualizar store (que es reactivo)
        this.store.actions.setSession(result.data)
      }
    },

    async handleLogout() {
      if (!confirm('¿Cerrar sesión?')) return
      const result = await logoutUser()
      if (result.success) {
        // ✅ Limpiar store
        this.store.actions.clearSession()
        this.$router.push('/login')
      }
    }
  }
}
</script>

<style scoped>
/* Los mismos estilos que ya tienes */
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
  gap: 12px;
}

.logo {
  display: flex;
  align-items: baseline;
  gap: 8px;
  cursor: pointer;
  user-select: none;
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

.nav-links {
  display: flex;
  gap: 4px;
  flex: 1;
  justify-content: center;
}

.nav-link {
  padding: 6px 12px;
  border-radius: 6px;
  color: #8b949e;
  text-decoration: none;
  font-size: 13px;
  font-weight: 500;
  transition: all 0.3s ease;
}

.nav-link:hover {
  background: #21262d;
  color: #e6edf3;
  text-decoration: none;
}

.nav-link.active {
  background: rgba(88, 166, 255, 0.1);
  color: #58a6ff;
}

.session-info {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
}

.user-badge {
  padding: 4px 10px;
  background: rgba(63, 185, 80, 0.1);
  border: 1px solid rgba(63, 185, 80, 0.2);
  border-radius: 12px;
  color: #3fb950;
}

.mount-badge {
  padding: 4px 10px;
  background: rgba(88, 166, 255, 0.1);
  border: 1px solid rgba(88, 166, 255, 0.2);
  border-radius: 12px;
  color: #58a6ff;
  font-family: monospace;
}

.btn-login {
  padding: 6px 14px;
  background: #58a6ff;
  color: #0d1117;
  border-radius: 6px;
  text-decoration: none;
  font-weight: 600;
  transition: all 0.3s ease;
}

.btn-login:hover {
  background: #79c0ff;
  text-decoration: none;
}

.btn-logout {
  padding: 6px 14px;
  background: transparent;
  color: #f85149;
  border: 1px solid #f85149;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-logout:hover {
  background: rgba(248, 81, 73, 0.1);
}

@media (max-width: 768px) {
  .nav-links {
    order: 3;
    width: 100%;
    justify-content: flex-start;
    overflow-x: auto;
  }
  .nav-link {
    font-size: 11px;
    padding: 4px 8px;
    white-space: nowrap;
  }
}
</style>