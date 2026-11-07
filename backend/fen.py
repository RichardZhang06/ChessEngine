from game import Game

# holds all game data in actual format that the chess engine needs
class Board:
    def __init__(self, Game_input: Game):
        # each of the 12 attributes below are bitboard for piece positions.
        # 1 indicates a piece is there, 0 indicates not.
        # it is a row dominant representation of the chess board, starting
        # at a1 and ending at h8, so a total of 64 bits. 
        # not quite following fen format of board due to simpler computation.
        self.white_pawns: int = 0
        self.white_bishops: int = 0
        self.white_knights: int = 0
        self.white_rooks: int = 0
        self.white_queens: int = 0
        self.white_kings: int = 0

        self.black_pawns: int = 0
        self.black_bishops: int = 0
        self.black_knights: int = 0
        self.black_rooks: int = 0
        self.black_queens:int = 0
        self.black_kings: int = 0

        # bool representing whether white is next to move or not
        self.white_to_move: bool = True

        # rightmost 4 bits are castling rights:
        # 1 means legal, 0 means illegal, position corresponds to FEN "KQkq"
        self.castling_rights: int = 0

        # integer representing en passant square: -1 if not possible, else the
        # board position from 0-63, representing a1-h8.
        self.en_passant_square: int = -1

        # halfmove clock ranges from 0 to 100
        self.halfmove_clock: int = 0

        # number of full moves in the game so far
        self.fullmove_number: int = 0

        # sets correct values based on input game fen
        self.parse_fen(Game_input.fen)
    

    def parse_fen(self, fen: str):
        # splits fen string into required sections
        sections: list[str] = fen.split()
        fen_board: str = sections[0]
        fen_color: str = sections[1]
        fen_castling: str = sections[2]
        fen_en_passant: str = sections[3]
        fen_halfmove: str = sections[4]
        fen_fullmove: str = sections[5]

        # updates easy-to-convert attributes
        self.halfmove_clock = int(fen_halfmove)
        self.fullmove_number = int(fen_fullmove)

        if fen_color != "w":
            # no need to change if not w, as it is initialised to True
            self.white_to_move = False
        
        if fen_en_passant != "-":
            # no need to change if no en passant move, due to initialisation
            # converts <letterNumber> move format to correct board position
            col = ord(fen_en_passant[0]) - ord("a")
            row = int(fen_en_passant[1]) - 1
            # have to offset
            self.en_passant_square = 8 * row + col

        # no need to case on "-", due to initialisation to 0000
        if "K" in fen_castling:
            self.castling_rights = self.castling_rights | 0b1000
        if "Q" in fen_castling:
            self.castling_rights = self.castling_rights | 0b0100
        if "k" in fen_castling:
            self.castling_rights = self.castling_rights | 0b0010
        if "q" in fen_castling:
            self.castling_rights = self.castling_rights | 0b0001

        # some bs to convert the board
        fen_rows: list[str] = fen_board.split("/")
        for row_index in range(8):
            curr_row: str = fen_rows[row_index]
            col_index: int = 0
            for string_index in range(len(curr_row)):
                char: str = curr_row[string_index]
                # offset is how much to shift a 1 bit to get it to the correct
                # index within a bitboard
                offset: int = 8 * (7 - row_index) + col_index
                position_bit: int = 0b1 << offset
                if char == "P":
                    self.white_pawns = self.white_pawns | position_bit
                elif char == "p":
                    self.black_pawns = self.black_pawns | position_bit
                elif char == "B":
                    self.white_bishops = self.white_bishops | position_bit
                elif char == "b":
                    self.black_bishops = self.black_bishops | position_bit
                elif char == "N":
                    self.white_knights = self.white_knights | position_bit
                elif char == "n":
                    self.black_knights = self.black_knights | position_bit
                elif char == "R":
                    self.white_rooks = self.white_rooks | position_bit
                elif char == "r":
                    self.black_rooks = self.black_rooks | position_bit
                elif char == "Q":
                    self.white_queens = self.white_queens | position_bit
                elif char == "q":
                    self.black_queens = self.black_queens | position_bit
                elif char == "K":
                    self.white_kings = self.white_kings | position_bit
                elif char == "k":
                    self.black_kings = self.black_kings | position_bit
                else:
                    # if not a letter, must be a number for empty squares
                    col_index += int(char)
                    continue

                # only reach here if the character was a piece, so add 1
                col_index += 1
    

    def gen_fen(self):
        # initialising library to easily check all boards
        pieces = [(self.white_pawns,   "P"), (self.white_knights, "N"),
                  (self.white_bishops, "B"), (self.white_rooks,   "R"),
                  (self.white_queens,  "Q"), (self.white_kings,   "K"),
                  (self.black_pawns,   "p"), (self.black_knights, "n"),
                  (self.black_bishops, "b"), (self.black_rooks,   "r"),
                  (self.black_queens,  "q"), (self.black_kings,   "k")]
        # rows is a matrix of strings, representing each row in FEN notation
        rows = []
        # loop through all rows, from 8 to 1 (FEN notation)
        for row_index in range(7, -1, -1):
            row_str = ""
            # gap is number of empty spaces until next piece or end of row
            gap = 0
            # loop through all columns in each row, from 1 to 8 (FEN notation)
            for col_index in range(8):
                # offsetting comparison bit to correct bit of each bitboard
                offset = row_index * 8 + col_index
                bit = 1 << offset
                curr_piece_char = ""
                for (bitboard, piece_char) in pieces:
                    # and operation on bits, will return 1 (True) if piece in 
                    # the specified position
                    if bitboard & bit:
                        # specifying which piece to add to the string
                        curr_piece_char = piece_char
                        break
                # if curr_piece_char unchanged, no piece there, so increment gap
                if curr_piece_char == "":
                    gap += 1
                    continue
                # if curr_piece_char is a symbol, add empty squares and piece
                if gap != 0:
                    row_str += str(gap)
                    gap = 0
                row_str += curr_piece_char
            # add any trailing empty squares, as loop above only adds to string
            # if a character is reached
            if gap != 0:
                row_str += str(gap)
            rows.append(row_str)
        # produces single line string that represents board found above
        fen_board = "/".join(rows)

        # getting character for whose turn is next
        if self.white_to_move == True:
            color = "w"
        else:
            color = "b"

        # converting castling rights to KQkq format string
        castling = ""
        if self.castling_rights & 0b1000:
            castling += "K"
        if self.castling_rights & 0b0100:
            castling += "Q"
        if self.castling_rights & 0b0010:
            castling += "k"
        if self.castling_rights & 0b0001:
            castling += "q"
        if castling == "":
            castling += "-"
        
        # converting en passant square to letterNumber format
        if self.en_passant_square == -1:
            en_passant = "-"
        else:
            col = self.en_passant_square % 8
            row = self.en_passant_square // 8
            en_passant = chr(ord('a') + col) + str(row + 1)
        
        # direct int to string conversion for these values
        halfmove = str(self.halfmove_clock)
        fullmove = str(self.fullmove_number)

        # joins all parts together into one string, separated by spaces
        return f"{fen_board} {color} {castling} {en_passant} {halfmove} {fullmove}"


