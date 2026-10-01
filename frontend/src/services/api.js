import axios from 'axios'

// Usar el proxy de Vite (no el backend directamente)
const API_BASE_URL = '/api'

const api = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  },
  timeout: 60000,
})

// ============================================================
// POST /analyze — Ejecutar un comando
// ============================================================
export const analyzeCommand = async (command) => {
  try {
    const response = await api.post('/analyze', { command })
    return {
      success: true,
      data: response.data,
      error: null
    }
  } catch (error) {
    console.error('Error en analyzeCommand:', error)
    return {
      success: false,
      data: null,
      error: error.message || 'Error de conexión con el servidor'
    }
  }
}

// ============================================================
// ✅ NUEVO: POST /login — Login desde GUI
// ============================================================
export const loginUser = async (id, user, pass) => {
  try {
    const response = await api.post('/login', { id, user, pass })
    return {
      success: response.data.success === true,
      data: response.data,
      error: null
    }
  } catch (error) {
    return {
      success: false,
      data: null,
      error: error.message || 'Error al iniciar sesión'
    }
  }
}

// ============================================================
// ✅ NUEVO: POST /logout — Logout desde GUI
// ============================================================
export const logoutUser = async () => {
  try {
    const response = await api.post('/logout', {})
    return {
      success: response.data.success === true,
      data: response.data,
      error: null
    }
  } catch (error) {
    return {
      success: false,
      data: null,
      error: error.message || 'Error al cerrar sesión'
    }
  }
}

// ============================================================
// GET /session/status
// ============================================================
export const getSessionStatus = async () => {
  try {
    const response = await api.get('/session/status', { timeout: 5000 })
    return {
      success: true,
      data: response.data,
      error: null
    }
  } catch (error) {
    return {
      success: false,
      data: null,
      error: error.message || 'Error consultando sesión'
    }
  }
}

// ============================================================
// ✅ NUEVO: GET /disks — Listar discos
// ============================================================
export const getDisks = async () => {
  try {
    const response = await api.get('/disks', { timeout: 5000 })
    return {
      success: true,
      data: response.data,
      error: null
    }
  } catch (error) {
    return {
      success: false,
      data: null,
      error: error.message || 'Error obteniendo discos'
    }
  }
}

// ============================================================
// GET /health — Verificar backend
// ============================================================
export const checkBackendStatus = async () => {
  try {
    const response = await api.get('/health', { timeout: 3000 })
    return {
      online: response.status === 200,
      message: response.data?.status || 'OK'
    }
  } catch (error) {
    return {
      online: false,
      message: error.message || 'Backend no disponible'
    }
  }
}

export default api