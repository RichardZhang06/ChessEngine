from fastapi import FastAPI
from pydantic import BaseModel
from game import Game


# start game request info
class GameStart(BaseModel):
    color: str


# makes FastAPI instance
app = FastAPI()


# creating new game
@app.post("/games")
async def read_root(start_request: GameStart):
    game = Game.create(start_request.color)
    print(game.game_id)
    return Game.get_game_data(game)

