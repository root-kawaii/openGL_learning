#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>

struct Keyframe
{
  float time;
  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;
};

struct Bone
{
  std::string name;
  int parentIndex;
  glm::mat4 localTransform;
  glm::mat4 worldTransform;
  glm::mat4 inverseBindPose;
};

struct Skeleton
{
  std::vector<Bone> bones;
  glm::mat4 rootTransform;
};

class Animation
{
public:
  struct BoneAnimation
  {
    std::string boneName;
    int targetBoneIndex; // Set during binding
    std::vector<Keyframe> keyframes;
  };

private:
  std::vector<BoneAnimation> boneAnimations;
  float duration;

public:
  // Get interpolated pose for all bones at specific time
  // std::vector<Keyframe> getPoseAtTime(float time) const
  // {
  //   std::vector<Keyframe> pose(boneAnimations.size());

  //   for (size_t i = 0; i < boneAnimations.size(); ++i)
  //   {
  //     pose[i] = interpolateBone(boneAnimations[i], time);
  //   }

  //   return pose;
  // };

  // Keyframe interpolateBone(const BoneAnimation &boneAnimation,
  //                          float time) const {
  //   const auto &keyframes = boneAnimation.keyframes;

  //   if (keyframes.empty()) {
  //     return Keyframe{0.0f, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)};
  //   }

  //   if (keyframes.size() == 1) {
  //     return keyframes[0];
  //   }

  //   for (size_t i = 0; i < boneAnimation.keyframes.size(); i++) {
  //     if (i == 0 && boneAnimation.keyframes[i].time >= i) {
  //       return boneAnimation.keyframes[0];
  //     }
  //     if (i == boneAnimation.keyframes.size() - 1 &&
  //         boneAnimation.keyframes[i].time <= i) {
  //       return boneAnimation.keyframes.back();
  //     }
  //     if (boneAnimation.keyframes[i].time < time <
  //         boneAnimation.keyframes[i + 1].time) {
  //       const Keyframe &keyframe0 = boneAnimation.keyframes[i];
  //       const Keyframe &keyframe1 = boneAnimation.keyframes[i + 1];

  //       // Calculate interpolation factor (0.0 to 1.0)
  //       float deltaTime = keyframe1.time - keyframe0.time;
  //       float factor = (time - keyframe0.time) / deltaTime;

  //       // Clamp factor to [0, 1]
  //       factor = glm::clamp(factor, 0.0f, 1.0f);

  //       // Interpolate
  //       Keyframe result;
  //       result.time = time;
  //       result.position =
  //           glm::mix(keyframe0.position, keyframe1.position, factor);
  //       result.rotation =
  //           glm::slerp(keyframe0.rotation, keyframe1.rotation, factor);
  //       result.scale = glm::mix(keyframe0.scale, keyframe1.scale, factor);
  //     }
  //   }

  //   return boneAnimation.keyframes[0];
  // }
};

class AnimationController
{

private:
  Animation *currentAnimation;
  Skeleton *skeleton;
  bool isLooping;
  float currentTime;

public:
  AnimationController() {};
  AnimationController(Skeleton *skel) : skeleton(skel) {};

  void setSkeleton(Skeleton *skel) { skeleton = skel; };
  void play();
  void update(float deltaTime);
};
