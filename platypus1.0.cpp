#include <string>
#include <utility>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <future> 
#include <random>
#include <unordered_map>
//fix vector being inefficient for move generation by using a preallocated array and keeping track of the number of moves generated
long long nodes_searched = 0;

const std::uint64_t RANK_1 = 0xFF;
const std::uint64_t RANK_2 = 0xFF00;
const std::uint64_t RANK_3 = 0xFF0000;
const std::uint64_t RANK_4 = 0xFF000000;
const std::uint64_t RANK_5 = 0xFF00000000;
const std::uint64_t RANK_6 = 0xFF0000000000;
const std::uint64_t RANK_7 = 0xFF000000000000;
const std::uint64_t RANK_8 = 0xFF00000000000000;
const std::uint64_t FILE_A = 0x0101010101010101;
const std::uint64_t FILE_B = 0x0202020202020202;
const std::uint64_t FILE_C = 0x0404040404040404;
const std::uint64_t FILE_D = 0x0808080808080808;
const std::uint64_t FILE_E = 0x1010101010101010;
const std::uint64_t FILE_F = 0x2020202020202020;
const std::uint64_t FILE_G = 0x4040404040404040;
const std::uint64_t FILE_H = 0x8080808080808080;

std::uint64_t PASSED_PAWN_MASKS[64];

inline void init_passed_pawn_masks() {
    for (int sq = 0; sq < 64; ++sq) {
        const int file = sq % 8;
        std::uint64_t mask = FILE_A << file;
        if (file > 0) mask |= FILE_A << (file - 1);
        if (file < 7) mask |= FILE_A << (file + 1);
        PASSED_PAWN_MASKS[sq] = mask;
    }
}
const int MATE_SCORE = 100000;
const std::string FILES = "abcdefgh";
const std::uint16_t PIECE_VALUES[6] = {100, 320, 330, 500, 900, 0};
const int QUIESCENCE_MAX_DEPTH = 12;

const std::string SQUARES[64] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};

using namespace std;

// ---- PeSTO tapered evaluation: mg/eg material values, phase weights ----
const int MG_VALUE[6] = {82, 337, 365, 477, 1025, 0};   // P N B R Q K
const int EG_VALUE[6] = {94, 281, 297, 512,  936, 0};
const int PHASE_WEIGHT[6] = {0, 1, 1, 2, 4, 0};
const int TOTAL_PHASE = 24; // 4N*1 + 4B*1 + 4R*2 + 2Q*4

// ---- PeSTO / Rofchade piece-square tables ----
// Index 0 = a8 (top-left as conventionally printed). White looks up table[sq ^ 56],
// black looks up table[sq] directly (see pesto_lookup below) — this avoids having
// to hand-reorder every table into our own a1=0 square numbering.
const int16_t MG_PAWN_TABLE[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};
const int16_t EG_PAWN_TABLE[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};
const int16_t MG_KNIGHT_TABLE[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};
const int16_t EG_KNIGHT_TABLE[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};
const int16_t MG_BISHOP_TABLE[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};
const int16_t EG_BISHOP_TABLE[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};
const int16_t MG_ROOK_TABLE[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};
const int16_t EG_ROOK_TABLE[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};
const int16_t MG_QUEEN_TABLE[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};
const int16_t EG_QUEEN_TABLE[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};
const int16_t MG_KING_TABLE[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};
const int16_t EG_KING_TABLE[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

//zorbrist hashing 
std::uint64_t ZORBRIST_PIECE[12][64]; // 12 piece types (6 white, 6 black) x 64 squares
std::uint64_t ZORBRIST_SIDE;
std::uint64_t ZORBRIST_EN_PASSANT[64];
std::uint64_t ZORBRIST_CASTLE[4]; // 4 possible castle states (1 bit for each side)

void innit_zorbrist(){
    init_passed_pawn_masks();
    std::mt19937_64 rng(0); // fixed seed for reproducibility
    for (int piece = 0; piece < 12; piece++) {
        for (int sq = 0; sq < 64; sq++) {
            ZORBRIST_PIECE[piece][sq] = rng();
        }
    }
    ZORBRIST_SIDE = rng();
    for (int sq = 0; sq < 64; sq++) {
        ZORBRIST_EN_PASSANT[sq] = rng();
    }
    for (int i  = 0; i  <  4; i++)  ZORBRIST_CASTLE[i] = rng();
}


class Gamestate{
    public:
        bool castle[4] = {true, true, true, true}; // white kingside, white queenside, black kingside, black queenside
        int en_passant_target = -1; // -1 means no en passant target 
        int white_to_move = 1; // 1 if white to move, 0 if black to move
        bool wcastled = false;
        bool bcastled = false;
};

class Bitboard{
    public:
        std::uint64_t white_pawns = 0x000000000000FF00;
        std::uint64_t white_knights = 0x0000000000000042;
        std::uint64_t white_bishops = 0x0000000000000024;
        std::uint64_t white_rooks = 0x0000000000000081;
        std::uint64_t white_queens = 0x0000000000000008;
        std::uint64_t white_kings = 0x0000000000000010;

        std::uint64_t black_pawns = 0x00FF000000000000;
        std::uint64_t black_knights = 0x4200000000000000;
        std::uint64_t black_bishops = 0x2400000000000000;
        std::uint64_t black_rooks = 0x8100000000000000;
        std::uint64_t black_queens = 0x0800000000000000;
        std::uint64_t black_kings = 0x1000000000000000;

        std::uint64_t white_all = white_pawns | white_knights | white_bishops | white_rooks | white_queens | white_kings;
        std::uint64_t black_all = black_pawns | black_knights | black_bishops | black_rooks | black_queens | black_kings;
        std::uint64_t all_pieces = white_all | black_all;

        char piece_at(std::uint64_t sq)const{
            std::uint64_t bit = 1ULL << sq;
            if (white_pawns & bit) return 'P';
            if (white_knights & bit) return 'N';
            if (white_bishops & bit) return 'B';
            if (white_rooks & bit) return 'R';
            if (white_queens & bit) return 'Q';
            if (white_kings & bit) return 'K';
            if (black_pawns & bit) return 'p';
            if (black_knights & bit) return 'n';
            if (black_bishops & bit) return 'b';
            if (black_rooks & bit) return 'r';
            if (black_queens & bit) return 'q';
            if (black_kings & bit) return 'k';

            return ' ';
        } 
        std::uint64_t hash(const Gamestate &state) const{
            std::uint64_t h = 0;
            std::uint64_t boards[12] = {
                white_pawns,white_knights,white_bishops,white_rooks,white_queens,white_kings,
                black_pawns,black_knights,black_bishops,black_rooks,black_queens,black_kings
            };
            for (int piece = 0; piece < 12; piece++) {
                std::uint64_t b = boards[piece];
                while (b) {
                    int sq = __builtin_ctzll(b); // get index of least significant bit
                    h ^= ZORBRIST_PIECE[piece][sq]; // XOR with corresponding random number
                    b &= b - 1; // clear least significant bit
                }
            }
            if (!state.white_to_move) h^= ZORBRIST_SIDE;
            if (state.en_passant_target != -1)h^= ZORBRIST_EN_PASSANT[state.en_passant_target];
            for (int i = 0; i<4; i++) state.castle[i] ? h^= ZORBRIST_CASTLE[i] : 0;
            return h;
        }
};

enum TTFlag {TT_EXACT, TT_LOWERBOUND, TT_UPPERBOUND};

struct TTEntry {
    std::uint64_t hash = 0;
    int score  = 0;
    int depth = -1;
    TTFlag flag = TT_EXACT;
    int best_from = -1;
    int best_to = -1;
};

const int TT_SIZE = 1 << 22; 
TTEntry transposition_table[TT_SIZE];

inline void clear_tt() {
    std::fill(std::begin(transposition_table), std::end(transposition_table), TTEntry());
}

inline TTEntry* tt_lookup(std::uint64_t h) {
    return &transposition_table[h & (TT_SIZE - 1)];
}

std::pair<char, std::int8_t> sq(std::uint8_t s[2]){
    int n = sizeof(FILES) / sizeof(FILES[0]);
    for (int idx = 0; idx < n; idx++){
        if (s[0] == FILES[idx]){
            return {FILES[idx], int(s[1]) - 1};
        }
    }
    return {' ', -1};
}

std::string sq_to_str(char pos[]) {
    if (pos == nullptr || pos[0] < 'a' || pos[0] > 'h' || pos[1] < '1' || pos[1] > '8') {
        return "";
    }

    const int file = pos[0] - 'a';
    const int rank = pos[1] - '1';
    return SQUARES[file + rank * 8];
}

int inbounds(int x, int y){
    return x >= 0 && x < 8 && y >= 0 && y < 8;
}
int piece_value(char piece){
    switch (piece) {
        case 'P': return PIECE_VALUES[0];
        case 'N': return PIECE_VALUES[1];
        case 'B': return PIECE_VALUES[2];
        case 'R': return PIECE_VALUES[3];
        case 'Q': return PIECE_VALUES[4];
        case 'K': return PIECE_VALUES[5];
        case 'p': return -PIECE_VALUES[0];
        case 'n': return -PIECE_VALUES[1];
        case 'b': return -PIECE_VALUES[2];
        case 'r': return -PIECE_VALUES[3];
        case 'q': return -PIECE_VALUES[4];
        case 'k': return -PIECE_VALUES[5];
        default: return 0;
    }
}
std::pair<int, int> sq_idx_to_xy(int idx){
    return {idx % 8, idx / 8};
}

int xy_to_sq_idx(int x, int y){
    return y * 8 + x;
}

void print_board(Bitboard &bb){
    cout << "\n   a b c d e f g h\n";
    for (int rank = 7; rank >= 0; rank--) {
        cout << rank + 1 << " ";
        for (int file = 0; file < 8; file++) {
            int idx = xy_to_sq_idx(file, rank);
            auto piece_info = bb.piece_at(idx);
            cout << "|";
            cout << piece_info;
        }
        cout << "| " << rank + 1 << "\n";
    }
    cout << "   a b c d e f g h\n";
}
int is_endgame(const Bitboard &bb) {
    std::uint64_t none_pawn_white = bb.white_all & ~bb.white_pawns;
    std::uint64_t none_pawn_black = bb.black_all & ~bb.black_pawns;
    int count = __builtin_popcountll(none_pawn_white) + __builtin_popcountll(none_pawn_black);
    return count <= 6; // Consider it endgame if there are 6 or fewer non-pawn pieces on the board
}

int is_white_attacked(int sq_idx, const Bitboard &bb) {
    int x = sq_idx % 8;
    int y = sq_idx / 8;
    std::uint64_t bit = 1ULL << sq_idx;

    // pawn attacks from black pawns
    if (((bb.black_pawns >> 7) & ~FILE_A) & bit) return true;
    if (((bb.black_pawns >> 9) & ~FILE_H) & bit) return true;

    // knight attacks (source pre-masked to avoid file wraparound)
    if (((bb.black_knights & ~FILE_A) >> 17) & bit) return true;
    if (((bb.black_knights & ~FILE_H) >> 15) & bit) return true;
    if (((bb.black_knights & ~(FILE_A | FILE_B)) >> 10) & bit) return true;
    if (((bb.black_knights & ~(FILE_G | FILE_H)) >> 6) & bit) return true;
    if (((bb.black_knights & ~FILE_H) << 17) & bit) return true;
    if (((bb.black_knights & ~FILE_A) << 15) & bit) return true;
    if (((bb.black_knights & ~(FILE_G | FILE_H)) << 10) & bit) return true;
    if (((bb.black_knights & ~(FILE_A | FILE_B)) << 6) & bit) return true;

    // black bishops / queens on diagonals
    const int diag_dx[4] = {1, 1, -1, -1};
    const int diag_dy[4] = {1, -1, 1, -1};
    for (int i = 0; i < 4; i++) {
        int dx = diag_dx[i];
        int dy = diag_dy[i];
        int nx = x + dx;
        int ny = y + dy;
        while (0 <= nx && nx < 8 && 0 <= ny && ny < 8) {
            int nsq = ny * 8 + nx;
            if ((bb.black_bishops & (1ULL << nsq)) || (bb.black_queens & (1ULL << nsq))) {
                return true;
            }
            if (bb.all_pieces & (1ULL << nsq)) {
                break;
            }
            nx += dx;
            ny += dy;
        }
    }

    // black rooks / queens on ranks/files
    for (int i = 0; i < 4; i++) {
        int dx = (i == 0 ? 1 : i == 1 ? -1 : 0);
        int dy = (i == 2 ? 1 : i == 3 ? -1 : 0);
        int nx = x + dx;
        int ny = y + dy;
        while (0 <= nx && nx < 8 && 0 <= ny && ny < 8) {
            int nsq = ny * 8 + nx;
            if ((bb.black_rooks & (1ULL << nsq)) || (bb.black_queens & (1ULL << nsq))) {
                return true;
            }
            if (bb.all_pieces & (1ULL << nsq)) {
                break;
            }
            nx += dx;
            ny += dy;
        }
    }
    // king attacks
    if ((bb.black_kings >> 8) & bit) return true;             // down (no wrap possible)
    if (((bb.black_kings >> 9) & ~FILE_H) & bit) return true; // down-left  (h-file wraps to a)
    if (((bb.black_kings >> 7) & ~FILE_A) & bit) return true; // down-right (a-file wraps to h)
    if (((bb.black_kings >> 1) & ~FILE_H) & bit) return true; // left       (a-file wraps to h)
    if ((bb.black_kings << 8) & bit) return true;             // up   (no wrap possible)
    if (((bb.black_kings << 9) & ~FILE_A) & bit) return true; // up-right   (h-file wraps to a)
    if (((bb.black_kings << 7) & ~FILE_H) & bit) return true; // up-left    (a-file wraps to h)
    if (((bb.black_kings << 1) & ~FILE_A) & bit) return true; // right      (h-file wraps to a)
    return false;
}

int is_black_attacked(int sq_idx, const Bitboard &bb) {
    int x = sq_idx % 8;
    int y = sq_idx / 8;
    std::uint64_t bit = 1ULL << sq_idx;

    //pawn attacks from white pawns
    if (((bb.white_pawns << 7) & ~FILE_H) & bit) return true;
    if (((bb.white_pawns << 9) & ~FILE_A) & bit) return true;

    // knight attacks
    if (((bb.white_knights & ~FILE_A) >> 17) & bit) return true;
    if (((bb.white_knights & ~FILE_H) >> 15) & bit) return true;
    if (((bb.white_knights & ~(FILE_A | FILE_B)) >> 10) & bit) return true;
    if (((bb.white_knights & ~(FILE_H | FILE_G)) >> 6) & bit) return true;
    if (((bb.white_knights & ~FILE_H) << 17) & bit) return true;
    if (((bb.white_knights << 15) & ~FILE_A) & bit) return true;
    if (((bb.white_knights << 10) & ~(FILE_H | FILE_G)) & bit) return true;
    if (((bb.white_knights << 6) & ~(FILE_A | FILE_B)) & bit) return true;


    // white bishops / queens on diagonals
    const int diag_dx[4] = {1, 1, -1, -1};
    const int diag_dy[4] = {1, -1, 1, -1};
    for (int i = 0; i < 4; i++) {
        int dx = diag_dx[i];
        int dy = diag_dy[i];
        int nx = x + dx;
        int ny = y + dy;
        while (0 <= nx && nx < 8 && 0 <= ny && ny < 8) {
            int nsq = ny * 8 + nx;
            if ((bb.white_bishops & (1ULL << nsq)) || (bb.white_queens & (1ULL << nsq))) {
                return true;
            }
            if (bb.all_pieces & (1ULL << nsq)) {
                break;
            }
            nx += dx;
            ny += dy;
        }
    }
    // white rooks / queens on ranks/files
    for (int i = 0; i < 4; i++) {
        int dx = (i == 0 ? 1 : i == 1 ? -1 : 0);
        int dy = (i == 2 ? 1 : i == 3 ? -1 : 0);
        int nx = x + dx;
        int ny = y + dy;
        while (0 <= nx && nx < 8 && 0 <= ny && ny < 8) {
            int nsq = ny * 8 + nx;
            if ((bb.white_rooks & (1ULL << nsq)) || (bb.white_queens & (1ULL << nsq))) {
                return true;
            }
            if (bb.all_pieces & (1ULL << nsq)) {
                break;
            }
            nx += dx;
            ny += dy;
        }
    }
    // king attacks
    if ((bb.white_kings >> 8) & bit) return true;             // down (no wrap possible)
    if (((bb.white_kings >> 9) & ~FILE_H) & bit) return true; // down-left  (h-file wraps to a)
    if (((bb.white_kings >> 7) & ~FILE_A) & bit) return true; // down-right (a-file wraps to h)
    if (((bb.white_kings >> 1) & ~FILE_H) & bit) return true; // left       (a-file wraps to h)
    if ((bb.white_kings << 8) & bit) return true;             // up   (no wrap possible)
    if (((bb.white_kings << 9) & ~FILE_A) & bit) return true; // up-right   (h-file wraps to a)
    if (((bb.white_kings << 7) & ~FILE_H) & bit) return true; // up-left    (a-file wraps to h)
    if (((bb.white_kings << 1) & ~FILE_A) & bit) return true; // right      (h-file wraps to a)
    return false;
}

int queen_development_pen(const Bitboard &bb) {
    int score = 0;
    int wundeveloped = 0;
    int wqueen_moved = 0;
    int bundeveloped = 0;
    int bqueen_moved = 0;
    
    wundeveloped += __builtin_popcountll(bb.white_knights & 0x42ULL);
    wundeveloped += __builtin_popcountll(bb.white_bishops & 0x24ULL);
    wqueen_moved = !(bb.white_queens & 0x8ULL);

    bundeveloped += __builtin_popcountll(bb.black_knights & 0x4200000000000000ULL);
    bundeveloped += __builtin_popcountll(bb.black_bishops & 0x2400000000000000ULL);
    bqueen_moved = !(bb.black_queens & 0x0800000000000000ULL);

    if (wqueen_moved && wundeveloped > 0) {
        score =  -20*wundeveloped;
    }
    if (bqueen_moved && bundeveloped > 0) {
        score += 20*bundeveloped;
    }
    return score;
}

int white_king_checked(const Bitboard &bb) {
    int king_sq = __builtin_ctzll(bb.white_kings);
    return is_white_attacked(king_sq, bb);
}
int black_king_checked(const Bitboard &bb) {
    int king_sq = __builtin_ctzll(bb.black_kings);
    return is_black_attacked(king_sq, bb);
}

std::vector<std::pair<int,int>> generate_white_captures(const Bitboard &bb, const Gamestate &state) {
    std::vector<std::pair<int,int>> moves;
    moves.reserve(35);

    // Pawns — diagonal captures only, plus en passant
    std::uint64_t pawns = bb.white_pawns;
    while (pawns) {
        int sq_idx = __builtin_ctzll(pawns); pawns &= pawns - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        if (x > 0 && y < 7 && (bb.black_all & (1ULL << (sq_idx + 7))))
            moves.push_back({sq_idx, sq_idx + 7});
        if (x < 7 && y < 7 && (bb.black_all & (1ULL << (sq_idx + 9))))
            moves.push_back({sq_idx, sq_idx + 9});
        if (state.en_passant_target != -1) {
            if (x > 0 && y == 4 && sq_idx + 7 == state.en_passant_target)
                moves.push_back({sq_idx, sq_idx + 7});
            if (x < 7 && y == 4 && sq_idx + 9 == state.en_passant_target)
                moves.push_back({sq_idx, sq_idx + 9});
        }
    }

    // Knights
    const int knight_offsets[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    std::uint64_t knights = bb.white_knights;
    while (knights) {
        int sq_idx = __builtin_ctzll(knights); knights &= knights - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (auto &off : knight_offsets) {
            int nx = x + off[0], ny = y + off[1];
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.black_all & (1ULL << tsq)) moves.push_back({sq_idx, tsq});
            }
        }
    }

    // Bishops + queens (diagonals)
    const int diag_dx[4] = {1,1,-1,-1}, diag_dy[4] = {1,-1,1,-1};
    std::uint64_t bishops = bb.white_bishops | bb.white_queens;
    while (bishops) {
        int sq_idx = __builtin_ctzll(bishops); bishops &= bishops - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + diag_dx[i], ny = y + diag_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.white_all & (1ULL << tsq)) break;
                if (bb.black_all & (1ULL << tsq)) { moves.push_back({sq_idx, tsq}); break; }
                nx += diag_dx[i]; ny += diag_dy[i];
            }
        }
    }

    // Rooks + queens (orthogonals)
    const int orth_dx[4] = {1,-1,0,0}, orth_dy[4] = {0,0,1,-1};
    std::uint64_t rooks = bb.white_rooks | bb.white_queens;
    while (rooks) {
        int sq_idx = __builtin_ctzll(rooks); rooks &= rooks - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + orth_dx[i], ny = y + orth_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.white_all & (1ULL << tsq)) break;
                if (bb.black_all & (1ULL << tsq)) { moves.push_back({sq_idx, tsq}); break; }
                nx += orth_dx[i]; ny += orth_dy[i];
            }
        }
    }

    // King captures
    std::uint64_t kings = bb.white_kings;
    while (kings) {
        int sq_idx = __builtin_ctzll(kings); kings &= kings - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            int nx = x+dx, ny = y+dy;
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny*8+nx;
                if (bb.black_all & (1ULL << tsq)) moves.push_back({sq_idx, tsq});
            }
        }
    }
    return moves;
}
std::vector<std::pair<int,int>> generate_black_captures(const Bitboard &bb, const Gamestate &state) {
    std::vector<std::pair<int,int>> moves;
    moves.reserve(35);

    // Pawns — diagonal captures only, plus en passant
    std::uint64_t pawns = bb.black_pawns;
    while (pawns) {
        int sq_idx = __builtin_ctzll(pawns); pawns &= pawns - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        if (x > 0 && y > 0 && (bb.white_all & (1ULL << (sq_idx - 9))))
            moves.push_back({sq_idx, sq_idx - 9});
        if (x < 7 && y > 0 && (bb.white_all & (1ULL << (sq_idx - 7))))
            moves.push_back({sq_idx, sq_idx - 7});
        if (state.en_passant_target != -1) {
            if (x > 0 && y == 3 && sq_idx - 9 == state.en_passant_target)
                moves.push_back({sq_idx, sq_idx - 9});
            if (x < 7 && y == 3 && sq_idx - 7 == state.en_passant_target)
                moves.push_back({sq_idx, sq_idx - 7});
        }
    }

    // Knights
    const int knight_offsets[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    std::uint64_t knights = bb.black_knights;
    while (knights) {
        int sq_idx = __builtin_ctzll(knights); knights &= knights - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (auto &off : knight_offsets) {
            int nx = x + off[0], ny = y + off[1];
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.white_all & (1ULL << tsq)) moves.push_back({sq_idx, tsq});
            }
        }
    }

    // Bishops + queens (diagonals)
    const int diag_dx[4] = {1,1,-1,-1}, diag_dy[4] = {1,-1,1,-1};
    std::uint64_t bishops = bb.black_bishops | bb.black_queens;
    while (bishops) {
        int sq_idx = __builtin_ctzll(bishops); bishops &= bishops - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + diag_dx[i], ny = y + diag_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.black_all & (1ULL << tsq)) break;
                if (bb.white_all & (1ULL << tsq)) { moves.push_back({sq_idx, tsq}); break; }
                nx += diag_dx[i]; ny += diag_dy[i];
            }
        }
    }

    // Rooks + queens (orthogonals)
    const int orth_dx[4] = {1,-1,0,0}, orth_dy[4] = {0,0,1,-1};
    std::uint64_t rooks = bb.black_rooks | bb.black_queens;
    while (rooks) {
        int sq_idx = __builtin_ctzll(rooks); rooks &= rooks - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + orth_dx[i], ny = y + orth_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.black_all & (1ULL << tsq)) break;
                if (bb.white_all & (1ULL << tsq)) { moves.push_back({sq_idx, tsq}); break; }
                nx += orth_dx[i]; ny += orth_dy[i];
            }
        }
    }

    // King captures
    std::uint64_t kings = bb.black_kings;
    while (kings) {
        int sq_idx = __builtin_ctzll(kings); kings &= kings - 1;
        int x = sq_idx % 8, y = sq_idx / 8;
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            int nx = x+dx, ny = y+dy;
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny*8+nx;
                if (bb.white_all & (1ULL << tsq)) moves.push_back({sq_idx, tsq});
            }
        }
    }
    return moves;
}

std::vector<std::pair<int,int>> generate_white_moves(const Bitboard &bb, const Gamestate &state) {
    std::vector<std::pair<int,int>> moves;
    moves.reserve(200);

    // Pawns
    uint64_t pawns = bb.white_pawns;
    while (pawns) {
        int sq = __builtin_ctzll(pawns); pawns &= pawns - 1;
        int x = sq % 8, y = sq / 8;
        if (y < 7) {
            int tsq = sq + 8;
            if (!(bb.all_pieces & (1ULL << tsq))) {
                moves.push_back({sq, tsq});
                if (y == 1) {
                    int tsq2 = sq + 16;
                    if (!(bb.all_pieces & (1ULL << tsq2)))
                        moves.push_back({sq, tsq2});
                }
            }
        }
        if (x > 0 && y < 7 && (bb.black_all & (1ULL << (sq + 7)))) moves.push_back({sq, sq + 7});
        if (x < 7 && y < 7 && (bb.black_all & (1ULL << (sq + 9)))) moves.push_back({sq, sq + 9});
        if (state.en_passant_target != -1) {
            if (x > 0 && y == 4 && sq + 7 == state.en_passant_target) moves.push_back({sq, sq + 7});
            if (x < 7 && y == 4 && sq + 9 == state.en_passant_target) moves.push_back({sq, sq + 9});
        }
    }

    // Knights
    const int knight_offsets[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    uint64_t knights = bb.white_knights;
    while (knights) {
        int sq = __builtin_ctzll(knights); knights &= knights - 1;
        int x = sq % 8, y = sq / 8;
        for (auto &off : knight_offsets) {
            int nx = x + off[0], ny = y + off[1];
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (!(bb.white_all & (1ULL << tsq))) moves.push_back({sq, tsq});
            }
        }
    }

    // Bishops + queens (diagonals)
    const int diag_dx[4] = {1,1,-1,-1}, diag_dy[4] = {1,-1,1,-1};
    uint64_t diag_sliders = bb.white_bishops | bb.white_queens;
    while (diag_sliders) {
        int sq = __builtin_ctzll(diag_sliders); diag_sliders &= diag_sliders - 1;
        int x = sq % 8, y = sq / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + diag_dx[i], ny = y + diag_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.white_all & (1ULL << tsq)) break;
                moves.push_back({sq, tsq});
                if (bb.black_all & (1ULL << tsq)) break;
                nx += diag_dx[i]; ny += diag_dy[i];
            }
        }
    }

    // Rooks + queens (orthogonals)
    const int orth_dx[4] = {1,-1,0,0}, orth_dy[4] = {0,0,1,-1};
    uint64_t orth_sliders = bb.white_rooks | bb.white_queens;
    while (orth_sliders) {
        int sq = __builtin_ctzll(orth_sliders); orth_sliders &= orth_sliders - 1;
        int x = sq % 8, y = sq / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + orth_dx[i], ny = y + orth_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.white_all & (1ULL << tsq)) break;
                moves.push_back({sq, tsq});
                if (bb.black_all & (1ULL << tsq)) break;
                nx += orth_dx[i]; ny += orth_dy[i];
            }
        }
    }

    // King
    uint64_t kings = bb.white_kings;
    while (kings) {
        int sq = __builtin_ctzll(kings); kings &= kings - 1;
        int x = sq % 8, y = sq / 8;
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (!(bb.white_all & (1ULL << tsq))) moves.push_back({sq, tsq});
            }
        }
        if (state.castle[0] && !(bb.all_pieces & 0x60) && !is_white_attacked(sq, bb) && !is_white_attacked(sq+1, bb) && !is_white_attacked(sq+2, bb))
            moves.push_back({sq, sq + 2});
        if (state.castle[1] && !(bb.all_pieces & 0xE)  && !is_white_attacked(sq, bb) && !is_white_attacked(sq-1, bb) && !is_white_attacked(sq-2, bb))
            moves.push_back({sq, sq - 2});
    }

    return moves;
}
std::vector<std::pair<int,int>> generate_black_moves(const Bitboard &bb, const Gamestate &state) {
    std::vector<std::pair<int,int>> moves;
    moves.reserve(200);

    // Pawns
    uint64_t pawns = bb.black_pawns;
    while (pawns) {
        int sq = __builtin_ctzll(pawns); pawns &= pawns - 1;
        int x = sq % 8, y = sq / 8;
        if (y > 0) {
            int tsq = sq - 8;
            if (!(bb.all_pieces & (1ULL << tsq))) {
                moves.push_back({sq, tsq});
                if (y == 6) {
                    int tsq2 = sq - 16;
                    if (!(bb.all_pieces & (1ULL << tsq2)))
                        moves.push_back({sq, tsq2});
                }
            }
        }
        if (x > 0 && y > 0 && (bb.white_all & (1ULL << (sq - 9)))) moves.push_back({sq, sq - 9});
        if (x < 7 && y > 0 && (bb.white_all & (1ULL << (sq - 7)))) moves.push_back({sq, sq - 7});
        if (state.en_passant_target != -1) {
            if (x > 0 && y == 3 && sq - 9 == state.en_passant_target) moves.push_back({sq, sq - 9});
            if (x < 7 && y == 3 && sq - 7 == state.en_passant_target) moves.push_back({sq, sq - 7});
        }
    }

    // Knights
    const int knight_offsets[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    uint64_t knights = bb.black_knights;
    while (knights) {
        int sq = __builtin_ctzll(knights); knights &= knights - 1;
        int x = sq % 8, y = sq / 8;
        for (auto &off : knight_offsets) {
            int nx = x + off[0], ny = y + off[1];
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (!(bb.black_all & (1ULL << tsq))) moves.push_back({sq, tsq});
            }
        }
    }

    // Bishops + queens (diagonals)
    const int diag_dx[4] = {1,1,-1,-1}, diag_dy[4] = {1,-1,1,-1};
    uint64_t diag_sliders = bb.black_bishops | bb.black_queens;
    while (diag_sliders) {
        int sq = __builtin_ctzll(diag_sliders); diag_sliders &= diag_sliders - 1;
        int x = sq % 8, y = sq / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + diag_dx[i], ny = y + diag_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.black_all & (1ULL << tsq)) break;
                moves.push_back({sq, tsq});
                if (bb.white_all & (1ULL << tsq)) break;
                nx += diag_dx[i]; ny += diag_dy[i];
            }
        }
    }

    // Rooks + queens (orthogonals)
    const int orth_dx[4] = {1,-1,0,0}, orth_dy[4] = {0,0,1,-1};
    uint64_t orth_sliders = bb.black_rooks | bb.black_queens;
    while (orth_sliders) {
        int sq = __builtin_ctzll(orth_sliders); orth_sliders &= orth_sliders - 1;
        int x = sq % 8, y = sq / 8;
        for (int i = 0; i < 4; i++) {
            int nx = x + orth_dx[i], ny = y + orth_dy[i];
            while (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (bb.black_all & (1ULL << tsq)) break;
                moves.push_back({sq, tsq});
                if (bb.white_all & (1ULL << tsq)) break;
                nx += orth_dx[i]; ny += orth_dy[i];
            }
        }
    }

    // King
    uint64_t kings = bb.black_kings;
    while (kings) {
        int sq = __builtin_ctzll(kings); kings &= kings - 1;
        int x = sq % 8, y = sq / 8;
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                int tsq = ny * 8 + nx;
                if (!(bb.black_all & (1ULL << tsq))) moves.push_back({sq, tsq});
            }
        }
        if (state.castle[2] && !(bb.all_pieces & 0x6000000000000000) && !is_black_attacked(sq, bb) && !is_black_attacked(sq+1, bb) && !is_black_attacked(sq+2, bb))
            moves.push_back({sq, sq + 2});
        if (state.castle[3] && !(bb.all_pieces & 0xE00000000000000)  && !is_black_attacked(sq, bb) && !is_black_attacked(sq-1, bb) && !is_black_attacked(sq-2, bb))
            moves.push_back({sq, sq - 2});
    }

    return moves;
}



std::pair<Bitboard,Gamestate> make_white_move(const Bitboard &bb, const Gamestate &state, int from_sq, int to_sq) {
    Bitboard new_bb = bb;
    Gamestate new_state = state;

    std::uint64_t from_bit = 1ULL << from_sq;
    std::uint64_t to_bit = 1ULL << to_sq;

    int from_x = from_sq % 8;
    int from_y = from_sq / 8;
    int to_x = to_sq % 8;
    int to_y = to_sq / 8;

    // Determine which piece is moving
    if (bb.white_pawns & from_bit) {
        new_bb.white_pawns &= ~from_bit; // Remove pawn from original square
        new_bb.white_pawns |= to_bit; // Place pawn on target square
        if (state.en_passant_target != -1 && (to_bit == (1ULL << state.en_passant_target))) {
            // Handle en passant capture
            if (to_x > from_x) {
                new_bb.black_pawns &= ~(1ULL << (to_sq - 8)); // Capture pawn to the left
            } else {
                new_bb.black_pawns &= ~(1ULL << (to_sq - 8)); // Capture pawn to the right
            }
        }
        //promotion
        if (to_y == 7) {
            new_bb.white_pawns &= ~to_bit; // Remove pawn from promotion square
            new_bb.white_queens |= to_bit; // Promote to queen (for simplicity, always promote to queen)
        }
    } else if (bb.white_knights & from_bit) {
        new_bb.white_knights &= ~from_bit;
        new_bb.white_knights |= to_bit;
    } else if (bb.white_bishops & from_bit) {
        new_bb.white_bishops &= ~from_bit;
        new_bb.white_bishops |= to_bit;
    } else if (bb.white_rooks & from_bit) {
        new_bb.white_rooks &= ~from_bit;
        new_bb.white_rooks |= to_bit;
            if (from_x == 0 && from_y == 0) {
                new_state.castle[1] = false; 
            }else if (from_x == 7 && from_y == 0) {
                new_state.castle[0] = false;
            }

    } else if (bb.white_queens & from_bit) {
        new_bb.white_queens &= ~from_bit;
        new_bb.white_queens |= to_bit;
    } else if (bb.white_kings & from_bit) {
        new_bb.white_kings &= ~from_bit;
        new_bb.white_kings |= to_bit;
        // Handle castling
        if (from_x == 4 && from_y == 0) {
            if (to_x == 6 && to_y == 0) {
                // Kingside castling
                new_bb.white_rooks &= ~(1ULL << 7); // Remove rook from h1
                new_bb.white_rooks |= (1ULL << 5); // Place rook on f1
                new_state.wcastled = true;
            } else if (to_x == 2 && to_y == 0) {
                // Queenside castling
                new_bb.white_rooks &= ~(1ULL << 0); // Remove rook from a1
                new_bb.white_rooks |= (1ULL << 3); // Place rook on d1
                new_state.wcastled = true;
            }
        }
        new_state.castle[0] = false;
        new_state.castle[1] = false; 
    }

    // Handle captures
    if (bb.black_all & to_bit) {
        if (bb.black_pawns & to_bit) {
            new_bb.black_pawns &= ~to_bit; // Remove captured piece
        } else if (bb.black_knights & to_bit) {
            new_bb.black_knights &= ~to_bit;
        } else if (bb.black_bishops & to_bit) {
            new_bb.black_bishops &= ~to_bit;
        } else if (bb.black_rooks & to_bit) {
            new_bb.black_rooks &= ~to_bit;
            if (to_x == 0 && to_y == 7) {
                new_state.castle[3] = false; // If black rook is captured, black loses kingside castling rights
            }else if (to_x == 7 && to_y == 7) {
                new_state.castle[2] = false; // If black rook is captured, black loses queenside castling rights
            }
            
        } else if (bb.black_queens & to_bit) {
            new_bb.black_queens &= ~to_bit;
        }
    }
    new_state.en_passant_target = -1; // Clear en passant target by default
    if (bb.white_pawns & from_bit) {
        if (to_y == from_y + 2) {
            new_state.en_passant_target = from_sq + 8; // Set en passant target square
        }
    }
    new_bb.white_all = new_bb.white_pawns | new_bb.white_knights | new_bb.white_bishops | new_bb.white_rooks | new_bb.white_queens | new_bb.white_kings;
    new_bb.black_all = new_bb.black_pawns | new_bb.black_knights | new_bb.black_bishops | new_bb.black_rooks | new_bb.black_queens | new_bb.black_kings;
    new_bb.all_pieces = new_bb.white_all | new_bb.black_all;
    return {new_bb, new_state}; 
}


std::pair<Bitboard,Gamestate> make_black_move(const Bitboard &bb, const Gamestate &state, int from_sq, int to_sq) {
    Bitboard new_bb = bb;
    Gamestate new_state = state;
    std::uint64_t from_bit = 1ULL << from_sq;
    std::uint64_t to_bit = 1ULL << to_sq;
    int from_x = from_sq % 8;
    int from_y = from_sq / 8;
    int to_x = to_sq % 8;
    int to_y = to_sq / 8;
    // Determine which piece is moving
    if (bb.black_pawns & from_bit) {
        new_bb.black_pawns &= ~from_bit; // Remove pawn from original square
        new_bb.black_pawns |= to_bit; // Place pawn on target square
        if (state.en_passant_target != -1 && (to_bit == (1ULL << state.en_passant_target))) {
            // Handle en passant capture
            if (to_x > from_x) {
                new_bb.white_pawns &= ~(1ULL << (to_sq + 8)); // Capture pawn to the left
            } else {
                new_bb.white_pawns &= ~(1ULL << (to_sq + 8)); // Capture pawn to the right
            }
        }
        //promotion
        if (to_y == 0) {
            new_bb.black_pawns &= ~to_bit; // Remove pawn from promotion square
            new_bb.black_queens |= to_bit; // Promote to queen (for simplicity, always promote to queen)
        }
    } else if (bb.black_knights & from_bit) {
        new_bb.black_knights &= ~from_bit;
        new_bb.black_knights |= to_bit;
    } else if (bb.black_bishops & from_bit) {
        new_bb.black_bishops &= ~from_bit;
        new_bb.black_bishops |= to_bit;
    } else if (bb.black_rooks & from_bit) {
        new_bb.black_rooks &= ~from_bit;
        new_bb.black_rooks |= to_bit;
            if (from_x == 0 && from_y == 7) {
                new_state.castle[3] = false; 
            }else if (from_x == 7 && from_y == 7) {
                new_state.castle[2] = false; 
            }
    } else if (bb.black_queens & from_bit) {
        new_bb.black_queens &= ~from_bit;
        new_bb.black_queens |= to_bit;
    } else if (bb.black_kings & from_bit) {
        new_bb.black_kings &= ~from_bit;
        new_bb.black_kings |= to_bit;
        // Handle castling
        if (from_x == 4 && from_y == 7) {
            if (to_x == 6 && to_y == 7) {
                // Kingside castling
                new_bb.black_rooks &= ~(1ULL << 63); // Remove rook from h8
                new_bb.black_rooks |= (1ULL << 61); // Place rook on f8
                new_state.bcastled = true;
            } else if (to_x == 2 && to_y == 7) {
                // Queenside castling
                new_bb.black_rooks &= ~(1ULL << 56); // Remove rook from a8
                new_bb.black_rooks |= (1ULL << 59); // Place rook on d8
                new_state.bcastled = true;
            }
        }  
        new_state.castle[2] = false; // Black loses kingside castling rights
        new_state.castle[3] = false; // Black loses queenside castling rights
    }
    // Handle captures
    if (bb.white_pawns & to_bit) {
        new_bb.white_pawns &= ~to_bit;
    } else if (bb.white_knights & to_bit) {
        new_bb.white_knights &= ~to_bit;
    } else if (bb.white_bishops & to_bit) {
        new_bb.white_bishops &= ~to_bit;
    } else if (bb.white_rooks & to_bit) {
        new_bb.white_rooks &= ~to_bit;
            if (to_x == 0 && to_y == 0) {
                    new_state.castle[1] = false; // If white rook is captured, white loses kingside castling rights
                }else if (to_x == 7 && to_y == 0) {
                    new_state.castle[0] = false; // If white rook is captured, white loses queenside castling rights
                }
    } else if (bb.white_queens & to_bit) {
        new_bb.white_queens &= ~to_bit;
    } else if (bb.white_kings & to_bit) {
        new_bb.white_kings &= ~to_bit;
    }

    new_state.en_passant_target = -1; // Clear en passant target by default
    if (bb.black_pawns & from_bit) {
        if (to_y == from_y - 2) {
            new_state.en_passant_target = from_sq - 8; // Set en passant target square
        }
    }
    new_bb.white_all = new_bb.white_pawns | new_bb.white_knights | new_bb.white_bishops | new_bb.white_rooks | new_bb.white_queens | new_bb.white_kings;
    new_bb.black_all = new_bb.black_pawns | new_bb.black_knights | new_bb.black_bishops | new_bb.black_rooks | new_bb.black_queens | new_bb.black_kings;
    new_bb.all_pieces = new_bb.white_all | new_bb.black_all;

    //add to history for 3 move repetition detection
    //new_state.history.push_back(new_bb);

    return {new_bb, new_state}; 
}

bool is_passed_pawn(int sq_idx, const Bitboard &bb, bool white) {
    const int rank = sq_idx / 8;
    const std::uint64_t file_mask = PASSED_PAWN_MASKS[sq_idx];

    if (white) {
        const std::uint64_t ahead_mask = file_mask & ~((1ULL << ((rank + 1) * 8)) - 1);
        return !(bb.black_pawns & ahead_mask);
    } else {
        const std::uint64_t ahead_mask = file_mask & ((1ULL << (rank * 8)) - 1);
        return !(bb.white_pawns & ahead_mask);
    }
}

int doubled_pawn_penalty(const Bitboard &bb) {
    int score = 0;
    const std::uint64_t FILE_A = 0x0101010101010101ULL;

    for (int x = 0; x < 8; ++x) {
        std::uint64_t file_mask = FILE_A << x; 
        if (__builtin_popcountll(bb.white_pawns & file_mask) > 1) {
            score += -10;
        }
        if (__builtin_popcountll(bb.black_pawns & file_mask) > 1) {
            score += 10; 
        }
    }
    return score;
}


// index 0-5: P N B R Q K
inline int piece_type_index(char p) {
    switch (toupper(p)) {
        case 'P': return 0; case 'N': return 1; case 'B': return 2;
        case 'R': return 3; case 'Q': return 4; case 'K': return 5;
    }
    return -1;
}

inline int pesto_lookup(const int16_t table[64], int sq, bool white) {
    return white ? table[sq ^ 56] : table[sq];
}

int game_phase(const Bitboard &bb) {
    int phase = 0;
    phase += __builtin_popcountll(bb.white_knights | bb.black_knights) * PHASE_WEIGHT[1];
    phase += __builtin_popcountll(bb.white_bishops | bb.black_bishops) * PHASE_WEIGHT[2];
    phase += __builtin_popcountll(bb.white_rooks   | bb.black_rooks)   * PHASE_WEIGHT[3];
    phase += __builtin_popcountll(bb.white_queens  | bb.black_queens)  * PHASE_WEIGHT[4];
    return std::min(phase, TOTAL_PHASE); // clamp in case of promotions
}

int castling_evaluation(const Bitboard &bb, const Gamestate &state, const int &endgame) {
    (void)bb;
    (void)endgame;

    int score = 0;
    if (state.wcastled == false) {
        if (state.castle[0]) score += 100; // White kingside
        if (state.castle[1]) score += 100; // White queenside
    }else{
        score += 200; // Bonus for castled king
    }
    if (state.bcastled == false) {
        if (state.castle[2]) score -= 100; // Black kingside
        if (state.castle[3]) score -= 100; // Black queenside
    }else{
        score -= 200; // Penalty for castled king
    }

    return score;
}

int evaluate(const Bitboard &bb, bool endgame, const Gamestate &state) {
    (void)endgame;
    int mg_score = 0, eg_score = 0;
    auto accumulate = [&](std::uint64_t bits, int piece_idx,
                          const int16_t mg_table[64], const int16_t eg_table[64], bool white) {
        while (bits) {
            int sq = __builtin_ctzll(bits); bits &= bits - 1;
            int sign = white ? 1 : -1;
            mg_score += sign * (MG_VALUE[piece_idx] + pesto_lookup(mg_table, sq, white));
            eg_score += sign * (EG_VALUE[piece_idx] + pesto_lookup(eg_table, sq, white));
            if (piece_idx == 0) { // pawn-specific extras, kept from the old eval
                if (white) { if (is_passed_pawn(sq, bb, true))  mg_score += 80 + 10 * (sq / 8), eg_score += 80 + 10 * (sq / 8); }
                else       { if (is_passed_pawn(sq, bb, false)) mg_score -= 80 + 10 * (7 - sq / 8), eg_score -= 80 + 10 * (7 - sq / 8); }
            }
        }
    };

    accumulate(bb.white_pawns,   0, MG_PAWN_TABLE,   EG_PAWN_TABLE,   true);
    accumulate(bb.white_knights, 1, MG_KNIGHT_TABLE, EG_KNIGHT_TABLE, true);
    accumulate(bb.white_bishops, 2, MG_BISHOP_TABLE, EG_BISHOP_TABLE, true);
    accumulate(bb.white_rooks,   3, MG_ROOK_TABLE,   EG_ROOK_TABLE,   true);
    accumulate(bb.white_queens,  4, MG_QUEEN_TABLE,  EG_QUEEN_TABLE,  true);
    accumulate(bb.white_kings,   5, MG_KING_TABLE,   EG_KING_TABLE,   true);

    accumulate(bb.black_pawns,   0, MG_PAWN_TABLE,   EG_PAWN_TABLE,   false);
    accumulate(bb.black_knights, 1, MG_KNIGHT_TABLE, EG_KNIGHT_TABLE, false);
    accumulate(bb.black_bishops, 2, MG_BISHOP_TABLE, EG_BISHOP_TABLE, false);
    accumulate(bb.black_rooks,   3, MG_ROOK_TABLE,   EG_ROOK_TABLE,   false);
    accumulate(bb.black_queens,  4, MG_QUEEN_TABLE,  EG_QUEEN_TABLE,  false);
    accumulate(bb.black_kings,   5, MG_KING_TABLE,   EG_KING_TABLE,   false);

    int phase = game_phase(bb);
    int score = (mg_score * phase + eg_score * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    score += doubled_pawn_penalty(bb);
    score += queen_development_pen(bb);
    score += castling_evaluation(bb, state, phase < 12); // rough endgame-ish signal for this term
    return score;
}

bool has_legal_moves(const Bitboard &bb, const Gamestate &state, bool white) {
    auto moves = white ? generate_white_moves(bb, state) : generate_black_moves(bb, state);
    for (const auto &m : moves) {
        auto result = white ? make_white_move(bb, state, m.first, m.second)
                            : make_black_move(bb, state, m.first, m.second);
        bool in_check = white ? white_king_checked(result.first)
                              : black_king_checked(result.first);
        if (!in_check) return true;
    }
    return false;
}

int is_checkmate(const Bitboard &bb, const Gamestate &state) {
    bool white = state.white_to_move;
    bool in_check = white ? white_king_checked(bb) : black_king_checked(bb);
    if (!in_check) return false;
    return !has_legal_moves(bb, state, white);
}

int is_stalemate(const Bitboard &bb, const Gamestate &state) {
    bool white = state.white_to_move;
    bool in_check = white ? white_king_checked(bb) : black_king_checked(bb);
    if (in_check) return false;
    return !has_legal_moves(bb, state, white);
}

int quiescence(const Bitboard &bb, const Gamestate &state, bool white, int alpha, int beta, int depth) {
    if (depth >= QUIESCENCE_MAX_DEPTH)
        return evaluate(bb, is_endgame(bb), state);

    bool in_check = white ? white_king_checked(bb) : black_king_checked(bb);

    if (!in_check) {
        int stand_pat = evaluate(bb, is_endgame(bb), state);
        if (white) {
            if (stand_pat >= beta) return stand_pat;
            if (stand_pat > alpha) alpha = stand_pat;
        } else {
            if (stand_pat <= alpha) return stand_pat;
            if (stand_pat < beta)  beta = stand_pat;
        }
    }

    auto moves = in_check
        ? (white ? generate_white_moves(bb, state)    : generate_black_moves(bb, state))
        : (white ? generate_white_captures(bb, state) : generate_black_captures(bb, state));

    int legal_moves_tried = 0;

    for (const auto &move : moves) {
        int from_sq = move.first, to_sq = move.second;
        if (!in_check) {
            if (white  && !(bb.black_all & (1ULL << to_sq))) continue;
            if (!white && !(bb.white_all & (1ULL << to_sq))) continue;
        }

        if (white) {
            auto result = make_white_move(bb, state, from_sq, to_sq);
            if (white_king_checked(result.first)) continue;
            legal_moves_tried++;
            int score = quiescence(result.first, result.second, false, alpha, beta, depth + 1);
            if (score >= beta) return score;
            if (score > alpha) alpha = score;
        } else {
            auto result = make_black_move(bb, state, from_sq, to_sq);
            if (black_king_checked(result.first)) continue;
            legal_moves_tried++;
            int score = quiescence(result.first, result.second, true, alpha, beta, depth + 1);
            if (score <= alpha) return score;
            if (score < beta)   beta = score;
        }
    }

    if (in_check && legal_moves_tried == 0)
        return white ? -MATE_SCORE + depth : MATE_SCORE - depth;

    return white ? alpha : beta;
}
int minmax(const Bitboard &bb, const Gamestate &state, bool white, int depth, int alpha, int beta) {
    nodes_searched++;
    // --- TT PROBE ---
    std::uint64_t h = bb.hash(state);
    TTEntry *entry = tt_lookup(h);
    if (entry->hash == h && entry->depth >= depth) {
        if      (entry->flag == TT_EXACT) return entry->score;
        else if (entry->flag == TT_LOWERBOUND) alpha = std::max(alpha, entry->score);
        else if (entry->flag == TT_UPPERBOUND) beta  = std::min(beta,  entry->score);
        if (alpha >= beta) return entry->score;
    }
    int tt_from  = -1;
    int tt_to    = -1;
    if (entry ->hash == h) {
        tt_from = entry->best_from;
        tt_to   = entry->best_to;
    }

    if (depth == 0) {
        int score = quiescence(bb, state, white, alpha, beta, 0);
        entry->hash = h;
        entry->score = score;
        entry->depth = 0;
        entry->flag = TT_EXACT;
        return score;

    }

    int original_alpha = alpha;
    int original_beta = beta;
    int v;
    int best_from = -1;
    int best_to = -1;
    auto store_and_return = [&](int result) {
        if(depth >= entry->depth){
        TTFlag flag;
        if (result <= original_alpha) flag = TT_UPPERBOUND;
        else if (result >= original_beta) flag = TT_LOWERBOUND;
        else flag = TT_EXACT;

        entry->hash = h;
        entry->score = result;
        entry->depth = depth;
        entry->flag = flag;
        entry->best_from = best_from;
        entry->best_to = best_to;
        return result;
        }
        return result;
    };
    if (white) {
        v = -1000000;
        int old_v = v;
        int moves_searched = 0;
        if (!is_endgame(bb)){
            const int R = 2;
            bool in_check = white_king_checked(bb);
            if (depth >= 3 && !in_check) {
                Gamestate null_state = state;
                null_state.white_to_move = !white;
                null_state.en_passant_target = -1;
                int null_score = minmax(bb, null_state, !white, depth - 1 - R, beta - 1, beta);
                if (null_score >= beta){
                    if (depth >= entry->depth) {
                        entry->hash = h;
                        entry->score = null_score;
                        entry->depth = depth;
                        entry->flag = TT_LOWERBOUND;
                    }
                    return beta;
                }
            }
        }
        std::vector<std::pair<int,int>> moves = generate_white_moves(bb, state);
        std::vector<std::tuple<int,int,int>> captures;
        std::vector<std::pair<int,int>> quiet;
        std::vector<std::pair<int,int>> all_moves;

        for (const auto &move : moves) {
            int from_sq = move.first;
            int to_sq   = move.second;
            if (bb.black_all & (1ULL << to_sq)) {
                char target_piece = bb.piece_at(to_sq);
                int move_score = 10*piece_value(target_piece) +abs(piece_value(bb.piece_at(from_sq)));
                            
                captures.emplace_back(from_sq, to_sq, move_score);
            } else {
                quiet.push_back(move);
            }
        }

        // sort and assemble once, after the categorisation loop
        std::sort(captures.begin(), captures.end(),
            [](const auto &a, const auto &b) {
                return std::get<2>(a) > std::get<2>(b);
            });
        for (const auto &cap : captures)
            all_moves.emplace_back(std::get<0>(cap), std::get<1>(cap));
        all_moves.insert(all_moves.end(), quiet.begin(), quiet.end());
        if (tt_from != -1) {
            auto it = std::find(all_moves.begin(), all_moves.end(),
                                std::make_pair(tt_from, tt_to));
            if (it != all_moves.end())
                std::rotate(all_moves.begin(), it, it + 1);
        }
        for (const auto &m : all_moves) {
                int from_sq = m.first;
                int to_sq   = m.second;
                auto result = make_white_move(bb, state, from_sq, to_sq);
                const Bitboard  &new_bb    = result.first;
                const Gamestate &new_state = result.second;
                if (white_king_checked(new_bb)) continue;
                moves_searched++;
                v     = std::max(v, minmax(new_bb, new_state, false, depth-1, alpha, beta));
                if (v>old_v) {
                    best_from = from_sq;
                    best_to = to_sq;
                    old_v = v;
                }
                alpha = std::max(alpha, v);
                if (beta <= alpha) break;
            }

        if (moves_searched > 0) return store_and_return(v);
        return white_king_checked(bb) ? -100000 : 0;
    }else{
        v = 1000000;
        int old_v = v;
        int moves_searched = 0;
        if (!is_endgame(bb)){
            const int R = 2;
            bool in_check = black_king_checked(bb);
            if (depth >= 3 && !in_check) {
                Gamestate null_state = state;
                null_state.white_to_move = !white;
                null_state.en_passant_target = -1;
                int null_score = minmax(bb, null_state, !white, depth - 1 - R, alpha, alpha + 1);
                if (null_score <= alpha) {
                    if (depth >= entry->depth) {
                        entry->hash = h;
                        entry->score = null_score;
                        entry->depth = depth;
                        entry->flag = TT_UPPERBOUND;
                    }
                    return alpha;
                }
            }
        }
        std::vector<std::pair<int,int>> moves = generate_black_moves(bb, state);
        std::vector<std::tuple<int,int,int>> captures;
        std::vector<std::pair<int,int>> quiet;
        std::vector<std::pair<int,int>> all_moves;

        for (const auto &move : moves) {
            int from_sq = move.first;
            int to_sq   = move.second;
            if (bb.white_all & (1ULL << to_sq)) {
                char target_piece = bb.piece_at(to_sq); 
                int move_score = 10*piece_value(target_piece) + abs(piece_value(bb.piece_at(from_sq)));
                captures.emplace_back(from_sq, to_sq, move_score);
            } else {
                quiet.push_back(move);
            }
        }

        std::sort(captures.begin(), captures.end(),
            [](const auto &a, const auto &b) {
                return std::get<2>(a) > std::get<2>(b);
            });
        for (const auto &cap : captures)
            all_moves.emplace_back(std::get<0>(cap), std::get<1>(cap));
        all_moves.insert(all_moves.end(), quiet.begin(), quiet.end());
        if (tt_from != -1) {
            auto it = std::find(all_moves.begin(), all_moves.end(),
                                std::make_pair(tt_from, tt_to));
            if (it != all_moves.end())
                std::rotate(all_moves.begin(), it, it + 1);
        }
        for (const auto &m : all_moves) {
                int from_sq = m.first;
                int to_sq   = m.second;
                auto result = make_black_move(bb, state, from_sq, to_sq);
                const Bitboard  &new_bb    = result.first;
                const Gamestate &new_state = result.second;
                if (black_king_checked(new_bb)) continue;
                moves_searched++;
                v    = std::min(v, minmax(new_bb, new_state, true, depth-1, alpha, beta));
                if (v<old_v){
                    best_from = from_sq;
                    best_to = to_sq;
                    old_v = v;
                }
                beta = std::min(beta, v);
                if (beta <= alpha) break;
            }

        if (moves_searched > 0) return store_and_return(v);
        return black_king_checked(bb) ? 100000 : 0;
    }   
}


std::tuple<int,int,int> best_move(const Bitboard &bb, const Gamestate &state, bool white, int max_depth) {
    const int INF = 1000000;
    const int INITIAL_DELTA = 50;

    auto generate_legal = [&](bool side) {
        std::vector<std::pair<int,int>> moves = side ? generate_white_moves(bb, state) 
                                                     : generate_black_moves(bb, state);
        moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const std::pair<int,int> &move) {
            auto result = side ? make_white_move(bb, state, move.first, move.second)
                               : make_black_move(bb, state, move.first, move.second);
            return side ? white_king_checked(result.first) : black_king_checked(result.first);
        }), moves.end());
        return moves;
    };

    std::vector<std::pair<int,int>> moves = generate_legal(white);
    if (moves.empty()) return {-1, -1, -1};

    int best_from  = moves[0].first;
    int best_to    = moves[0].second;
    int best_score = white ? -INF : INF;
    int delta      = INITIAL_DELTA;

    for (int d = 1; d <= max_depth; ++d) {
        // Place previous iteration's best move first for better cutoffs
        auto it = std::find(moves.begin(), moves.end(), std::make_pair(best_from, best_to));
        if (it != moves.end()) std::rotate(moves.begin(), it, it + 1);

        int alpha = -INF;
        int beta  = INF;
        if (d > 2) {
            alpha = best_score - delta;
            beta  = best_score + delta;
        }

        while (true) {
            const int window_alpha = alpha;
            const int window_beta  = beta;
            int iter_best_score = white ? -INF : INF;
            int iter_best_from  = best_from;
            int iter_best_to    = best_to;
            int search_alpha = window_alpha;
            int search_beta  = window_beta;

            for (const auto &m : moves) {
                int v;
                if (white) {
                    auto result = make_white_move(bb, state, m.first, m.second);
                    v = minmax(result.first, result.second, false, d - 1, search_alpha, search_beta);
                    if (v > iter_best_score) {
                        iter_best_score = v;
                        iter_best_from  = m.first;
                        iter_best_to    = m.second;
                    }
                    if (v > search_alpha) search_alpha = v;
                    if (search_beta <= search_alpha) break;
                } else {
                    auto result = make_black_move(bb, state, m.first, m.second);
                    v = minmax(result.first, result.second, true, d - 1, search_alpha, search_beta);
                    if (v < iter_best_score) {
                        iter_best_score = v;
                        iter_best_from  = m.first;
                        iter_best_to    = m.second;
                    }
                    if (v < search_beta) search_beta = v;
                    if (search_beta <= search_alpha) break;
                }
            }

            if (iter_best_score <= window_alpha) {
                // Fail-low: widen window downward and re-search
                alpha = window_alpha - delta;
                beta  = window_beta;
                delta *= 2;
            } else if (iter_best_score >= window_beta) {
                // Fail-high: widen window upward and re-search
                alpha = window_alpha;
                beta  = window_beta + delta;
                delta *= 2;
            } else {
                // Score within window — accept result
                best_score = iter_best_score;
                best_from  = iter_best_from;
                best_to    = iter_best_to;
                delta      = INITIAL_DELTA;
                break;
            }
        }
    }

    return {best_score, best_from, best_to};
}
std::pair<Bitboard, Gamestate> engine_move(const Bitboard &bb, const Gamestate &state, bool white, int depth) {
    int from_sq, to_sq, score;
    std::tie(score, from_sq, to_sq) = best_move(bb, state, white, depth);
    if (from_sq == -1 || to_sq == -1) {
        std::cout << "No legal moves available" << std::endl;
        return {bb, state};
    }
    std::cout << "Best move: " << char('a' + (from_sq % 8)) << (from_sq / 8 + 1) << " to " << char('a' + (to_sq % 8)) << (to_sq / 8 + 1) << " with score " << score << std::endl;
    if (white) {
        return make_white_move(bb, state, from_sq, to_sq);
    } else {
        return make_black_move(bb, state,from_sq, to_sq);
    }
}       

void import_fen( Bitboard &bb, Gamestate &state) {
    clear_tt();
    std::string input_fen;
    std::cout << "Enter FEN string: ";
    std::getline(std::cin, input_fen);
    // 1. Reset all bitboards to 0 before parsing new state
    bb.white_pawns = bb.white_knights = bb.white_bishops = 0;
    bb.white_rooks = bb.white_queens = bb.white_kings = 0;
    bb.black_pawns = bb.black_knights = bb.black_bishops = 0;
    bb.black_rooks = bb.black_queens = bb.black_kings = 0;

    // 2. Split FEN into individual fields using stringstream
    std::stringstream ss(input_fen);
    std::string pieces, active_color, castling, en_passant;
    ss >> pieces >> active_color >> castling >> en_passant;

    // 3. Parse Piece Placement Field
    int rank = 7; // Start at rank 8 (index 7)
    int file = 0; // Start at file A (index 0)

    for (char c : pieces) {
        if (c == '/') {
            rank--;      // Move down to the next rank
            file = 0;    // Reset back to file A
        } 
        else if (std::isdigit(c)) {
            file += (c - '0'); // Skip empty squares
        } 
        else {
            // Determine square index (assuming 0 = a1, 63 = h8 layout)
            int square = rank * 8 + file;
            std::uint64_t bit = 1ULL << square;

            switch (c) {
                case 'P': bb.white_pawns |= bit; break;
                case 'N': bb.white_knights |= bit; break;
                case 'B': bb.white_bishops |= bit; break;
                case 'R': bb.white_rooks |= bit; break;
                case 'Q': bb.white_queens |= bit; break;
                case 'K': bb.white_kings |= bit; break;
                case 'p': bb.black_pawns |= bit; break;
                case 'n': bb.black_knights |= bit; break;
                case 'b': bb.black_bishops |= bit; break;
                case 'r': bb.black_rooks |= bit; break;
                case 'q': bb.black_queens |= bit; break;
                case 'k': bb.black_kings |= bit; break;
            }
            file++;
        }
    }

    // 4. Update the aggregate helper bitboards
    bb.white_all = bb.white_pawns | bb.white_knights | bb.white_bishops | 
                   bb.white_rooks | bb.white_queens | bb.white_kings;
    bb.black_all = bb.black_pawns | bb.black_knights | bb.black_bishops | 
                   bb.black_rooks | bb.black_queens | bb.black_kings;
    bb.all_pieces = bb.white_all | bb.black_all;

    // 5. Parse Active Color Field
    state.white_to_move = (active_color == "w");

    // 6. Parse Castling Rights Field
    state.castle[0] = (castling.find('K') != std::string::npos);
    state.castle[1] = (castling.find('Q') != std::string::npos);
    state.castle[2] = (castling.find('k') != std::string::npos);
    state.castle[3] = (castling.find('q') != std::string::npos);

    // 7. Parse En Passant Target Field
    if (en_passant == "-" || en_passant.length() < 2) {
        state.en_passant_target = -1;
    } else {
        int ep_file = en_passant[0] - 'a';
        int ep_rank = en_passant[1] - '1';
        state.en_passant_target = ep_rank * 8 + ep_file;
    }
}

int main() {
    innit_zorbrist();
    Bitboard bb;
    Gamestate state;
    cout << "Enter search depth (e.g., 4): ";
    int depth;
    cin >> depth;
    cin.ignore(); // Clear newline from input buffer
    cout <<"import FEN (I) or new game (N)? ";
    char choice;
    cin >> choice;
    cin.ignore(); // Clear newline from input buffer
    if (choice == 'I' || choice == 'i') {
        import_fen(bb, state);
        cout << "what is the castling rights (e.g., KQkq for all, - for none)? ";
        string castling_input;
        cin >> castling_input;
        state.castle[0] = (castling_input.find('K') != std::string::npos);
        state.castle[1] = (castling_input.find('Q') != std::string::npos);
        state.castle[2] = (castling_input.find('k') != std::string::npos);
        state.castle[3] = (castling_input.find('q') != std::string::npos);
        cout <<"has white castled? (Y/N): ";
        char wcastled_input;
        cin >> wcastled_input;
        state.wcastled = (wcastled_input == 'Y' || wcastled_input == 'y');
        cout <<"has black castled? (Y/N): ";
        char bcastled_input;
        cin >> bcastled_input;
        state.bcastled = (bcastled_input == 'Y' || bcastled_input == 'y');
    } else {
        clear_tt();
        bb = Bitboard();
        state = Gamestate();
    }
    cout << "what colour are you playing (W)hite or (B)lack? ";
    char player_choice;
    cin >> player_choice;
    cin.ignore(); // Clear newline from input buffer

    int player_colour = (player_choice == 'W' || player_choice == 'w') ? 1 : 0;
    
    while (true) {
        nodes_searched = 0;
        if (state.white_to_move == player_colour) {
            // Player's turn
            std::string move_input;
            std::cout << "Enter your move (e.g., e2e4): ";
            std::getline(std::cin, move_input);
            if (move_input.length() != 4) {
                std::cout << "Invalid move format. Please use the format e2e4." << std::endl;
                continue;
            }
            int from_file = move_input[0] - 'a';
            int from_rank = move_input[1] - '1';
            int to_file   = move_input[2] - 'a';
            int to_rank   = move_input[3] - '1';
            int from_sq = from_rank * 8 + from_file;
            int to_sq   = to_rank * 8 + to_file;
            auto result = state.white_to_move ? make_white_move(bb, state, from_sq, to_sq)
                                              : make_black_move(bb, state, from_sq, to_sq);
            bb = result.first;
            state = result.second;
        } else {
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            auto result = engine_move(bb, state, state.white_to_move, depth);
            bb = result.first;
            state = result.second;
            cout << nodes_searched << " nodes searched" << endl;
                std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                std::cout << "Time taken: " << duration << " ms" << std::endl;
            float nps = nodes_searched / (duration / 1000.0);
            std::cout << "Nodes per second: " << nps << std::endl;
            print_board(bb);

        }
        state.white_to_move = !state.white_to_move;
        if (is_checkmate(bb, state)) {
            std::cout << "Checkmate! " << (state.white_to_move ? "Black" : "White") << " wins!" << std::endl;
            break;
        } else if (is_stalemate(bb, state)) {
            std::cout << "Stalemate! It's a draw!" << std::endl;
            break;
        }
    }
    return 0;
}