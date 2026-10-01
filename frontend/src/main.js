import { createApp } from 'vue'
import './style.css'
import App from './App.vue'
import router from './router'
import { store } from './store'

const app = createApp(App)

// ✅ Proveer el store a toda la aplicación
app.provide('store', store)

// ✅ Usar Vue Router
app.use(router)

app.mount('#app')