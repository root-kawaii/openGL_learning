#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct InventoryItem
{
    std::string id;
    std::string displayName;
    int count = 0;
};

struct WeaponSlot
{
    std::string id;
    std::string displayName;
    int ammo = -1;
    bool equipped = false;
};

class Player
{
private:
    int playerId = 0;
    std::string name;
    int score = 0;
    int turnsPlayed = 0;
    bool isActive = false;
    bool isMyTurn = false;

    float maxHealth = 100.0f;
    float health = 100.0f;
    float maxShields = 50.0f;
    float shields = 35.0f;

    std::vector<WeaponSlot> weapons;
    std::vector<InventoryItem> inventory;
    std::unordered_map<std::string, int> keyCounts;

    static std::string makeDisplayName(const std::string &id);

public:
    Player(int id = 0, const std::string &playerName = "Player");

    int getId() const { return playerId; }
    const std::string &getName() const { return name; }

    void addScore(int points) { score += points; }
    int getScore() const { return score; }
    void incrementTurnsPlayed() { turnsPlayed++; }
    int getTurnsPlayed() const { return turnsPlayed; }
    void setActive(bool active) { isActive = active; }
    bool getActive() const { return isActive; }
    void setMyTurn(bool myTurn) { isMyTurn = myTurn; }
    bool getMyTurn() const { return isMyTurn; }

    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    float getShields() const { return shields; }
    float getMaxShields() const { return maxShields; }
    void setHealth(float value);
    void setShields(float value);

    const std::vector<WeaponSlot> &getWeapons() const { return weapons; }
    const WeaponSlot *getEquippedWeapon() const;
    void addWeapon(const std::string &weaponId, int ammo = -1, bool equipIfFirst = true);
    bool hasWeapon(const std::string &weaponId) const;
    bool equipWeapon(const std::string &weaponId);
    bool equipWeaponSlot(size_t slotIndex);

    const std::vector<InventoryItem> &getInventory() const { return inventory; }
    void addItem(const std::string &itemId, int count = 1);
    int getItemCount(const std::string &itemId) const;

    const std::unordered_map<std::string, int> &getKeys() const { return keyCounts; }
    void addKey(const std::string &keyId, int count = 1);
    bool hasKey(const std::string &keyId) const;
    int getKeyCount(const std::string &keyId) const;
};
