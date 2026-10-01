import { createRouter, createWebHistory } from 'vue-router'
import HomeView from '../views/HomeView.vue'
import LoginView from '../views/LoginView.vue'
import VisualizerView from '../views/VisualizerView.vue'
import FileSystemView from '../views/FileSystemView.vue'
import JournalView from '../views/JournalView.vue'
import LossView from '../views/LossView.vue'

const routes = [
  {
    path: '/',
    name: 'home',
    component: HomeView,
    meta: { title: 'Home' }
  },
  {
    path: '/login',
    name: 'login',
    component: LoginView,
    meta: { title: 'Login' }
  },
  {
    path: '/visualizer',
    name: 'visualizer',
    component: VisualizerView,
    meta: { title: 'Visualizador' }
  },
  {
    path: '/files',
    name: 'files',
    component: FileSystemView,
    meta: { title: 'Archivos' }
  },
  {
    path: '/journal',
    name: 'journal',
    component: JournalView,
    meta: { title: 'Journaling' }
  },
  {
    path: '/loss',
    name: 'loss',
    component: LossView,
    meta: { title: 'LOSS' }
  },
  {
    path: '/:pathMatch(.*)*',
    redirect: '/'
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

router.beforeEach((to, from, next) => {
  document.title = `EXT2 Analyzer - ${to.meta.title || 'Home'}`
  next()
})

export default router