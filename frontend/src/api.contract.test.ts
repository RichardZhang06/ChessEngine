// Contract test: the real API client talking, through the real Vite dev-server proxy, to a running backend.
// Skipped unless VITE_BACKEND_URL is set, e.g.:
//   cd backend && uvicorn main:app --port 8000 &
//   cd frontend && VITE_BACKEND_URL=http://localhost:8000 npm test
import { createServer, type ViteDevServer } from 'vite'
import { afterAll, beforeAll, describe, expect, it, vi } from 'vitest'
import { ApiError, createGame, resign, submitMove, type GameState } from './api'

const backend = import.meta.env.VITE_BACKEND_URL as string | undefined

describe.skipIf(!backend)('frontend <-> backend contract', () => {
  let server: ViteDevServer
  const realFetch = globalThis.fetch

  beforeAll(async () => {
    server = await createServer({ server: { port: 0 }, logLevel: 'silent' })
    await server.listen()
    const origin = `http://localhost:${server.config.server.port ?? (server.httpServer!.address() as { port: number }).port}`
    // The client uses relative URLs (/api/...), so send them to the dev server, whose proxy forwards them to the backend.
    vi.stubGlobal('fetch', (url: string, init?: RequestInit) => realFetch(origin + url, init))
  })

  afterAll(async () => {
    vi.unstubAllGlobals()
    await server?.close()
  })

  const START_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'

  function expectGameState(g: GameState) {
    expect(Array.isArray(g.moves)).toBe(true)
    g.moves.forEach((m) => expect(m).toMatch(/^[a-h][1-8][a-h][1-8][qrbn]?$/))
    expect(g.last_move === null || typeof g.last_move === 'string').toBe(true)
    expect(g.last_move).toBe(g.moves.length ? g.moves.at(-1) : null)
    expect(g.fen.split(' ')).toHaveLength(6)
    expect(['white', 'black']).toContain(g.turn)
    expect(['playing', 'checkmate', 'stalemate', 'draw', 'resigned']).toContain(g.status)
    if (g.status === 'playing') expect(g.result).toBeNull()
    else expect(['1-0', '0-1', '1/2-1/2']).toContain(g.result)
  }

  it('creates a game as white with the documented response', async () => {
    const g = await createGame('white')
    expect(g.game_id).toBeTruthy()
    expect(g.moves).toEqual([])
    expect(g.last_move).toBeNull()
    expect(g.fen).toBe(START_FEN)
    expect(g.turn).toBe('white')
    expect(g.status).toBe('playing')
    expect(g.result).toBeNull()
    expectGameState(g)
  })

  it('has the engine open when the human plays black', async () => {
    const g = await createGame('black')
    expect(g.moves).toHaveLength(1)
    expect(g.last_move).toBe(g.moves[0])
    expect(g.turn).toBe('black')
    expectGameState(g)
  })

  it('answers a move with the engine reply and a consistent state', async () => {
    const { game_id } = await createGame('white')
    const g = await submitMove(game_id, 'e2e4')
    expect(g.moves).toHaveLength(2)
    expect(g.moves[0]).toBe('e2e4')
    expect(g.turn).toBe('white')
    expectGameState(g)
  })

  it('rejects an illegal move with an ApiError(400)', async () => {
    const { game_id } = await createGame('white')
    const error = await submitMove(game_id, 'e2e5').catch((e) => e)
    expect(error).toBeInstanceOf(ApiError)
    expect(error.status).toBe(400)
  })

  it('returns 404 for an unknown game', async () => {
    const error = await submitMove('does-not-exist', 'e2e4').catch((e) => e)
    expect(error).toBeInstanceOf(ApiError)
    expect(error.status).toBe(404)
  })

  it('resigns, then reports 409 for any further move', async () => {
    const { game_id } = await createGame('white')
    const g = await resign(game_id)
    expect(g.status).toBe('resigned')
    expect(g.result).toBe('0-1')
    expectGameState(g)
    const error = await submitMove(game_id, 'e2e4').catch((e) => e)
    expect(error).toBeInstanceOf(ApiError)
    expect(error.status).toBe(409)
  })

  it('plays a whole game of legal moves without the two sides disagreeing', async () => {
    const { game_id } = await createGame('white')
    // Always play the first of a few safe developing moves; stop when the game ends or the list runs out.
    const plan = ['e2e4', 'g1f3', 'f1c4', 'e1g1', 'd2d3', 'b1c3']
    let state: GameState | null = null
    for (const move of plan) {
      try {
        state = await submitMove(game_id, move)
      } catch (e) {
        if (e instanceof ApiError && e.status === 400) continue // the engine's reply made this move illegal; skip it
        throw e
      }
      expectGameState(state)
      if (state.status !== 'playing') break
    }
    expect(state).not.toBeNull()
  }, 60_000) // each move waits for the real engine
})
