

#include "animation_controller.h"
#include <cstddef>

void AnimationController::update(float deltaTime) {
  if (!currentAnimation || !skeleton)
    return;

  currentTime += deltaTime;

  auto pose = currentAnimation->getPoseAtTime(currentTime);

  for (std::size_t i = 0; i < pose.size(); i++) {

    auto &ref = currentAnimation->getBoneAnimations()[i];
    auto boneIndex = ref.targetBoneIndex;

    if (boneIndex >= 0 &&
        boneIndex < static_cast<int>(skeleton->bones.size())) {
      // Convert keyframe to matrix
      glm::mat4 translation = glm::translate(glm::mat4(1.0f), pose[i].position);
      glm::mat4 rotation = glm::mat4_cast(pose[i].rotation);
      glm::mat4 scale = glm::scale(glm::mat4(1.0f), pose[i].scale);

      skeleton->bones[boneIndex].localTransform =
          translation * rotation * scale;
    }
  }

  updateSkeletonHierarchy();
}

void AnimationController::updateSkeletonHierarchy() {
  if (!skeleton)
    return;

  for (size_t i = 0; i < skeleton->bones.size(); ++i) {
    if (skeleton->bones[i].parentIndex == -1) {
      // Root bone
      skeleton->bones[i].worldTransform =
          skeleton->rootTransform * skeleton->bones[i].localTransform;
    } else {
      // Child bone
      glm::mat4 parentWorld =
          skeleton->bones[skeleton->bones[i].parentIndex].worldTransform;
      skeleton->bones[i].worldTransform =
          parentWorld * skeleton->bones[i].localTransform;
    }
  }
}
