import { formatEvaluation, whiteShare, type Evaluation } from './evaluation'

interface Props {
  evaluation: Evaluation | null
  flipped: boolean // true when the board is shown from Black's side, so White's part of the bar goes on top
}

export function EvalBar({ evaluation, flipped }: Props) {
  const white = Math.round(whiteShare(evaluation) * 1000) / 10
  const label = formatEvaluation(evaluation)
  return (
    <div
      className={`evalbar${flipped ? ' flipped' : ''}`}
      role="img"
      aria-label={`Evaluation ${label}`}
      title={`Evaluation ${label} (positive favors White)`}
    >
      <div className="evalbar-white" style={{ height: `${white}%` }} />
      <span className={`evalbar-label ${white >= 50 ? 'on-white' : 'on-black'}`}>{label}</span>
    </div>
  )
}
