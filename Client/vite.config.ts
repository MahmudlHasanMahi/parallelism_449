import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'
import type { Plugin } from 'vite'

function fixCosmographCss(): Plugin {
  return {
    name: 'fix-cosmograph-css',
    enforce: 'pre',
    resolveId(id) {
      if (id === '@/cosmograph/style.module.css') {
        return '\0cosmograph-empty-css'
      }
    },
    load(id) {
      if (id === '\0cosmograph-empty-css') {
        return 'export default {}'
      }
    },
  }
}

export default defineConfig({
  plugins: [fixCosmographCss(), react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
})