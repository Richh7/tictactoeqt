#include <QApplication>
#include <QByteArray>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTcpSocket>
#include <QVBoxLayout>
#include <QWidget>

#include <cstring>

struct Move {
    int row;
    int col;
};

struct Board {
    int cells[3][3];
};

class TicTacToeClient : public QWidget {
    Q_OBJECT

public:
    explicit TicTacToeClient(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowTitle("Tic Tac Toe Qt");

        hostEdit = new QLineEdit("127.0.0.1", this);
        portSpin = new QSpinBox(this);
        portSpin->setRange(1, 65535);
        portSpin->setValue(8080);

        connectButton = new QPushButton("Connetti", this);
        statusLabel = new QLabel("Non connesso", this);

        auto* connectionLayout = new QHBoxLayout;
        connectionLayout->addWidget(hostEdit);
        connectionLayout->addWidget(portSpin);
        connectionLayout->addWidget(connectButton);

        auto* boardLayout = new QGridLayout;
        boardLayout->setSpacing(6);

        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                auto* button = new QPushButton(this);
                button->setFixedSize(96, 96);
                button->setStyleSheet("font-size: 36px; font-weight: 700;");
                button->setEnabled(false);

                cells[row][col] = button;
                boardLayout->addWidget(button, row, col);

                connect(button, &QPushButton::clicked, this, [this, row, col]() {
                    sendMove(row, col);
                });
            }
        }

        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->addLayout(connectionLayout);
        rootLayout->addWidget(statusLabel);
        rootLayout->addLayout(boardLayout);
        setLayout(rootLayout);

        connect(connectButton, &QPushButton::clicked, this, &TicTacToeClient::connectToServer);
        connect(&socket, &QTcpSocket::connected, this, &TicTacToeClient::onConnected);
        connect(&socket, &QTcpSocket::readyRead, this, &TicTacToeClient::onReadyRead);
        connect(&socket, &QTcpSocket::disconnected, this, &TicTacToeClient::onDisconnected);
        connect(&socket, &QTcpSocket::errorOccurred, this, &TicTacToeClient::onSocketError);

        resize(340, 420);
    }

private slots:
    void connectToServer() {
        if(socket.state() != QAbstractSocket::UnconnectedState) {
            socket.abort();
        }

        incoming.clear();
        playerId = 0;
        myTurn = false;
        gameFinished = false;
        parserState = ParserState::PlayerId;
        clearBoard();
        updateBoardButtons();

        connectButton->setEnabled(false);
        hostEdit->setEnabled(false);
        portSpin->setEnabled(false);
        statusLabel->setText("Connessione in corso...");

        socket.connectToHost(hostEdit->text(), static_cast<quint16>(portSpin->value()));
    }

    void onConnected() {
        statusLabel->setText("Connected. Waiting for the second player...");
    }

    void onReadyRead() {
        incoming.append(socket.readAll());
        parseIncoming();
    }

    void onDisconnected() {
        connectButton->setEnabled(true);
        hostEdit->setEnabled(true);
        portSpin->setEnabled(true);
        setBoardEnabled(false);

        if(!gameFinished) {
            statusLabel->setText("Disconnected from the server.");
        }
    }

    void onSocketError(QAbstractSocket::SocketError) {
        if(socket.state() == QAbstractSocket::UnconnectedState) {
            connectButton->setEnabled(true);
            hostEdit->setEnabled(true);
            portSpin->setEnabled(true);
        }

        if(!gameFinished) {
            statusLabel->setText("Network error: " + socket.errorString());
        }
    }

private:
    enum class ParserState {
        PlayerId,
        GameOver,
        Board,
        TurnOrWinner
    };

    template <typename T>
    bool readValue(T& value) {
        if(incoming.size() < static_cast<int>(sizeof(T))) {
            return false;
        }

        std::memcpy(&value, incoming.constData(), sizeof(T));
        incoming.remove(0, static_cast<int>(sizeof(T)));
        return true;
    }

    void parseIncoming() {
        while(true) {
            switch(parserState) {
            case ParserState::PlayerId:
                if(!readValue(playerId)) {
                    return;
                }

                statusLabel->setText(QString("You are player %1 (%2).")
                                         .arg(playerId)
                                         .arg(symbolFor(playerId)));
                parserState = ParserState::GameOver;
                break;

            case ParserState::GameOver:
                if(!readValue(currentGameOver)) {
                    return;
                }

                parserState = ParserState::Board;
                break;

            case ParserState::Board:
                if(!readValue(board)) {
                    return;
                }

                renderBoard();
                parserState = ParserState::TurnOrWinner;
                break;

            case ParserState::TurnOrWinner:
                if(currentGameOver) {
                    int winner = 0;

                    if(!readValue(winner)) {
                        return;
                    }

                    showGameResult(winner);
                    parserState = ParserState::GameOver;
                    return;
                }

                int turn = 0;

                if(!readValue(turn)) {
                    return;
                }

                myTurn = turn != 0;
                updateTurnStatus();
                updateBoardButtons();
                parserState = ParserState::GameOver;
                break;
            }
        }
    }

    void sendMove(int row, int col) {
        if(!myTurn || board.cells[row][col] != 0 || socket.state() != QAbstractSocket::ConnectedState) {
            return;
        }

        Move move{row, col};
        socket.write(reinterpret_cast<const char*>(&move), sizeof(move));
        socket.flush();

        myTurn = false;
        statusLabel->setText("Mossa inviata. Attesa dell'avversario...");
        updateBoardButtons();
    }

    void renderBoard() {
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                cells[row][col]->setText(symbolFor(board.cells[row][col]));
            }
        }
    }

    void clearBoard() {
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                board.cells[row][col] = 0;
                cells[row][col]->setText("");
            }
        }
    }

    void updateTurnStatus() {
        if(myTurn) {
            statusLabel->setText("It's your turn.");
        } else {
            statusLabel->setText("Waiting for the opponent.");
        }
    }

    void showGameResult(int winner) {
        gameFinished = true;
        myTurn = false;
        setBoardEnabled(false);

        QString message;

        if(winner == 0) {
            message = "Pareggio!";
        } else if(winner == playerId) {
            message = "Hai vinto!";
        } else {
            message = "Hai perso.";
        }

        statusLabel->setText(message);
        QMessageBox::information(this, "Partita terminata", message);
    }

    void updateBoardButtons() {
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                cells[row][col]->setEnabled(myTurn && board.cells[row][col] == 0);
            }
        }
    }

    void setBoardEnabled(bool enabled) {
        for(int row = 0; row < 3; ++row) {
            for(int col = 0; col < 3; ++col) {
                cells[row][col]->setEnabled(enabled);
            }
        }
    }

    QString symbolFor(int value) const {
        if(value == 1) {
            return "X";
        }

        if(value == 2) {
            return "O";
        }

        return "";
    }

    QTcpSocket socket;
    QByteArray incoming;
    ParserState parserState = ParserState::PlayerId;

    Board board{{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
    int playerId = 0;
    int currentGameOver = 0;
    bool myTurn = false;
    bool gameFinished = false;

    QLineEdit* hostEdit = nullptr;
    QSpinBox* portSpin = nullptr;
    QPushButton* connectButton = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* cells[3][3]{};
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    TicTacToeClient window;
    window.show();

    return app.exec();
}

#include "client.moc"
