export interface BoardTheme {
  name: string
  light: string
  dark: string
}

export const boardThemes: BoardTheme[] = [
  { name: 'Classic', light: '#f0d9b5', dark: '#b58863' },
  { name: 'Green', light: '#eeeed2', dark: '#769656' },
  { name: 'Blue', light: '#dee3e6', dark: '#8ca2ad' },
  { name: 'Purple', light: '#e8dff5', dark: '#9b7bc4' },
  { name: 'Gray', light: '#d9d9d9', dark: '#7a7a7a' },
]
