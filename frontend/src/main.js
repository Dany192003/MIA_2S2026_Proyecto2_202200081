import { createApp } from 'vue'
import './style.css'
import App from './App.vue'
import { store } from './store'

const app = createApp(App)

// ✅ Proveer el store a toda la aplicación
app.provide('store', store)

app.mount('#app')