import { useEffect, useState } from 'react'
import { createGame, resign, submitMove, type Color, type GameState } from './api'
import { applyMove, parseFen, squareCoords, squareName, type Board } from './chess'
import { backgrounds } from './backgrounds'
import { boardThemes } from './boards'
import { pieceImage, pieceLabel, pieceSets } from './pieces'
import './App.css'

const START_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'

function statusText(g: GameState, human: Color): string {
  switch (g.status) {
    case 'checkmate':
      return `Checkmate — ${g.result === (human === 'white' ? '1-0' : '0-1') ? 'you win!' : 'you lose.'}`
    case 'stalemate':
      return 'Stalemate — draw.'
    case 'draw':
      return 'Draw.'
    case 'resigned':
      return 'You resigned.'
    default:
      return g.turn === human ? 'Your turn' : 'Engine is thinking…'
  }
}

function load(key: string, fallback: string, valid: string[]) {
  try {
    const v = localStorage.getItem(key)
    if (v && valid.includes(v)) return v
  } catch {
    /* storage unavailable */
  }
  return fallback
}

function save(key: string, value: string) {
  try {
    localStorage.setItem(key, value)
  } catch {
    /* storage unavailable */
  }
}

function App() {
  const [settingsOpen, setSettingsOpen] = useState(false)
  const [pieceSet, setPieceSet] = useState(() =>
    load('pieceSet', pieceSets[0] ?? 'default', pieceSets),
  )
  const [boardName, setBoardName] = useState(() =>
    load('boardTheme', boardThemes[0].name, boardThemes.map((t) => t.name)),
  )
  const [bgName, setBgName] = useState(() =>
    load('background', backgrounds[0].name, backgrounds.map((b) => b.name)),
  )
  const background = backgrounds.find((b) => b.name === bgName) ?? backgrounds[0]

  useEffect(() => {
    const root = document.documentElement
    root.style.setProperty('--bg', background.bg)
    root.style.setProperty('--text', background.text)
    root.style.setProperty('--text-h', background.text)
    document.body.style.background = background.bg
  }, [background])

  const theme = boardThemes.find((t) => t.name === boardName) ?? boardThemes[0]

  const [human, setHuman] = useState<Color>('white')
  const [game, setGame] = useState<GameState | null>(null)
  const [gameId, setGameId] = useState<string | null>(null)
  const [board, setBoard] = useState<Board>(parseFen(START_FEN))
  const [selected, setSelected] = useState<string | null>(null)
  const [pendingPromo, setPendingPromo] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)

  const over = game !== null && game.status !== 'playing'
  const canMove = game !== null && !over && !busy && game.turn === human

  function sync(g: GameState) {
    setGame(g)
    setBoard(parseFen(g.fen))
  }

  async function start(color: Color) {
    setHuman(color)
    setGame(null)
    setGameId(null)
    setBoard(parseFen(START_FEN))
    setBusy(true)
    setError(null)
    try {
      const g = await createGame(color)
      setGameId(g.game_id)
      setSelected(null)
      sync(g)
    } catch (e) {
      setError((e as Error).message)
    } finally {
      setBusy(false)
    }
  }

  async function play(move: string) {
    if (!gameId) return
    const prev = board
    setBoard(applyMove(board, move))
    setSelected(null)
    setBusy(true)
    setError(null)
    try {
      sync(await submitMove(gameId, move))
    } catch (e) {
      setBoard(prev)
      setError((e as Error).message)
    } finally {
      setBusy(false)
    }
  }

  function onSquare(sq: string) {
    if (!canMove) return
    const { file, rankIdx } = squareCoords(sq)
    const piece = board[rankIdx][file]
    if (selected === null || (piece && piece.color === human)) {
      setSelected(piece && piece.color === human ? sq : null)
      return
    }
    if (selected === sq) return setSelected(null)
    const moving = squareCoords(selected)
    const p = board[moving.rankIdx][moving.file]
    const lastRank = human === 'white' ? 0 : 7
    if (p?.type === 'p' && rankIdx === lastRank) setPendingPromo(selected + sq)
    else play(selected + sq)
  }

  async function onResign() {
    if (!gameId) return
    setBusy(true)
    try {
      sync(await resign(gameId))
    } catch (e) {
      setError((e as Error).message)
    } finally {
      setBusy(false)
    }
  }

  useEffect(() => {
    start('white')
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  const ranks = [...Array(8).keys()]
  const files = [...Array(8).keys()]
  if (human === 'black') {
    ranks.reverse()
    files.reverse()
  }
  const last = game?.last_move
  const pairs: string[][] = []
  for (let i = 0; i < (game?.moves.length ?? 0); i += 2) pairs.push(game!.moves.slice(i, i + 2))

  return (
    <main className="app">
      <div className="layout">
        <div>
          <div
            className="board"
            style={{ '--light': theme.light, '--dark': theme.dark } as React.CSSProperties}
          >
            {ranks.map((r) =>
              files.map((f) => {
                const sq = squareName(f, r)
                const piece = board[r][f]
                const img = piece && pieceImage(piece, pieceSet)
                const cls = [
                  'square',
                  (r + f) % 2 === 0 ? 'light' : 'dark',
                  sq === selected && 'selected',
                  last && (sq === last.slice(0, 2) || sq === last.slice(2, 4)) && 'last',
                ]
                return (
                  <div key={sq} className={cls.filter(Boolean).join(' ')} onClick={() => onSquare(sq)}>
                    {f === files[0] && <span className="coord rank">{8 - r}</span>}
                    {r === ranks[7] && <span className="coord file">{'abcdefgh'[f]}</span>}
                    {piece &&
                      (img ? (
                        <img src={img} alt={pieceLabel(piece)} draggable={false} />
                      ) : (
                        <span className="missing">{piece.type}</span>
                      ))}
                  </div>
                )
              }),
            )}
          </div>
        </div>
        <aside className="side">
          <label className="field">
            Play as{' '}
            <select value={human} disabled={busy} onChange={(e) => start(e.target.value as Color)}>
              <option value="white">White</option>
              <option value="black">Black</option>
            </select>
          </label>
          <h2>{game ? statusText(game, human) : 'Starting…'}</h2>
          {error && <p className="error">{error}</p>}
          <div className="moves">
            {pairs.map((p, i) => (
              <div key={i}>
                {i + 1}. {p.join('  ')}
              </div>
            ))}
          </div>
          <div className="row actions">
            <button disabled={busy} onClick={() => start(human)}>New game</button>
            <button disabled={busy || !game || over} onClick={onResign}>Resign</button>
          </div>
          <button className="gear" onClick={() => setSettingsOpen((o) => !o)}>
            ⚙ Settings
          </button>
          {settingsOpen && (
            <div className="settings">
              <label className="field">
                Piece skin
                <select
                  value={pieceSet}
                  onChange={(e) => {
                    setPieceSet(e.target.value)
                    save('pieceSet', e.target.value)
                  }}
                >
                  {pieceSets.length === 0 && <option value="default">(none found)</option>}
                  {pieceSets.map((s) => (
                    <option key={s} value={s}>{s}</option>
                  ))}
                </select>
              </label>
              <label className="field">
                Board skin
                <select
                  value={boardName}
                  onChange={(e) => {
                    setBoardName(e.target.value)
                    save('boardTheme', e.target.value)
                  }}
                >
                  {boardThemes.map((t) => (
                    <option key={t.name} value={t.name}>{t.name}</option>
                  ))}
                </select>
              </label>
              <label className="field">
                Background
                <select
                  value={bgName}
                  onChange={(e) => {
                    setBgName(e.target.value)
                    save('background', e.target.value)
                  }}
                >
                  {backgrounds.map((b) => (
                    <option key={b.name} value={b.name}>{b.name}</option>
                  ))}
                </select>
              </label>
            </div>
          )}
        </aside>
      </div>
      {pendingPromo && (
        <div className="promo">
          {(['q', 'r', 'b', 'n'] as const).map((t) => {
            const piece = { type: t, color: human }
            const img = pieceImage(piece, pieceSet)
            return (
              <button
                key={t}
                onClick={() => {
                  const m = pendingPromo + t
                  setPendingPromo(null)
                  play(m)
                }}
              >
                {img ? <img src={img} alt={pieceLabel(piece)} /> : t}
              </button>
            )
          })}
        </div>
      )}
    </main>
  )
}

export default App
