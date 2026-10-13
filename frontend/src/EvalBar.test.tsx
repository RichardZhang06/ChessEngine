import { renderToStaticMarkup } from 'react-dom/server'
import { describe, expect, it } from 'vitest'
import { EvalBar } from './EvalBar'

const html = (props: Parameters<typeof EvalBar>[0]) => renderToStaticMarkup(<EvalBar {...props} />)

describe('EvalBar', () => {
  it('shows the score and gives White half the bar for an equal position', () => {
    const out = html({ evaluation: { cp: 0, mate: null }, flipped: false })
    expect(out).toContain('0.00')
    expect(out).toContain('height:50%')
    expect(out).toContain('aria-label="Evaluation 0.00"')
  })

  it('gives White the larger share when White is ahead', () => {
    const out = html({ evaluation: { cp: 400, mate: null }, flipped: false })
    expect(out).toContain('+4.00')
    expect(out).toContain('height:90.9%')
    expect(out).toContain('on-white')
  })

  it('puts the label on the black part when Black is ahead', () => {
    const out = html({ evaluation: { cp: -400, mate: null }, flipped: false })
    expect(out).toContain('-4.00')
    expect(out).toContain('height:9.1%')
    expect(out).toContain('on-black')
  })

  it('shows mates and fills the bar completely', () => {
    expect(html({ evaluation: { cp: null, mate: 3 }, flipped: false })).toContain('height:100%')
    expect(html({ evaluation: { cp: null, mate: -3 }, flipped: false })).toContain('-M3')
  })

  it('is marked as flipped when the board is shown from Black\'s side', () => {
    expect(html({ evaluation: { cp: 0, mate: null }, flipped: true })).toContain('evalbar flipped')
    expect(html({ evaluation: { cp: 0, mate: null }, flipped: false })).not.toContain('flipped')
  })

  it('shows a dash while there is no evaluation yet', () => {
    expect(html({ evaluation: null, flipped: false })).toContain('–')
  })
})
