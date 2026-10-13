import { describe, expect, it } from 'vitest'
import { applyMove, parseFen, squareCoords, squareName } from './chess'

const START = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'

describe('parseFen', () => {
  it('reads the starting position (rank 8 first)', () => {
    const board = parseFen(START)
    expect(board).toHaveLength(8)
    expect(board[0][0]).toEqual({ type: 'r', color: 'black' })
    expect(board[0][4]).toEqual({ type: 'k', color: 'black' })
    expect(board[7][3]).toEqual({ type: 'q', color: 'white' })
    expect(board[6][0]).toEqual({ type: 'p', color: 'white' })
    expect(board[4].every((sq) => sq === null)).toBe(true)
  })

  it('expands empty-square counts', () => {
    const board = parseFen('4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1')
    expect(board[7].map((p) => p?.type ?? '.').join('')).toBe('r...k..r') // lower-case: the type letter ignores color
    expect(board[7][0]).toEqual({ type: 'r', color: 'white' })
    expect(board[7][7]).toEqual({ type: 'r', color: 'white' })
    expect(board[0][4]).toEqual({ type: 'k', color: 'black' })
  })
})

describe('squares', () => {
  it('maps between names and board indexes', () => {
    expect(squareName(0, 7)).toBe('a1')
    expect(squareName(4, 4)).toBe('e4')
    expect(squareName(7, 0)).toBe('h8')
    expect(squareCoords('e2')).toEqual({ file: 4, rankIdx: 6 })
    expect(squareCoords('a8')).toEqual({ file: 0, rankIdx: 0 })
  })
})

describe('applyMove', () => {
  it('moves a piece and leaves the original board untouched', () => {
    const board = parseFen(START)
    const next = applyMove(board, 'e2e4')
    expect(next[6][4]).toBeNull()
    expect(next[4][4]).toEqual({ type: 'p', color: 'white' })
    expect(board[6][4]).toEqual({ type: 'p', color: 'white' })
  })

  it('captures by overwriting the target square', () => {
    const next = applyMove(parseFen('4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1'), 'e4d5')
    expect(next[3][3]).toEqual({ type: 'p', color: 'white' })
    expect(next[4][4]).toBeNull()
  })

  it('applies promotions', () => {
    const next = applyMove(parseFen('4k3/P7/8/8/8/8/8/4K3 w - - 0 1'), 'a7a8q')
    expect(next[0][0]).toEqual({ type: 'q', color: 'white' })
    expect(next[1][0]).toBeNull()
  })
})
