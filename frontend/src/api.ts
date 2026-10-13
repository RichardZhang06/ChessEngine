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

/** Engine analysis of a position. Scores are from the point of view of the side to move. */
export interface AnalysisResult {
  bestmove: string | null
  depth: number | null
  score_cp: number | null // centipawns, null when the engine reports a forced mate instead
  mate: number | null // moves until mate; negative when the side to move is the one getting mated
  pv: string[]
}

export class ApiError extends Error {
  status: number
  constructor(message: string, status: number) {
    super(message)
    this.status = status
  }
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
    throw new ApiError(detail || messages[res.status] || `Request failed (${res.status})`, res.status)
  }
  return res.json()
}

export const createGame = (color: Color) =>
  request<GameState & { game_id: string }>('/games', { color })

export const submitMove = (id: string, move: string) =>
  request<GameState>(`/games/${id}/moves`, { action: 'move', move })

export const resign = (id: string) =>
  request<GameState>(`/games/${id}/moves`, { action: 'resign' })

export const analyze = (id: string, movetimeMs?: number) =>
  request<AnalysisResult>(`/games/${id}/analysis`, movetimeMs ? { movetime_ms: movetimeMs } : {})
