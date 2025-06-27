#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket, threading, random, datetime, sys

HOST, PORT = "0.0.0.0", 12345
LOGFILE = "tictactoe.log"

LETTERS = "ABCDEFGHI"
POS = {c: (i // 3, i % 3) for i, c in enumerate(LETTERS)}

# Genera il timestamp in ISO‐8601
def log(msg):
    ts = datetime.datetime.now().isoformat(timespec="seconds")
    with open(LOGFILE, "a", encoding="utf-8") as f:
        f.write(f"[{ts}] {msg}\n")

class Player:
    def __init__(self, conn, addr):
        self.conn, self.addr, self.symbol = conn, addr, None
    
    def send(self, m): self.conn.sendall((m + "\n").encode())
    
    def recvline(self):
        data = b""
        while not data.endswith(b"\n"):
            chunk = self.conn.recv(1024)
            if not chunk: raise ConnectionError
            data += chunk
        return data.decode().strip()
    
    def close(self): self.conn.close()

# Ogni partita deve essere un thread separato. 

class Game(threading.Thread):
    def __init__(self, p1, p2):
        super().__init__(daemon=True)

        self.players = [p1, p2] # 	Lista con i due oggetti Player

        self.board = list(LETTERS)          # lista di 9 char

        p1.symbol, p2.symbol = ("X", "O") if random.choice([0, 1]) == 0 else ("O", "X")

        self.turn = 0 if p1.symbol == "X" else 1
        self.start_time = datetime.datetime.now() # datetime.datetime.now()	Memorizza l’istante di avvio per statistiche/log.
    
    def run(self):
        p1, p2 = self.players
        log(f"START {p1.addr}->{p1.symbol} vs {p2.addr}->{p2.symbol}")

        for p in self.players: p.send(f"START {p.symbol}")

        self.broadcast_board()

        playing = True
        result = "ABORT"

        while playing:
            cur, other = self.players[self.turn], self.players[1 - self.turn] # cur è il giocatore di turno 
            cur.send("YOUR_TURN")
            try:
                line = cur.recvline()
            except ConnectionError:
                other.send("ABORT") # Se cur si disconette la partita viene annullata 
                break

            parts = line.upper().split()

            if len(parts) != 2 or parts[0] != "MOVE" or parts[1] not in LETTERS: # Il messaggio deve essere MOVE + LETTERA
                cur.send("INVALID"); 
                continue

            letter = parts[1]
            idx = LETTERS.index(letter)
            if self.board[idx] in "XO": # Se la casella è gia occupata
                cur.send("INVALID"); 
                continue

            # esegue la mossa
            self.board[idx] = cur.symbol
            self.broadcast_board()

            if self.winning(cur.symbol):
                cur.send("WIN"); other.send("LOSE")
                playing, result = False, f"WIN {cur.symbol}"
            elif all(c in "XO" for c in self.board):
                self.broadcast("DRAW")
                playing, result = False, "DRAW"
            else:
                self.turn = 1 - self.turn

        dur = datetime.datetime.now() - self.start_time
        log(f"END {p1.addr} vs {p2.addr} moves={self.board} result={result} dur={dur}")
        for p in self.players: p.send("BYE"); p.close()

    # ---- helpers ----
    def broadcast(self, m): [p.send(m) for p in self.players]
    def broadcast_board(self): self.broadcast("BOARD " + "".join(self.board))
    def winning(self, s): # 	Controlla se il simbolo s (X o O) ha una riga, colonna o diagonale piena
        b = self.board
        lines = [(0,1,2), (3,4,5), (6,7,8),
                 (0,3,6), (1,4,7), (2,5,8),
                 (0,4,8), (2,4,6)]
        return any(b[a]==b[b_]==b[c]==s for a,b_,c in lines)

waiting, wlock = [], threading.Lock()

def accept_loop():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT)); s.listen()
        print(f"Server in ascolto su {HOST}:{PORT}")
        log(f"SERVER START {HOST}:{PORT}")

        while True:
            conn, addr = s.accept()
            p = Player(conn, addr); 
            p.send("WELCOME")
            with wlock: # Se c’è già qualcuno in attesa → esce quel qualcuno dalla lista, crea Game(p, mate).start() (nuovo thread).
                if waiting:
                    mate = waiting.pop(0)
                    Game(p, mate).start()
                else:
                    waiting.append(p); # Se non c’è nessuno → aggiunge p alla coda e gli dice WAIT.
                    p.send("WAIT")

if __name__ == "__main__":
    try: accept_loop()
    except KeyboardInterrupt: print("\nServer terminato."); sys.exit(0)
