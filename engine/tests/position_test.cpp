#include "position.h"
#include "test_util.h"

static const char* START = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int main() {
  Position pos;
  CHECK_EQ(pos.fen(), std::string(START));
  CHECK_EQ(pos.key(), pos.compute_key());

  // FEN round trips
  const char* fens[] = {
      START,
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      "rnbqkb1r/pp1p1ppp/5n2/2pPp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 4",
      "4k3/8/8/8/8/8/8/4K2R b K - 12 40",
  };
  for (const char* f : fens) {
    Position p;
    CHECK(p.set_fen(f));
    CHECK_EQ(p.fen(), std::string(f));
    CHECK_EQ(p.key(), p.compute_key());
  }

  // Malformed FENs are rejected and leave the position unchanged
  const char* bad[] = {"", "garbage", "8/8/8/8/8/8/8/8 w - - 0 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBN w KQkq - 0 1",
                       "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
                       "4k3/8/8/8/8/8/4q3/4K3 b - - 0 1" /* side not to move is in check */};
  for (const char* f : bad) {
    Position p;
    CHECK(!p.set_fen(f));
    CHECK_EQ(p.fen(), std::string(START));
  }

  // make/unmake restores everything, including the hash
  {
    Position p;
    p.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    std::string before = p.fen();
    Key key = p.key();
    Move castle = make_move(E1, G1, KING_CASTLE);
    p.make(castle);
    CHECK(p.piece_on(G1) == make_piece(WHITE, KING));
    CHECK(p.piece_on(F1) == make_piece(WHITE, ROOK));
    CHECK_EQ(p.key(), p.compute_key());
    p.unmake();
    CHECK_EQ(p.fen(), before);
    CHECK_EQ(p.key(), key);
  }

  // en passant, promotion and null move
  {
    Position p;
    p.set_fen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    std::string before = p.fen();
    Key key = p.key();
    p.make(make_move(36, 43, EN_PASSANT));  // e5xd6
    CHECK(p.piece_on(35) == NO_PIECE);       // d5 pawn removed
    CHECK(p.piece_on(43) == make_piece(WHITE, PAWN));
    CHECK_EQ(p.key(), p.compute_key());
    p.unmake();
    CHECK_EQ(p.fen(), before);
    CHECK_EQ(p.key(), key);

    p.make_null();
    CHECK(p.side_to_move() == BLACK);
    CHECK_EQ(p.key(), p.compute_key());
    p.unmake_null();
    CHECK_EQ(p.fen(), before);
    CHECK_EQ(p.key(), key);
  }
  {
    Position p;
    p.set_fen("1n2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    std::string before = p.fen();
    p.make(make_move(48, 57, PROMO_CAPTURE_Q));  // axb8=Q
    CHECK(p.piece_on(57) == make_piece(WHITE, QUEEN));
    CHECK_EQ(p.key(), p.compute_key());
    p.unmake();
    CHECK_EQ(p.fen(), before);
  }

  TEST_MAIN_END();
}
