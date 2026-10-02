import { reactive } from 'vue'

const state = reactive({
  // Estado de sesión
  isLoggedIn: false,
  currentUser: '',
  mountId: '',
  diskPath: '',
  uid: -1,
  gid: -1,
  group: '',
  
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
    state.isLoggedIn = sessionData.active === true
    state.currentUser = sessionData.user || ''
    state.mountId = sessionData.mountId || ''
    state.diskPath = sessionData.diskPath || ''
    state.uid = sessionData.uid ?? -1
    state.gid = sessionData.gid ?? -1
    state.group = sessionData.group || ''
  },
  
  clearSession() {
    state.isLoggedIn = false
    state.currentUser = ''
    state.mountId = ''
    state.diskPath = ''
    state.uid = -1
    state.gid = -1
    state.group = ''
  },
  
  setMountedPartitions(partitions) {
    state.mountedPartitions = partitions || []
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

// Getters
const getters = {
  getSelectedMount() {
    return state.mountedPartitions.find(p => p.id === state.selectedMountId) || null
  },
  
  getActiveMountId() {
    if (state.selectedMountId) return state.selectedMountId
    if (state.mountedPartitions.length > 0) return state.mountedPartitions[0].id
    return state.mountId || ''
  }
}

export const store = {
  state,      // ← reactivo directo (NO readonly)
  actions,
  getters
}