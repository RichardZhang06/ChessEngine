import type { Piece } from './chess'

const NAMES = { p: 'pawn', n: 'knight', b: 'bishop', r: 'rook', q: 'queen', k: 'king' }

// Eagerly collects every image under assets/pieces so a missing file doesn't break the build.
// Folder = skin: assets/pieces/<skin>/white-pawn.png. Files directly in assets/pieces form the "default" skin.
const files = import.meta.glob('./assets/pieces/**/*.{png,svg,webp,jpg,jpeg}', {
  eager: true,
  query: '?url',
  import: 'default',
}) as Record<string, string>

const sets: Record<string, Record<string, string>> = {}
for (const [path, url] of Object.entries(files)) {
  const parts = path.replace('./assets/pieces/', '').split('/')
  const set = parts.length > 1 ? parts[0] : 'default'
  const name = parts[parts.length - 1].replace(/\.\w+$/, '')
  ;(sets[set] ??= {})[name] = url
}

export const pieceSets = Object.keys(sets).sort()

export const pieceImage = (p: Piece, set: string): string | undefined =>
  sets[set]?.[`${p.color}-${NAMES[p.type]}`]

export const pieceLabel = (p: Piece) => `${p.color} ${NAMES[p.type]}`
