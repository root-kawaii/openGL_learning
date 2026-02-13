#pragma once

enum class EntityClassType
{
    DEFAULT,
    STRIKER,
    DEFENDER,
    MIDFIELDER,
    GOALKEEPER
};

struct EntityClass
{
    EntityClassType type = EntityClassType::DEFAULT;
    int movementSpeed = 5;
    int strength = 5;
    int accuracy = 5;
    float weight = 75.0f;
    float height = 1.80f;

    static EntityClass fromType(EntityClassType classType)
    {
        EntityClass ec;
        ec.type = classType;

        switch (classType)
        {
        case EntityClassType::STRIKER:
            ec.movementSpeed = 4;
            ec.strength = 8;
            ec.accuracy = 7;
            ec.weight = 80.0f;
            ec.height = 1.82f;
            break;
        case EntityClassType::DEFENDER:
            ec.movementSpeed = 6;
            ec.strength = 4;
            ec.accuracy = 3;
            ec.weight = 85.0f;
            ec.height = 1.85f;
            break;
        case EntityClassType::MIDFIELDER:
            ec.movementSpeed = 7;
            ec.strength = 5;
            ec.accuracy = 6;
            ec.weight = 70.0f;
            ec.height = 1.75f;
            break;
        case EntityClassType::GOALKEEPER:
            ec.movementSpeed = 3;
            ec.strength = 6;
            ec.accuracy = 8;
            ec.weight = 90.0f;
            ec.height = 1.90f;
            break;
        case EntityClassType::DEFAULT:
        default:
            break;
        }

        return ec;
    }
};
