<template>
  <div class="login-view">
    <div class="login-card">
      <div class="login-header">
        <h1>🔐 Login</h1>
        <p class="login-subtitle">Inicia sesión para acceder a los comandos</p>
      </div>

      <div class="login-body">
        <div class="form-group">
          <label for="id">ID Partición</label>
          <input
            id="id"
            v-model="form.id"
            type="text"
            placeholder="Ej: 811A"
            :disabled="loading || store.state.isLoggedIn"
            @keyup.enter="submitLogin"
          />
        </div>

        <div class="form-group">
          <label for="user">Usuario</label>
          <input
            id="user"
            v-model="form.user"
            type="text"
            placeholder="Ej: root"
            :disabled="loading || store.state.isLoggedIn"
            @keyup.enter="submitLogin"
          />
        </div>

        <div class="form-group">
          <label for="pass">Contraseña</label>
          <input
            id="pass"
            v-model="form.pass"
            type="password"
            placeholder="••••••"
            :disabled="loading || store.state.isLoggedIn"
            @keyup.enter="submitLogin"
          />
        </div>

        <div class="form-check">
          <input id="remember" v-model="rememberUser" type="checkbox" />
          <label for="remember">Recordar usuario</label>
        </div>

        <button
          class="btn-submit"
          :disabled="loading || store.state.isLoggedIn || !canSubmit"
          @click="submitLogin"
        >
          <span v-if="!loading">Iniciar Sesión</span>
          <span v-else>Iniciando...</span>
        </button>

        <div v-if="error" class="login-error">
          {{ error }}
        </div>

        <div v-if="store.state.isLoggedIn" class="login-success">
          ✅ Ya hay una sesión activa como <strong>{{ store.state.currentUser }}</strong> en <strong>{{ store.state.mountId }}</strong>
          <br><br>
          <button class="btn-go-home" @click="$router.push('/')">Ir al Home</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { inject } from 'vue'
import { loginUser, getSessionStatus } from '../services/api.js'

export default {
  name: 'LoginView',
  setup() {
    const store = inject('store')
    return { store }
  },
  data() {
    return {
      form: {
        id: '',
        user: '',
        pass: ''
      },
      rememberUser: false,
      loading: false,
      error: null
    }
  },
  computed: {
    canSubmit() {
      return this.form.id && this.form.user && this.form.pass
    }
  },
  mounted() {
    this.loadRememberedUser()
    this.checkCurrentSession()
  },
  methods: {
    loadRememberedUser() {
      const saved = localStorage.getItem('ext2_remembered_user')
      if (saved) {
        try {
          const data = JSON.parse(saved)
          this.form.user = data.user || ''
          this.form.id = data.id || ''
          this.rememberUser = true
        } catch (e) {
          console.error('Error cargando usuario recordado', e)
        }
      }
    },

    async checkCurrentSession() {
      const result = await getSessionStatus()
      if (result.success && result.data?.active) {
        this.store.actions.setSession(result.data)
      }
    },

    async submitLogin() {
      if (!this.canSubmit) return
      
      this.loading = true
      this.error = null

      try {
        const result = await loginUser(this.form.id, this.form.user, this.form.pass)

        if (result.success) {
          // ✅ Actualizar store con la nueva sesión
          this.store.actions.setSession({
            active: true,
            user: this.form.user,
            mountId: this.form.id,
            diskPath: result.data?.diskPath || '',
            uid: result.data?.session?.uid || 1,
            gid: result.data?.session?.gid || 1,
            group: result.data?.session?.group || 'root'
          })

          // Guardar usuario si se marcó "recordar"
          if (this.rememberUser) {
            localStorage.setItem('ext2_remembered_user', JSON.stringify({
              user: this.form.user,
              id: this.form.id
            }))
          } else {
            localStorage.removeItem('ext2_remembered_user')
          }

          // ✅ Redirigir al home
          this.$router.push('/')
        } else {
          this.error = result.data?.message || result.error || 'Error al iniciar sesión'
        }
      } catch (err) {
        this.error = 'Error de conexión: ' + err.message
      }

      this.loading = false
    }
  }
}
</script>

<style scoped>
/* Los mismos estilos + .btn-go-home */
.login-view {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 60vh;
  padding: 20px;
}

.login-card {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 12px;
  padding: 32px;
  width: 100%;
  max-width: 420px;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.5);
}

.login-header {
  text-align: center;
  margin-bottom: 24px;
}

.login-header h1 {
  font-size: 24px;
  font-weight: 700;
  color: #e6edf3;
  margin: 0 0 8px 0;
}

.login-subtitle {
  font-size: 13px;
  color: #8b949e;
  margin: 0;
}

.login-body {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.form-group label {
  font-size: 12px;
  font-weight: 600;
  color: #8b949e;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.form-group input {
  width: 100%;
  padding: 10px 14px;
  font-size: 14px;
  background: #0d1117;
  border: 1px solid #30363d;
  border-radius: 6px;
  color: #e6edf3;
  transition: border-color 0.3s ease;
}

.form-group input:focus {
  outline: none;
  border-color: #58a6ff;
}

.form-group input:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.form-check {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  color: #8b949e;
}

.form-check input[type="checkbox"] {
  width: 16px;
  height: 16px;
  cursor: pointer;
}

.btn-submit {
  width: 100%;
  padding: 12px;
  font-size: 14px;
  font-weight: 600;
  color: #0d1117;
  background: #58a6ff;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  transition: all 0.3s ease;
  margin-top: 8px;
}

.btn-submit:hover:not(:disabled) {
  background: #79c0ff;
}

.btn-submit:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-go-home {
  padding: 8px 20px;
  background: #58a6ff;
  color: #0d1117;
  border: none;
  border-radius: 6px;
  font-weight: 600;
  cursor: pointer;
  font-size: 13px;
}

.login-error {
  padding: 12px;
  background: rgba(248, 81, 73, 0.1);
  border: 1px solid #f85149;
  border-radius: 6px;
  color: #f85149;
  font-size: 13px;
  text-align: center;
}

.login-success {
  padding: 12px;
  background: rgba(63, 185, 80, 0.1);
  border: 1px solid #3fb950;
  border-radius: 6px;
  color: #3fb950;
  font-size: 13px;
  text-align: center;
}
</style>