#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>

#include <cstring>

struct Board {
    int cells[3][3];
};

struct Move {
    int row;
    int col;
};

class TicTacToeServer : public QObject {
    Q_OBJECT

public:
    explicit TicTacToeServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, &TicTacToeServer::onNewConnection);
    }

    bool listen(quint16 port) {
        if(!server.listen(QHostAddress::Any, port)) {
            qCritical() << "Unable to start the server:" << server.errorString();
            return false;
        }

        qInfo() << "Server listening on port" << port;
        qInfo() << "Waiting for two players...";
        return true;
    }

private slots:
    void onNewConnection() {
        while(server.hasPendingConnections()) {
            QTcpSocket* client = server.nextPendingConnection();

            if(clients.size() >= 2) {
                qInfo() << "Connection rejected: game is already full.";
                client->disconnectFromHost();
                client->deleteLater();
                continue;
            }

            clients.append(client);
            incoming.append(QByteArray{});

            const int playerNumber = clients.size();
            qInfo() << "Player" << playerNumber << "connected.";

            connect(client, &QTcpSocket::readyRead, this, [this, client]() {
                onReadyRead(client);
            });
            connect(client, &QTcpSocket::disconnected, this, [this, client]() {
                onClientDisconnected(client);
            });

            if(clients.size() == 2) {
                startGame();
            } else {
                qInfo() << "Waiting for the second player...";
            }
        }
    }

private:
    void startGame() {
        resetBoard();
        gameActive = true;
        gameFinished = false;
        currentPlayer = static_cast<int>(QRandomGenerator::global()->bounded(2));

        const int p1 = 1;
        const int p2 = 2;
        writeRaw(clients[0], p1);
        writeRaw(clients[1], p2);

        qInfo() << "Game started. Player" << currentPlayer + 1 << "goes first.";
        sendTurnState();
    }

    void resetBoard() {
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                board.cells[row][col] = 0;
            }
        }
    }

    void onReadyRead(QTcpSocket* client) {
        const int playerIndex = clients.indexOf(client);

        if(playerIndex < 0) {
            return;
        }

        incoming[playerIndex].append(client->readAll());

        while(incoming[playerIndex].size() >= static_cast<int>(sizeof(Move))) {
            Move move{};
            std::memcpy(&move, incoming[playerIndex].constData(), sizeof(Move));
            incoming[playerIndex].remove(0, static_cast<int>(sizeof(Move)));

            handleMove(playerIndex, move);
        }
    }

    void handleMove(int playerIndex, const Move& move) {
        if(!gameActive || gameFinished || playerIndex != currentPlayer) {
            return;
        }

        if(!isValidMove(move)) {
            qInfo() << "Invalid move from player" << playerIndex + 1
                    << "row" << move.row << "column" << move.col;
            sendTurnState();
            return;
        }

        board.cells[move.row][move.col] = playerIndex + 1;

        const int winner = winnerFound();

        if(winner != 0 || boardFull()) {
            finishGame(winner);
            return;
        }

        currentPlayer = 1 - currentPlayer;
        sendTurnState();
    }

    bool isValidMove(const Move& move) const {
        return move.row >= 0 && move.row < 3 &&
               move.col >= 0 && move.col < 3 &&
               board.cells[move.row][move.col] == 0;
    }

    bool boardFull() const {
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                if(board.cells[row][col] == 0) {
                    return false;
                }
            }
        }

        return true;
    }

    int winnerFound() const {
        for(int row = 0; row < 3; ++row) {
            if(board.cells[row][0] != 0 &&
               board.cells[row][0] == board.cells[row][1] &&
               board.cells[row][0] == board.cells[row][2]) {
                return board.cells[row][0];
            }
        }

        for(int col = 0; col < 3; ++col) {
            if(board.cells[0][col] != 0 &&
               board.cells[0][col] == board.cells[1][col] &&
               board.cells[0][col] == board.cells[2][col]) {
                return board.cells[0][col];
            }
        }

        if(board.cells[0][0] != 0 &&
           board.cells[0][0] == board.cells[1][1] &&
           board.cells[0][0] == board.cells[2][2]) {
            return board.cells[0][0];
        }

        if(board.cells[0][2] != 0 &&
           board.cells[0][2] == board.cells[1][1] &&
           board.cells[0][2] == board.cells[2][0]) {
            return board.cells[0][2];
        }

        return 0;
    }

    void sendTurnState() {
        const int gameOver = 0;

        for(int index = 0; index < clients.size(); ++index) {
            writeRaw(clients[index], gameOver);
            writeRaw(clients[index], board);

            const int myTurn = index == currentPlayer ? 1 : 0;
            writeRaw(clients[index], myTurn);
            clients[index]->flush();
        }
    }

    void finishGame(int winner) {
        gameFinished = true;
        gameActive = false;

        const int gameOver = 1;

        for(QTcpSocket* client : clients) {
            writeRaw(client, gameOver);
            writeRaw(client, board);
            writeRaw(client, winner);
            client->flush();
            client->disconnectFromHost();
        }

        qInfo() << "Game finished. Winner:" << winner;
        scheduleCleanup();
    }

    void onClientDisconnected(QTcpSocket* client) {
        const int playerIndex = clients.indexOf(client);

        if(playerIndex < 0) {
            client->deleteLater();
            return;
        }

        qInfo() << "Giocatore" << playerIndex + 1 << "disconnesso.";

        if(gameActive && !gameFinished && clients.size() == 2) {
            const int winner = 1 - playerIndex + 1;
            gameFinished = true;
            gameActive = false;

            QTcpSocket* opponent = clients[1 - playerIndex];

            if(opponent && opponent->state() == QAbstractSocket::ConnectedState) {
                const int gameOver = 1;
                writeRaw(opponent, gameOver);
                writeRaw(opponent, board);
                writeRaw(opponent, winner);
                opponent->flush();
                opponent->disconnectFromHost();
            }
        }

        scheduleCleanup(gameFinished ? 1000 : 0);
    }

    void scheduleCleanup(int delayMs = 1000) {
        if(cleanupScheduled) {
            return;
        }

        cleanupScheduled = true;
        QTimer::singleShot(delayMs, this, &TicTacToeServer::cleanupClients);
    }

    void cleanupClients() {
        for(QTcpSocket* client : clients) {
            if(client) {
                client->deleteLater();
            }
        }

        clients.clear();
        incoming.clear();
        gameActive = false;
        gameFinished = false;
        cleanupScheduled = false;
        currentPlayer = 0;

        qInfo() << "Waiting for two players...";
    }

    template <typename T>
    void writeRaw(QTcpSocket* socket, const T& value) {
        socket->write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    QTcpServer server;
    QVector<QTcpSocket*> clients;
    QVector<QByteArray> incoming;
    Board board{{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
    int currentPlayer = 0;
    bool gameActive = false;
    bool gameFinished = false;
    bool cleanupScheduled = false;
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    TicTacToeServer server;

    if(!server.listen(8080)) {
        return 1;
    }

    return app.exec();
}

#include "server.moc"
