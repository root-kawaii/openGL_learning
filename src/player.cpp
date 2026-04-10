#include "player.h"

#include <algorithm>
#include <cctype>

namespace
{
std::string titleCaseWords(std::string value)
{
    bool capitalize = true;
    for (char &c : value)
    {
        if (c == '_' || c == ':' || c == '-')
        {
            c = ' ';
            capitalize = true;
            continue;
        }

        c = capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                       : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        capitalize = std::isspace(static_cast<unsigned char>(c)) != 0;
    }
    return value;
}
}

Player::Player(int id, const std::string &playerName)
    : playerId(id), name(playerName)
{
}

std::string Player::makeDisplayName(const std::string &id)
{
    return titleCaseWords(id);
}

void Player::setHealth(float value)
{
    health = std::clamp(value, 0.0f, maxHealth);
}

void Player::setShields(float value)
{
    shields = std::clamp(value, 0.0f, maxShields);
}

const WeaponSlot *Player::getEquippedWeapon() const
{
    for (const auto &weapon : weapons)
    {
        if (weapon.equipped)
            return &weapon;
    }
    return weapons.empty() ? nullptr : &weapons.front();
}

void Player::addWeapon(const std::string &weaponId, int ammo, bool equipIfFirst)
{
    for (auto &weapon : weapons)
    {
        if (weapon.id == weaponId)
        {
            if (ammo >= 0)
            {
                if (weapon.ammo < 0)
                    weapon.ammo = ammo;
                else
                    weapon.ammo += ammo;
            }
            return;
        }
    }

    WeaponSlot slot;
    slot.id = weaponId;
    slot.displayName = makeDisplayName(weaponId);
    slot.ammo = ammo;
    slot.equipped = weapons.empty() && equipIfFirst;
    weapons.push_back(slot);
}

bool Player::hasWeapon(const std::string &weaponId) const
{
    return std::any_of(
        weapons.begin(),
        weapons.end(),
        [&](const WeaponSlot &weapon)
        { return weapon.id == weaponId; });
}

bool Player::equipWeapon(const std::string &weaponId)
{
    bool found = false;
    for (auto &weapon : weapons)
    {
        const bool equipThis = weapon.id == weaponId;
        weapon.equipped = equipThis;
        found = found || equipThis;
    }
    return found;
}

bool Player::equipWeaponSlot(size_t slotIndex)
{
    if (slotIndex >= weapons.size())
        return false;

    for (size_t i = 0; i < weapons.size(); ++i)
        weapons[i].equipped = (i == slotIndex);

    return true;
}

void Player::addItem(const std::string &itemId, int count)
{
    if (count <= 0)
        return;

    for (auto &item : inventory)
    {
        if (item.id == itemId)
        {
            item.count += count;
            return;
        }
    }

    inventory.push_back({itemId, makeDisplayName(itemId), count});
}

int Player::getItemCount(const std::string &itemId) const
{
    for (const auto &item : inventory)
    {
        if (item.id == itemId)
            return item.count;
    }
    return 0;
}

void Player::addKey(const std::string &keyId, int count)
{
    if (count <= 0 || keyId.empty())
        return;

    keyCounts[keyId] += count;
}

bool Player::hasKey(const std::string &keyId) const
{
    auto it = keyCounts.find(keyId);
    return it != keyCounts.end() && it->second > 0;
}

int Player::getKeyCount(const std::string &keyId) const
{
    auto it = keyCounts.find(keyId);
    return it != keyCounts.end() ? it->second : 0;
}
