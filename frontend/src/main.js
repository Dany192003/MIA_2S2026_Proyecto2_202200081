import { createApp } from 'vue'
import './style.css'
import App from './App.vue'
import router from './router'
import { store } from './store'

const app = createApp(App)

app.provide('store', store)
app.use(router)

app.mount('#app')