export interface BoardTheme {
  name: string
  light: string
  dark: string
  mark: string // right-click highlight / arrow color
}

export const boardThemes: BoardTheme[] = [
  { name: 'Classic', light: '#f0d9b5', dark: '#b58863', mark: '#d64541' },
  { name: 'Green', light: '#eeeed2', dark: '#769656', mark: '#e0463c' },
  { name: 'Blue', light: '#dee3e6', dark: '#8ca2ad', mark: '#e67e22' },
  { name: 'Purple', light: '#e8dff5', dark: '#9b7bc4', mark: '#2fb36d' },
  { name: 'Gray', light: '#d9d9d9', dark: '#7a7a7a', mark: '#e0463c' },
]
