#pragma once

#include <string>

class Player
{
private:
    std::string name;
    int score;
    int turnsPlayed;
    bool isActive = false;
    bool isMyTurn = false;

public:
    Player(const std::string &playerName)
        : name(playerName), score(0), turnsPlayed(0) {}

    void addScore(int points) { score += points; }
    int getScore() const { return score; }
    void incrementTurnsPlayed() { turnsPlayed++; }
    int getTurnsPlayed() const { return turnsPlayed; }
    void setActive(bool active) { isActive = active; }
    bool getActive() const { return isActive; }
    void setMyTurn(bool myTurn) { isMyTurn = myTurn; }
    bool getMyTurn() const { return isMyTurn; }
};