import type { AnalysisResult, Color } from './api'

/** An evaluation from White's point of view: positive is good for White. Exactly one of the fields is set. */
export interface Evaluation {
  cp: number | null
  mate: number | null // moves until mate; positive: White mates, negative: Black mates
}

/** The engine scores from the side to move's point of view; flip it when Black is to move. */
export function toWhitePerspective(result: AnalysisResult, turn: Color): Evaluation | null {
  const sign = turn === 'white' ? 1 : -1
  if (result.mate !== null && result.mate !== 0) return { cp: null, mate: sign * result.mate }
  if (result.score_cp !== null) return { cp: sign * result.score_cp, mate: null }
  return null
}

/** "+0.35", "-1.20", "0.00", or "M3" / "-M3" for forced mates. */
export function formatEvaluation(e: Evaluation | null): string {
  if (!e) return '–'
  if (e.mate !== null) return `${e.mate < 0 ? '-' : ''}M${Math.abs(e.mate)}`
  const pawns = (e.cp ?? 0) / 100
  return pawns > 0 ? `+${pawns.toFixed(2)}` : pawns < 0 ? pawns.toFixed(2) : '0.00'
}

/** Fraction of the bar given to White, from 0 to 1: 0.5 when equal, approaching 1 as White's lead grows. */
export function whiteShare(e: Evaluation | null): number {
  if (!e) return 0.5
  if (e.mate !== null) return e.mate > 0 ? 1 : 0
  return 1 / (1 + 10 ** (-(e.cp ?? 0) / 400))
}
