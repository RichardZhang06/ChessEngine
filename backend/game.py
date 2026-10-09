import json


# class that contains all info about a current game
# documentation can be found in README.md
class Game:
    def __init__(self, game_id, moves, last_move, fen, turn, status, result):
        # game id is the identifier for the game
        self.game_id: str = game_id

        # moves is a list of str moves, last_move is the str last move
        self.moves: list = moves
        self.last_move: str | None = last_move

        # fen is Forsyth-Edwards Notation of the current board state
        self.fen: str = fen

        # turn is whose turn it is to play next
        self.turn: str = turn

        # status is a string indicating current game state
        self.status: str = status

        # string indicating the result "x-y", x is white's score, 
        # y is black's score, with loss = 0, draw = 1/2, win = 1
        self.result: str | None = result
    

    # formats a game's data into a dictionary to be stored in json file
    @staticmethod
    def format_game_data(game: Game):
        game_data = {}
        game_data["game_id"] = game.game_id
        game_data["moves"] = game.moves
        game_data["last_move"] = game.last_move
        game_data["fen"] = game.fen
        game_data["turn"] = game.turn
        game_data["status"] = game.status
        game_data["result"] = game.result
        return game_data


    # uses game_id and games.json to return a dictionary of the game data needed
    @staticmethod
    def get_game_data(game: Game):
        with open("games.json", "r") as games_file:
            games_dict = json.load(games_file)
        game_data = games_dict[game.game_id]
        return game_data


    # creates a new game based on player's color input
    @staticmethod
    def create(color: str):
        # games.json keeps all games and their data stored under a game id
        with open("games.json", "r") as games_file:
            games_dict = json.load(games_file)
        
        # game_id is simply how many games were made before
        game_id = str(len(games_dict))
        # empty values for these
        moves = []
        last_move = None
        # initial board state in FEN
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
        # initial values
        turn = "white"
        status = "playing"
        result = None

        # IMPLEMENT MAKING MOVE FIRST BEFORE PASSING IT BACK IF PLAYER BLACK
        # BELOW IS TEMP E2E3 PAWN MOVE
        if color == "black":
            last_mpve = "e2e3"
            moves.append(last_move)
            fen = "rnbqkbnr/pppppppp/8/8/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 1"
            turn = "black"
        
        # initialises instance of game class
        new_game = Game(game_id, moves, last_move, fen, turn, status, result)

        # stores all game data in the games.json file
        game_data = Game.format_game_data(new_game)
        games_dict[game_id] = game_data
        # rewrites games.json to include new game data
        with open("games.json", "w") as games_file:
            json.dump(games_dict, games_file)
        return new_game


