import { reactive, readonly } from 'vue'

const state = reactive({
  // Estado de sesión
  isLoggedIn: false,
  currentUser: '',
  mountId: '',
  diskPath: '',
  uid: -1,
  gid: -1,
  
  // Particiones montadas
  mountedPartitions: [],
  
  // Partición seleccionada (activa)
  selectedMountId: '',
  
  // Estado de carga
  loading: false
})

// Acciones para modificar el estado
const actions = {
  setSession(sessionData) {
    state.isLoggedIn = sessionData.active || false
    state.currentUser = sessionData.user || ''
    state.mountId = sessionData.mountId || ''
    state.diskPath = sessionData.diskPath || ''
    state.uid = sessionData.uid || -1
    state.gid = sessionData.gid || -1
  },
  
  clearSession() {
    state.isLoggedIn = false
    state.currentUser = ''
    state.mountId = ''
    state.diskPath = ''
    state.uid = -1
    state.gid = -1
  },
  
  setMountedPartitions(partitions) {
    state.mountedPartitions = partitions || []
    // Si no hay selección y hay particiones, seleccionar la primera
    if (!state.selectedMountId && state.mountedPartitions.length > 0) {
      state.selectedMountId = state.mountedPartitions[0].id
    }
  },
  
  selectMountId(mountId) {
    state.selectedMountId = mountId
  },
  
  setLoading(loading) {
    state.loading = loading
  }
}

// Getters para acceder al estado
const getters = {
  getSelectedMount() {
    return state.mountedPartitions.find(p => p.id === state.selectedMountId) || null
  },
  
  getActiveMountId() {
    // Prioridad: selección manual > primera montada
    if (state.selectedMountId) return state.selectedMountId
    if (state.mountedPartitions.length > 0) return state.mountedPartitions[0].id
    return state.mountId || ''
  }
}

export const store = {
  state: readonly(state),
  actions,
  getters
}
