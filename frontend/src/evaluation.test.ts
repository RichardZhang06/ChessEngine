import { describe, expect, it } from 'vitest'
import type { AnalysisResult } from './api'
import { formatEvaluation, toWhitePerspective, whiteShare } from './evaluation'

const analysis = (over: Partial<AnalysisResult>): AnalysisResult => ({
  bestmove: 'e2e4',
  depth: 8,
  score_cp: 0,
  mate: null,
  pv: ['e2e4'],
  ...over,
})

describe('toWhitePerspective', () => {
  it('keeps the score when White is to move', () => {
    expect(toWhitePerspective(analysis({ score_cp: 35 }), 'white')).toEqual({ cp: 35, mate: null })
  })

  it('flips the score when Black is to move (the engine scores for the side to move)', () => {
    expect(toWhitePerspective(analysis({ score_cp: 35 }), 'black')).toEqual({ cp: -35, mate: null })
    expect(toWhitePerspective(analysis({ score_cp: -120 }), 'black')).toEqual({ cp: 120, mate: null })
  })

  it('reports mates for the right side', () => {
    expect(toWhitePerspective(analysis({ score_cp: null, mate: 3 }), 'white')).toEqual({ cp: null, mate: 3 })
    expect(toWhitePerspective(analysis({ score_cp: null, mate: 3 }), 'black')).toEqual({ cp: null, mate: -3 })
    expect(toWhitePerspective(analysis({ score_cp: null, mate: -2 }), 'white')).toEqual({ cp: null, mate: -2 })
    expect(toWhitePerspective(analysis({ score_cp: null, mate: -2 }), 'black')).toEqual({ cp: null, mate: 2 })
  })

  it('has no evaluation when the engine gave no score', () => {
    expect(toWhitePerspective(analysis({ score_cp: null, mate: null }), 'white')).toBeNull()
  })
})

describe('formatEvaluation', () => {
  it('formats centipawns as pawns with a sign', () => {
    expect(formatEvaluation({ cp: 35, mate: null })).toBe('+0.35')
    expect(formatEvaluation({ cp: -120, mate: null })).toBe('-1.20')
    expect(formatEvaluation({ cp: 0, mate: null })).toBe('0.00')
    expect(formatEvaluation({ cp: 1050, mate: null })).toBe('+10.50')
  })

  it('formats mates', () => {
    expect(formatEvaluation({ cp: null, mate: 3 })).toBe('M3')
    expect(formatEvaluation({ cp: null, mate: -1 })).toBe('-M1')
  })

  it('shows a dash when there is nothing to show', () => {
    expect(formatEvaluation(null)).toBe('–')
  })
})

describe('whiteShare', () => {
  it('is half for an equal or unknown position', () => {
    expect(whiteShare({ cp: 0, mate: null })).toBe(0.5)
    expect(whiteShare(null)).toBe(0.5)
  })

  it('grows with White\'s lead, symmetrically, and stays inside (0, 1) for centipawn scores', () => {
    const shares = [-900, -300, -100, 0, 100, 300, 900].map((cp) => whiteShare({ cp, mate: null }))
    expect([...shares].sort((a, b) => a - b)).toEqual(shares)
    expect(shares[0]).toBeGreaterThan(0)
    expect(shares[6]).toBeLessThan(1)
    expect(whiteShare({ cp: 250, mate: null }) + whiteShare({ cp: -250, mate: null })).toBeCloseTo(1)
    expect(whiteShare({ cp: 400, mate: null })).toBeCloseTo(0.909, 2)
  })

  it('fills or empties the bar for mates', () => {
    expect(whiteShare({ cp: null, mate: 2 })).toBe(1)
    expect(whiteShare({ cp: null, mate: -2 })).toBe(0)
  })
})
