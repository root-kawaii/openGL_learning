#include "object.h"

#include <iostream>

Object::Object(Model& model, glm::vec3 position, float ID):model(model), position(position), ID(ID)
{

}


Object::~Object() {

}
