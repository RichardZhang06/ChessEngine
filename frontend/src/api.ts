export type Color = 'white' | 'black'
export type Status = 'playing' | 'checkmate' | 'stalemate' | 'draw' | 'resigned'

export interface GameState {
  game_id?: string
  moves: string[]
  last_move: string | null
  fen: string
  turn: Color
  status: Status
  result: string | null
}

async function request<T>(path: string, body: unknown): Promise<T> {
  const res = await fetch(`/api${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
  if (!res.ok) {
    let detail = ''
    try {
      detail = (await res.json()).detail ?? ''
    } catch {
      /* no body */
    }
    const messages: Record<number, string> = {
      400: 'Illegal move',
      404: 'Game not found',
      409: 'Game is over or not your turn',
      500: 'Backend error',
    }
    throw new Error(detail || messages[res.status] || `Request failed (${res.status})`)
  }
  return res.json()
}

export const createGame = (color: Color) =>
  request<GameState & { game_id: string }>('/games', { color })

export const submitMove = (id: string, move: string) =>
  request<GameState>(`/games/${id}/moves`, { action: 'move', move })

export const resign = (id: string) =>
  request<GameState>(`/games/${id}/moves`, { action: 'resign' })
