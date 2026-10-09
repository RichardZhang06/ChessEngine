The start of a Chess Engine!

Notes:
- Ensure we comply with UCI (Universal Chess Interface), documentation is in UCI_protocol.txt
- Frontend is responsible for displaying the board UI and taking user input
- Backend is responsible for game state and game logic as well as UCI

https://ameye.dev/notes/chess-engine/

UCI move documentation:
https://publish.obsidian.md/modern-uci-doc/UCI+Docs/Miscellaneous/Move+Notation

REST API Endpoints
| Method | Endpoint | Purpose | 
| ------ | -------- | ------- |
| POST | /games | Create new game | 
| POST | /games/{id}/moves | Submit a human move and get engine's response | 
| GET | /games/{id} | Get current game state (Implement later) so that we can go back to a game | 

POST /games

Request:
```json
{
    "color": "white"
}
```

Response (201 Created):
```json
{
    "game_id": "abc123",
    "moves": [],
    "last_move": null,
    "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "turn": "white",
    "status": "playing",
    "result": null
}
```

If human chooses black, the engine should make the opening move before returning the response.
FEN (Forsyth-Edwards Notation): https://chessprogramming.org/Forsyth-Edwards_Notation This is a way to encode the current board state in a compact and succinct manner 

POST /games/abc123/moves

Submits a human move or resigns the game.

```json
Request:
{
    "action": "move",
    "move": "e2e4"
}
```

OR 
```json
Request:
{
    "action": "resign"
}
```

Moves use UCI coordinate notation

Response (200 OK):
```json
{
    "moves": ["e2e4", "e7e5"],
    "last_move": "e7e5",
    "fen": "...",
    "turn": "white",
    "status": "playing",
    "result": null
}
```

Response Fields
- moves is the complete history of moves
- last move is the most recent move (engine move typically but if the human move ends the game it will be that move)
- fen is the board state
- turn is the side to move next. 
- status is current game state
- Result is "1-0" if white wins, the other way for black, and "1/2-1/2" in a draw. Default is null when there is no result yet.

| Status | Meaning |
| ------ | ------- |
| playing | Game in progress | 
| checkmate | A player has been checkmated | 
| stalemate | A player is in stalemate |
| draw | The game is a draw by repitition, 50 move rule, insufficient material |
| resigned | A player resigned |

For draw, backend should be accountable for draw conditions. For checkmate, stalemate, draw, and resign, result should not be null.

Error Responses
- 400 Bad Request: Illegal move or malformed request
- 404 Not Found: Game id does not exist
- 409 Conflict: Game is already over or it is not the human player's turn
- 500 Internal Server Error: Backend died

Optional Endpoints

| Method | Endpoint | Purpose | 
| ------ | -------- | ------- |
| GET | /games/{id}/moves | Retrieve move history | 
| POST | /games/{id}/undo | Undo a move | 
| POST | /games/{id}/analysis | Request position analysis | 