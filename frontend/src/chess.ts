export type PieceChar = 'p' | 'n' | 'b' | 'r' | 'q' | 'k'
export interface Piece {
  type: PieceChar
  color: 'white' | 'black'
}
export type Board = (Piece | null)[][] // [rank index 0 = rank 8][file 0 = a]

export function parseFen(fen: string): Board {
  return fen
    .split(' ')[0]
    .split('/')
    .map((row) => {
      const out: (Piece | null)[] = []
      for (const ch of row) {
        if (/\d/.test(ch)) for (let i = 0; i < +ch; i++) out.push(null)
        else
          out.push({
            type: ch.toLowerCase() as PieceChar,
            color: ch === ch.toUpperCase() ? 'white' : 'black',
          })
      }
      return out
    })
}

export const squareName = (file: number, rankIdx: number) =>
  'abcdefgh'[file] + (8 - rankIdx)

export const squareCoords = (sq: string) => ({
  file: 'abcdefgh'.indexOf(sq[0]),
  rankIdx: 8 - +sq[1],
})

/** Naive move application for instant feedback; the server's FEN replaces it. */
export function applyMove(board: Board, move: string): Board {
  const next = board.map((r) => [...r])
  const from = squareCoords(move.slice(0, 2))
  const to = squareCoords(move.slice(2, 4))
  const piece = next[from.rankIdx][from.file]
  next[from.rankIdx][from.file] = null
  next[to.rankIdx][to.file] = piece
    ? { ...piece, type: (move[4] as PieceChar) ?? piece.type }
    : null
  return next
}
