import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { analyze, ApiError, createGame, resign, submitMove } from './api'

// What the backend promises in the README. These mocked tests pin down what the client sends and how it reads replies.
const STATE = {
  moves: ['e2e4', 'e7e5'],
  last_move: 'e7e5',
  fen: 'rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2',
  turn: 'white',
  status: 'playing',
  result: null,
}

const fetchMock = vi.fn()

function reply(status: number, body: unknown) {
  fetchMock.mockResolvedValueOnce(new Response(JSON.stringify(body), { status }))
}

beforeEach(() => vi.stubGlobal('fetch', fetchMock))
afterEach(() => {
  fetchMock.mockReset()
  vi.unstubAllGlobals()
})

function lastRequest() {
  const [url, init] = fetchMock.mock.calls.at(-1)!
  return { url: url as string, method: init.method, headers: init.headers, body: JSON.parse(init.body) }
}

describe('requests', () => {
  it('createGame POSTs the color to /api/games', async () => {
    reply(201, { game_id: 'abc', ...STATE })
    const game = await createGame('black')
    expect(lastRequest()).toMatchObject({ url: '/api/games', method: 'POST', body: { color: 'black' } })
    expect(lastRequest().headers).toMatchObject({ 'Content-Type': 'application/json' })
    expect(game.game_id).toBe('abc')
  })

  it('submitMove POSTs a move action in UCI notation', async () => {
    reply(200, STATE)
    const state = await submitMove('abc', 'e2e4')
    expect(lastRequest()).toMatchObject({
      url: '/api/games/abc/moves',
      method: 'POST',
      body: { action: 'move', move: 'e2e4' },
    })
    expect(state.moves).toEqual(['e2e4', 'e7e5'])
    expect(state.last_move).toBe('e7e5')
  })

  it('submitMove keeps the promotion letter', async () => {
    reply(200, STATE)
    await submitMove('abc', 'e7e8q')
    expect(lastRequest().body.move).toBe('e7e8q')
  })

  it('resign POSTs the resign action to the same endpoint', async () => {
    reply(200, { ...STATE, status: 'resigned', result: '0-1' })
    const state = await resign('abc')
    expect(lastRequest()).toMatchObject({ url: '/api/games/abc/moves', body: { action: 'resign' } })
    expect(state.status).toBe('resigned')
    expect(state.result).toBe('0-1')
  })
})

describe('analysis', () => {
  const RESULT = { bestmove: 'e2e4', depth: 7, score_cp: 31, mate: null, pv: ['e2e4', 'e7e5'] }

  it('POSTs to the analysis endpoint with an optional time limit', async () => {
    reply(200, RESULT)
    const result = await analyze('abc', 300)
    expect(lastRequest()).toMatchObject({ url: '/api/games/abc/analysis', method: 'POST', body: { movetime_ms: 300 } })
    expect(result).toEqual(RESULT)
  })

  it('sends an empty body when no time limit is given', async () => {
    reply(200, RESULT)
    await analyze('abc')
    expect(lastRequest().body).toEqual({})
  })

  it('maps errors like the other calls', async () => {
    reply(404, {})
    await expect(analyze('missing')).rejects.toMatchObject({ status: 404, message: 'Game not found' })
  })
})

describe('errors', () => {
  it.each([
    [400, 'Illegal move'],
    [404, 'Game not found'],
    [409, 'Game is over or not your turn'],
    [500, 'Backend error'],
  ])('maps HTTP %i to a readable message with the status attached', async (status, message) => {
    reply(status, {})
    const error = await submitMove('abc', 'e2e5').catch((e) => e)
    expect(error).toBeInstanceOf(ApiError)
    expect(error.status).toBe(status)
    expect(error.message).toBe(message)
  })

  it('prefers the backend detail message when there is one', async () => {
    reply(400, { detail: 'Illegal move: e2e5' })
    await expect(submitMove('abc', 'e2e5')).rejects.toThrow('Illegal move: e2e5')
  })

  it('copes with an error response that is not JSON', async () => {
    fetchMock.mockResolvedValueOnce(new Response('Bad Gateway', { status: 502 }))
    await expect(createGame('white')).rejects.toThrow('Request failed (502)')
  })
})
