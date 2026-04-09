#include "game.h"
#include "history.h"
#include "../tracy/public/tracy/Tracy.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <../json/single_include/nlohmann/json.hpp>

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);

Game::Game()
    : deltaTime(0.0f), isRunning(true), window(nullptr)
{
    this->initWindow();
    initialize();
    mode = ENGINE;
}

Game::~Game()
{
    cleanup();
}

namespace
{
    std::string toLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });
        return value;
    }
}

bool Game::isFirstPersonSolid(const std::shared_ptr<GameObject> &obj) const
{
    if (!obj)
        return false;

    if (obj == firstPersonWeaponObject)
        return false;

    if (obj->name.rfind("Light_", 0) == 0)
        return false;

    if (obj->name.rfind("__runtime_", 0) == 0)
        return false;

    if (obj->name == "ball")
        return false;

    AABB aabb = obj->GetWorldAABB();
    if (!aabb.IsValid())
        return false;

    glm::vec3 size = aabb.GetSize();
    return size.x > 0.05f && size.y > 0.05f && size.z > 0.05f;
}

std::optional<glm::vec3> Game::findGameplaySpawnPoint() const
{
    if (!scene)
        return std::nullopt;

    // Main gameplay spawn for the current default dungeon level.
    return glm::vec3(0.0f, 2.0f, 0.0f);
}

void Game::enterFirstPersonGameMode()
{
    firstPersonEnabled = true;
    firstPersonGrounded = false;
    firstPersonJumpPressedLastFrame = false;
    firstPersonVerticalVelocity = 0.0f;
    firstPersonAimBlend = 0.0f;
    firstPersonShootCooldown = 0.0f;
    firstPersonShootAnimTime = 0.0f;
    clearFirstPersonProjectiles();

    if (auto spawn = findGameplaySpawnPoint())
    {
        camera.Position = *spawn;
    }

    camera.Yaw = -90.0f;
    camera.Pitch = 0.0f;
    camera.gameModeYawCenter = camera.Yaw;
    camera.ProcessMouseMovement(0.0f, 0.0f);
    firstMouse = true;

    GLFWwindow *activeWindow = inputWindow ? inputWindow : window;
    if (activeWindow)
        glfwSetInputMode(activeWindow, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);

    bool grounded = false;
    float groundHeight = -std::numeric_limits<float>::infinity();
    camera.Position = resolveFirstPersonCollisions(camera.Position, camera.Position,
                                                   grounded, groundHeight);
    firstPersonGrounded = grounded;
    previousAudioListenerPosition = camera.Position;
    audioListenerPrimed = true;
    audioManager.updateListener(camera.Position, camera.Front, glm::vec3(0.0f), camera.Up);
}

glm::vec3 Game::resolveFirstPersonCollisions(const glm::vec3 &targetCameraPos,
                                             const glm::vec3 &currentCameraPos,
                                             bool &grounded,
                                             float &groundHeight) const
{
    glm::vec3 resolved = targetCameraPos;
    grounded = false;
    groundHeight = -std::numeric_limits<float>::infinity();
    bool movingUp = targetCameraPos.y > currentCameraPos.y + 1e-4f;

    if (!scene)
        return resolved;

    float feet = resolved.y - firstPersonEyeHeight;
    float head = feet + firstPersonHeight;

    for (const auto &obj : scene->getGameObjects())
    {
        if (!isFirstPersonSolid(obj))
            continue;

        AABB aabb = obj->GetWorldAABB();

        bool overlapsBodyVertically = head > aabb.min.y && feet < aabb.max.y;
        bool shouldResolveSides = overlapsBodyVertically &&
                                  feet < (aabb.max.y - firstPersonGroundSnap);

        if (shouldResolveSides)
        {
            float closestX = std::max(aabb.min.x, std::min(resolved.x, aabb.max.x));
            float closestZ = std::max(aabb.min.z, std::min(resolved.z, aabb.max.z));
            glm::vec2 delta(resolved.x - closestX, resolved.z - closestZ);
            float distSq = glm::dot(delta, delta);
            float radiusSq = firstPersonRadius * firstPersonRadius;

            if (distSq < radiusSq)
            {
                if (distSq > 1e-6f)
                {
                    float dist = std::sqrt(distSq);
                    float push = firstPersonRadius - dist;
                    delta /= dist;
                    resolved.x += delta.x * push;
                    resolved.z += delta.y * push;
                }
                else
                {
                    float pushLeft = std::abs(resolved.x - aabb.min.x);
                    float pushRight = std::abs(aabb.max.x - resolved.x);
                    float pushBack = std::abs(resolved.z - aabb.min.z);
                    float pushFront = std::abs(aabb.max.z - resolved.z);

                    float minPush = pushLeft;
                    resolved.x = aabb.min.x - firstPersonRadius;

                    if (pushRight < minPush)
                    {
                        minPush = pushRight;
                        resolved.x = aabb.max.x + firstPersonRadius;
                    }
                    if (pushBack < minPush)
                    {
                        minPush = pushBack;
                        resolved.x = targetCameraPos.x;
                        resolved.z = aabb.min.z - firstPersonRadius;
                    }
                    if (pushFront < minPush)
                    {
                        resolved.x = targetCameraPos.x;
                        resolved.z = aabb.max.z + firstPersonRadius;
                    }
                }
            }
        }

        float expandedMinX = aabb.min.x - firstPersonRadius;
        float expandedMaxX = aabb.max.x + firstPersonRadius;
        float expandedMinZ = aabb.min.z - firstPersonRadius;
        float expandedMaxZ = aabb.max.z + firstPersonRadius;

        if (resolved.x >= expandedMinX && resolved.x <= expandedMaxX &&
            resolved.z >= expandedMinZ && resolved.z <= expandedMaxZ)
        {
            float top = aabb.max.y;
            if (!movingUp &&
                feet >= top - firstPersonGroundSnap &&
                feet <= top + firstPersonStepHeight &&
                top > groundHeight)
            {
                groundHeight = top;
                grounded = true;
            }

            if (currentCameraPos.y > resolved.y && feet < top && head > top)
            {
                resolved.y = top + firstPersonEyeHeight;
            }

            float ceiling = aabb.min.y;
            if (currentCameraPos.y < resolved.y &&
                head > ceiling && feet < ceiling &&
                ceiling > groundHeight)
            {
                resolved.y = ceiling - firstPersonHeight + firstPersonEyeHeight;
            }
        }
    }

    if (grounded && !movingUp)
        resolved.y = groundHeight + firstPersonEyeHeight;

    return resolved;
}

void Game::updateFirstPersonController()
{
    if (!firstPersonEnabled || mode != GAME)
        return;

    GLFWwindow *activeWindow = inputWindow ? inputWindow : window;
    if (!activeWindow)
        return;

    glm::vec3 flatFront(camera.Front.x, 0.0f, camera.Front.z);
    if (glm::length(flatFront) < 1e-4f)
        flatFront = glm::vec3(0.0f, 0.0f, -1.0f);
    else
        flatFront = glm::normalize(flatFront);

    glm::vec3 flatRight = glm::normalize(glm::cross(flatFront, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 moveDir(0.0f);

    if (glfwGetKey(activeWindow, GLFW_KEY_W) == GLFW_PRESS)
        moveDir += flatFront;
    if (glfwGetKey(activeWindow, GLFW_KEY_S) == GLFW_PRESS)
        moveDir -= flatFront;
    if (glfwGetKey(activeWindow, GLFW_KEY_D) == GLFW_PRESS)
        moveDir += flatRight;
    if (glfwGetKey(activeWindow, GLFW_KEY_A) == GLFW_PRESS)
        moveDir -= flatRight;

    if (glm::length(moveDir) > 1e-4f)
        moveDir = glm::normalize(moveDir);

    float speed = firstPersonMoveSpeed;
    if (glfwGetKey(activeWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(activeWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        speed *= 1.35f;
    }

    bool jumpPressed = glfwGetKey(activeWindow, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (firstPersonGrounded && jumpPressed && !firstPersonJumpPressedLastFrame)
    {
        firstPersonVerticalVelocity = firstPersonJumpVelocity;
        firstPersonGrounded = false;
    }
    firstPersonJumpPressedLastFrame = jumpPressed;

    firstPersonVerticalVelocity -= firstPersonGravity * deltaTime;

    glm::vec3 target = camera.Position;
    target += moveDir * speed * deltaTime;
    target.y += firstPersonVerticalVelocity * deltaTime;

    bool grounded = false;
    float groundHeight = -std::numeric_limits<float>::infinity();
    glm::vec3 resolved = resolveFirstPersonCollisions(target, camera.Position, grounded, groundHeight);

    float finalY = resolved.y;
    if (grounded && firstPersonVerticalVelocity <= 0.0f)
    {
        firstPersonVerticalVelocity = 0.0f;
        if (camera.Position.y > resolved.y + 0.001f)
        {
            float landingAlpha = std::clamp(deltaTime * 18.0f, 0.0f, 1.0f);
            finalY = glm::mix(camera.Position.y, resolved.y, landingAlpha);
            if (std::abs(finalY - resolved.y) < 0.01f)
                finalY = resolved.y;
        }
    }

    firstPersonGrounded = grounded;
    camera.Position = resolved;
    camera.Position.y = finalY;
}

void Game::createFirstPersonWeapon()
{
    if (firstPersonWeaponObject || !scene)
        return;

    firstPersonWeaponObject = std::make_shared<GameObject>(
        "__runtime_fps_steampunk_revolver",
        "assets/steampunk_revolver.glb",
        glm::vec3(0.0f, -1000.0f, 0.0f),
        glm::vec3(0.0f),
        glm::vec3(1.15f),
        0.0f,
        "pbr_model_textured",
        glm::vec3(1.0f));
    scene->addGameObject(firstPersonWeaponObject);
    hideFirstPersonWeapon();
}

void Game::hideFirstPersonWeapon()
{
    if (!firstPersonWeaponObject)
        return;

    firstPersonWeaponObject->ClearTransformOverride();
    firstPersonWeaponObject->position = glm::vec3(0.0f, -1000.0f, 0.0f);
}

void Game::clearFirstPersonProjectiles()
{
    if (scene)
    {
        for (auto &projectile : firstPersonProjectiles)
        {
            if (projectile.object)
                scene->removeGameObject(projectile.object->ID);
        }
    }
    firstPersonProjectiles.clear();
}

bool Game::shouldAttachTorchAudio(const std::shared_ptr<GameObject> &obj) const
{
    if (!obj)
        return false;

    const std::string lowerName = toLowerCopy(obj->name);
    const std::string lowerModelPath = toLowerCopy(obj->modelPath);

    if (lowerName.rfind("__runtime_", 0) == 0)
        return false;

    return lowerName.find("torch") != std::string::npos ||
           lowerModelPath.find("torch") != std::string::npos ||
           obj->name.rfind("Light_", 0) == 0;
}

void Game::clearTorchAudioEmitters()
{
    for (auto &emitter : torchAudioEmitters)
    {
        if (emitter.source)
            emitter.source->stop();
    }
    torchAudioEmitters.clear();
}

void Game::rebuildTorchAudioEmitters()
{
    clearTorchAudioEmitters();

    if (!scene || !torchAmbientClip)
        return;

    AudioAttenuationSettings attenuation;
    attenuation.referenceDistance = 300.0f;
    attenuation.maxDistance = 300.0f;
    attenuation.rolloffFactor = 0.0f;

    int torchIndex = 0;
    for (const auto &obj : scene->getGameObjects())
    {
        if (!shouldAttachTorchAudio(obj))
            continue;

        auto source = audioManager.createSpatialSource(torchAmbientClip, obj->position, attenuation);
        if (!source)
            continue;

        source->setLooping(true);
        source->setGain(0.0f);
        source->setPitch(0.96f + 0.02f * static_cast<float>(torchIndex % 3));
        torchAudioEmitters.push_back({obj, source});
        ++torchIndex;
    }
}

void Game::updateTorchAudioEmitters()
{
    if (torchAudioEmitters.empty())
        return;

    torchAudioEmitters.erase(
        std::remove_if(
            torchAudioEmitters.begin(),
            torchAudioEmitters.end(),
            [](const TorchAudioEmitter &emitter)
            {
                return emitter.object.expired() || !emitter.source;
            }),
        torchAudioEmitters.end());

    constexpr float kTorchAudibleDistance = 20.0f;
    constexpr float kTorchAudibleDistanceSq = kTorchAudibleDistance * kTorchAudibleDistance;
    constexpr float kTorchMaxGain = 0.64f;

    TorchAudioEmitter *nearestEmitter = nullptr;
    float nearestDistanceSq = std::numeric_limits<float>::max();

    for (auto &emitter : torchAudioEmitters)
    {
        auto obj = emitter.object.lock();
        if (!obj || !emitter.source)
            continue;

        const float distanceSq = glm::distance2(obj->position, camera.Position);
        if (distanceSq < nearestDistanceSq)
        {
            nearestDistanceSq = distanceSq;
            nearestEmitter = &emitter;
        }
    }

    for (auto &emitter : torchAudioEmitters)
    {
        auto obj = emitter.object.lock();
        if (!obj || !emitter.source)
            continue;

        emitter.source->setPosition(obj->position);
        emitter.source->setDirection(glm::vec3(0.0f, 1.0f, 0.0f));
        emitter.source->setGain(0.0f);
        emitter.source->pause();
    }

    if (!nearestEmitter || nearestDistanceSq > kTorchAudibleDistanceSq)
        return;

    auto nearestObject = nearestEmitter->object.lock();
    if (!nearestObject || !nearestEmitter->source)
        return;

    const float distance = std::sqrt(nearestDistanceSq);
    const float falloff = 1.0f - std::clamp(distance / kTorchAudibleDistance, 0.0f, 1.0f);
    nearestEmitter->source->setPosition(nearestObject->position);
    nearestEmitter->source->setGain(kTorchMaxGain * falloff * falloff);
    if (!nearestEmitter->source->isPlaying())
        nearestEmitter->source->play();
}

void Game::spawnFirstPersonProjectile(const glm::vec3 &origin, const glm::vec3 &velocity)
{
    if (!scene)
        return;

    if (!firstPersonProjectileModel)
        firstPersonProjectileModel = std::make_shared<Model>(std::filesystem::path("assets/ball.obj"));

    auto projectileObject = std::make_shared<GameObject>(
        "__runtime_fps_projectile_" + std::to_string(firstPersonProjectileCounter++),
        firstPersonProjectileModel,
        origin,
        glm::vec3(0.0f),
        glm::vec3(0.11f),
        0.0f,
        "pbr_model_textured",
        glm::vec3(1.0f, 0.82f, 0.45f));

    scene->addGameObject(projectileObject);
    firstPersonProjectiles.push_back({projectileObject, velocity, 3.0f, false, 3});
}

void Game::updateFirstPersonProjectiles()
{
    if (!scene || firstPersonProjectiles.empty())
        return;

    constexpr float kProjectileRadius = 0.11f;
    constexpr float kProjectileGravity = 8.5f;
    ProjectileBounceConfig bounceConfig;

    std::vector<size_t> expiredIndices;

    for (size_t i = 0; i < firstPersonProjectiles.size(); ++i)
    {
        auto &projectile = firstPersonProjectiles[i];
        if (!projectile.object)
        {
            expiredIndices.push_back(i);
            continue;
        }

        projectile.lifetime -= deltaTime;
        if (projectile.lifetime <= 0.0f)
        {
            expiredIndices.push_back(i);
            continue;
        }

        if (projectile.impacted)
            continue;

        projectile.velocity.y -= kProjectileGravity * deltaTime;

        glm::vec3 currentPos = projectile.object->position;
        glm::vec3 totalMove = projectile.velocity * deltaTime;
        int substeps = std::max(1, static_cast<int>(std::ceil(glm::length(totalMove) / 0.2f)));
        glm::vec3 stepMove = totalMove / static_cast<float>(substeps);

        for (int step = 0; step < substeps; ++step)
        {
            glm::vec3 candidate = currentPos + stepMove;
            auto hit = ProjectileBounceUtils::sweepSphereAgainstObjects(
                currentPos,
                candidate,
                kProjectileRadius,
                scene->getGameObjects(),
                [this](const std::shared_ptr<GameObject> &obj)
                { return isFirstPersonSolid(obj); });

            if (hit)
            {
                currentPos = ProjectileBounceUtils::resolveBouncePosition(*hit, bounceConfig);
                projectile.velocity = ProjectileBounceUtils::computeBounceVelocity(
                    projectile.velocity, hit->normal, bounceConfig);
                projectile.remainingBounces--;

                if (projectile.remainingBounces < 0 || glm::length(projectile.velocity) <= 0.001f)
                {
                    projectile.impacted = true;
                    projectile.lifetime = 0.18f;
                    projectile.velocity = glm::vec3(0.0f);
                    projectile.object->position = hit->point;
                    projectile.object->scale = glm::vec3(0.14f);
                    break;
                }

                continue;
            }
            currentPos = candidate;
        }

        projectile.object->position = currentPos;

        if (currentPos.y < -10.0f)
            expiredIndices.push_back(i);
    }

    for (auto it = expiredIndices.rbegin(); it != expiredIndices.rend(); ++it)
    {
        auto &projectile = firstPersonProjectiles[*it];
        if (projectile.object)
            scene->removeGameObject(projectile.object->ID);
        firstPersonProjectiles.erase(firstPersonProjectiles.begin() + *it);
    }
}

void Game::updateFirstPersonWeapon()
{
    if (!firstPersonWeaponObject)
        return;

    if (mode != GAME || !firstPersonEnabled)
    {
        hideFirstPersonWeapon();
        return;
    }

    GLFWwindow *activeWindow = inputWindow ? inputWindow : window;
    if (!activeWindow)
        return;

    const bool aimPressed =
        glfwGetMouseButton(activeWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool shootPressed =
        glfwGetMouseButton(activeWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    float aimStep = deltaTime * 8.0f;
    if (aimPressed)
        firstPersonAimBlend = std::min(1.0f, firstPersonAimBlend + aimStep);
    else
        firstPersonAimBlend = std::max(0.0f, firstPersonAimBlend - aimStep);

    if (firstPersonShootCooldown > 0.0f)
        firstPersonShootCooldown = std::max(0.0f, firstPersonShootCooldown - deltaTime);

    constexpr float kShootAnimDuration = 0.12f;
    if (shootPressed && firstPersonShootCooldown <= 0.0f)
    {
        firstPersonShootAnimTime = kShootAnimDuration;
        firstPersonShootCooldown = 0.14f;
        glm::vec3 shotOrigin = camera.Position + camera.Front * 0.22f;
        glm::vec3 shotVelocity = camera.Front * 52.0f;
        spawnFirstPersonProjectile(shotOrigin, shotVelocity);

        if (firstPersonShotClip)
        {
            AudioAttenuationSettings attenuation;
            attenuation.referenceDistance = 1.5f;
            attenuation.maxDistance = 28.0f;
            attenuation.rolloffFactor = 1.1f;

            AudioDirectionalCone cone;
            cone.innerAngleDegrees = 18.0f;
            cone.outerAngleDegrees = 70.0f;
            cone.outerGain = 0.35f;

            auto shotSource = audioManager.createDirectionalSource(
                firstPersonShotClip,
                shotOrigin,
                camera.Front,
                attenuation,
                cone);
            if (shotSource)
            {
                shotSource->setVelocity(shotVelocity * 0.1f);
                shotSource->setGain(0.65f);
                shotSource->setPitch(1.0f);
                shotSource->setAutoDestroy(true);
                shotSource->play();
            }
        }
    }
    else if (firstPersonShootAnimTime > 0.0f)
    {
        firstPersonShootAnimTime = std::max(0.0f, firstPersonShootAnimTime - deltaTime);
    }

    glm::vec3 hipOffset(0.12f, -0.24f, -0.42f);
    glm::vec3 aimOffset(0.03f, -0.16f, -0.34f);
    glm::vec3 localOffset = glm::mix(hipOffset, aimOffset, firstPersonAimBlend);

    float recoilT = 0.0f;
    if (firstPersonShootAnimTime > 0.0f)
    {
        recoilT = 1.0f - (firstPersonShootAnimTime / kShootAnimDuration);
        recoilT = std::sin(recoilT * 3.14159265f);
    }

    localOffset.z += recoilT * 0.08f;
    localOffset.x += recoilT * 0.01f;
    localOffset.y -= recoilT * 0.02f;

    glm::vec3 baseEulerDegrees(4.0f, -92.0f, -8.0f);
    glm::vec3 aimEulerDegrees(0.0f, -90.0f, -2.0f);
    glm::vec3 localEuler = glm::mix(baseEulerDegrees, aimEulerDegrees, firstPersonAimBlend);
    localEuler.x -= recoilT * 18.0f;
    localEuler.z += recoilT * 6.0f;

    glm::vec3 worldPos = camera.Position +
                         camera.Right * localOffset.x +
                         camera.Up * localOffset.y +
                         (-camera.Front) * localOffset.z;

    glm::mat4 cameraRotationMatrix(1.0f);
    cameraRotationMatrix[0] = glm::vec4(camera.Right, 0.0f);
    cameraRotationMatrix[1] = glm::vec4(camera.Up, 0.0f);
    cameraRotationMatrix[2] = glm::vec4(-camera.Front, 0.0f);

    glm::quat cameraRotation = glm::quat_cast(cameraRotationMatrix);
    glm::quat localRotation = glm::quat(glm::radians(localEuler));
    glm::quat correctionRotation =
        glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
        glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::quat worldRotation = cameraRotation * localRotation * correctionRotation;

    glm::mat4 weaponTransform =
        glm::translate(glm::mat4(1.0f), worldPos) *
        glm::mat4_cast(worldRotation) *
        glm::scale(glm::mat4(1.0f), glm::vec3(1.15f));

    // Keep position updated for culling/debug, but render from the exact matrix
    // so the viewmodel is not reinterpreted through the generic Euler path.
    firstPersonWeaponObject->position = worldPos;
    firstPersonWeaponObject->SetTransformOverride(weaponTransform);
}

bool Game::initialize()
{
    // Load settings first
    loadSettings();
    players.push_back(Player("Player 1"));
    SCR_HEIGHT = settings.resolutionHeight;
    SCR_WIDTH = settings.resolutionWidth;
    std::cout << "Initializing game with resolution: " << SCR_WIDTH << "x" << SCR_HEIGHT << std::endl;
    framebuffer_size_callback(window, settings.resolutionWidth, settings.resolutionHeight);
    uiManager = std::make_shared<UIManager>(settings.resolutionHeight, settings.resolutionWidth);
    uiManager->setWindow(window);
    uiManager->setInputManager(&inputManager);
    uiManager->setGame(this);
    auto mainScene = std::make_shared<Scene>(&renderManager);
    this->setScene(mainScene);
    scene->setGame(this);
    scene->setUIManager(uiManager.get());
    renderManager.setGame(this);
    renderManager.setUIManager(uiManager.get());
    createFirstPersonWeapon();
    audioManager.setMasterVolume(settings.masterVolume);
    firstPersonShotClip = audioManager.loadClip("assets/audio_1.wav");
    torchAmbientClip = audioManager.loadClipMono("assets/fire_sound.mp3");
    previousAudioListenerPosition = camera.Position;
    audioListenerPrimed = true;
    audioManager.updateListener(camera.Position, camera.Front, glm::vec3(0.0f), camera.Up);
    rebuildTorchAudioEmitters();

    vnManager.init(&renderManager, &camera);

    // Initialize turn system - player-based, not entity-based
    // All entities start with hasMovedThisTurn = false
    resetAllEntityMovement();

    return true;
}

void Game::run()
{
    // TODO: Main game loop
    // while (isRunning) {
    //     calculateDeltaTime();
    //     processInput();
    //     update(deltaTime);
    //     render();
    //     pollEvents();
    // }
}

void Game::update()
{
    ZoneScoped;
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    ImGui::Text("Frametime %f", deltaTime);
    ImGui::Text("FPS %f", 1 / deltaTime);
    lastFrame = currentFrame;

    inputManager.processInput(this, inputWindow ? inputWindow : window, &camera, deltaTime, MULTISAMPLE, seed, &renderManager);
    updateFirstPersonController();
    updateFirstPersonWeapon();
    updateFirstPersonProjectiles();
    updateTorchAudioEmitters();

    glm::vec3 listenerVelocity(0.0f);
    if (audioListenerPrimed && deltaTime > 1e-4f)
        listenerVelocity = (camera.Position - previousAudioListenerPosition) / deltaTime;

    audioManager.updateListener(camera.Position, camera.Front, listenerVelocity, camera.Up);
    audioManager.update(deltaTime);
    previousAudioListenerPosition = camera.Position;
    audioListenerPrimed = true;

    // --- Phase 1: Object Movement (pre-collision) ---
    auto gameObjects = scene->getGameObjects();
    for (auto &obj : gameObjects)
    {
        obj->speed += obj->acceleration * deltaTime;
        // This is the desired position *before* we check for collisions.
        obj->position += obj->speed * deltaTime;
    }

    // Update queued movement system
    updateTurnExecution();

    // Update ball flight trajectory
    for (auto &entity : scene->getGameEntities())
    {
        entity->updateBallFlight(deltaTime);
    }

    // Loose ball pickup — check every 5 frames
    static int pickupFrameCounter = 0;
    if (++pickupFrameCounter >= 5 && scene->ball)
    {
        pickupFrameCounter = 0;
        bool ballOwned = false;
        bool ballInFlight = false;
        for (auto &entity : scene->getGameEntities())
        {
            if (entity->getHasBall())
                ballOwned = true;
            if (entity->isBallInFlight())
                ballInFlight = true;
        }
        if (!ballOwned && !ballInFlight)
        {
            float closestDist = 1.5f; // pickup radius
            std::shared_ptr<GameEntity> closest = nullptr;
            for (auto &entity : scene->getGameEntities())
            {
                float dist = glm::distance(entity->object->position, scene->ball->position);
                if (dist < closestDist)
                {
                    closestDist = dist;
                    closest = entity;
                }
            }
            if (closest)
            {
                closest->setHasBall(true);
                scene->ball->position = closest->object->position;
                scene->ball->position.y = 0.5f;
                std::cout << "[Pickup] " << closest->object->name << " picked up loose ball!" << std::endl;
            }
        }
    }

    // --- Animation System Test: Switch between animations every 3 seconds ---
    static float animationTimer = 0.0f;
    static bool isPlayingBounce = false;
    animationTimer += deltaTime;

    if (animationTimer >= 1.0f)
    {
        animationTimer = 0.0f;
        isPlayingBounce = !isPlayingBounce;

        // Switch animation on all mech_drone models
        for (auto &obj : gameObjects)
        {
            if (obj->modelPath.find("mech_drone_multi") != std::string::npos)
            {
                if (isPlayingBounce)
                {
                    std::cout << "[Animation] Switching to Bounce animation with 0.5s blend" << std::endl;
                    obj->model->PlayAnimation("Bounce", 0.5f);
                }
                else
                {
                    std::cout << "[Animation] Switching to Take 001 animation with 0.5s blend" << std::endl;
                    obj->model->PlayAnimation("Take 001", 0.5f);
                }
            }
        }
    }

    // --- Phase 2: Optimized Collision Detection ---
    // Using new CollisionSystem for better performance
    // glm::vec3 cameraCorrection = collisionSystem.performCollisionPass(camera, gameObjects);

    // --- Phase 3: Apply All Final Corrections ---
    handleInput();
    // camera.Position += cameraCorrection;

    handleTurn();

    // --- FPS Limiting to 180 FPS ---
    const float targetFPS = 250.0f;
    const float targetFrameTime = 1.0f / targetFPS;

    float frameEndTime = static_cast<float>(glfwGetTime());
    float frameElapsed = frameEndTime - currentFrame;
    float sleepTime = targetFrameTime - frameElapsed;

    if (sleepTime > 0.0f)
    {
        // Convert to microseconds for more precise sleep
        // std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sleepTime * 1000000.0f)));
    }
}

void Game::loadSettings()
{
    std::ifstream settingsFile("settings/settings.json");
    if (!settingsFile.is_open())
    {
        std::cerr << "Failed to open settings file. Using default settings." << std::endl;
        return;
    }

    nlohmann::json settingsJson;
    try
    {
        settingsFile >> settingsJson;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::cerr << "Error parsing settings JSON: " << e.what() << std::endl;
        return;
    }

    std::cout << "Loading settings from settings.json..." << std::endl;

    // Load graphics settings
    if (settingsJson.contains("graphics"))
    {
        const auto &graphics = settingsJson["graphics"];

        if (graphics.contains("resolution"))
        {
            const auto &resolution = graphics["resolution"];
            settings.resolutionWidth = resolution.value("width", settings.resolutionWidth);
            settings.resolutionHeight = resolution.value("height", settings.resolutionHeight);
            settings.fullscreen = resolution.value("fullscreen", settings.fullscreen);

            std::cout << "  Resolution: " << settings.resolutionWidth << "x" << settings.resolutionHeight
                      << (settings.fullscreen ? " (Fullscreen)" : " (Windowed)") << std::endl;
        }
    }

    // Load audio settings
    if (settingsJson.contains("audio"))
    {
        const auto &audio = settingsJson["audio"];
        settings.masterVolume = audio.value("masterVolume", settings.masterVolume);

        std::cout << "  Master Volume: " << settings.masterVolume << std::endl;

        // TODO: Apply audio settings when AudioManager has setMasterVolume method
        // audioManager.setMasterVolume(settings.masterVolume);
    }

    // Load gameplay settings
    if (settingsJson.contains("gameplay"))
    {
        const auto &gameplay = settingsJson["gameplay"];
        settings.difficulty = gameplay.value("difficulty", settings.difficulty);
        settings.autosave = gameplay.value("autosave", settings.autosave);
        settings.autosaveInterval = gameplay.value("autosaveInterval", settings.autosaveInterval);

        std::cout << "  Difficulty: " << settings.difficulty << std::endl;
        std::cout << "  Autosave: " << (settings.autosave ? "Enabled" : "Disabled");
        if (settings.autosave)
            std::cout << " (every " << settings.autosaveInterval << "s)";
        std::cout << std::endl;
    }

    // Load controls settings
    if (settingsJson.contains("controls"))
    {
        const auto &controls = settingsJson["controls"];
        settings.invertY = controls.value("invertY", settings.invertY);

        std::cout << "  Invert Y-Axis: " << (settings.invertY ? "Yes" : "No") << std::endl;
    }

    std::cout << "Settings loaded successfully!" << std::endl;
}

void Game::render()
{
    // TODO: Render frame
    // renderManager.beginFrame();
    // renderManager.clear();
    // sceneManager.render();
    // renderManager.renderScene();
    // renderManager.endFrame();
    // renderManager.present();
}

void Game::cleanup()
{
    clearTorchAudioEmitters();
    clearFirstPersonProjectiles();
    hideFirstPersonWeapon();
    // TODO: Cleanup all resources
    // TODO: Destroy managers
    // TODO: Close window and terminate GLFW
}

// void Game::processInput()
// {
//     // TODO: Process input events
//     // inputManager.update();
//     // TODO: Handle game-specific input (pause, quit, etc.)
// }

// void Game::calculateDeltaTime()
// {
//     // TODO: Calculate time between frames
//     // static float lastFrame = 0.0f;
//     // float currentFrame = glfwGetTime();
//     // deltaTime = currentFrame - lastFrame;
//     // lastFrame = currentFrame;
// }

// void Game::pollEvents()
// {
//     // TODO: Poll window events
//     // glfwPollEvents();
//     // TODO: Check if window should close
// }

// void Game::setState(GameState newState)
// {
//     // TODO: Handle state transitions
//     // TODO: Cleanup current state
//     // TODO: Initialize new state
//     currentState = newState;
// }

// void Game::pause()
// {
//     // TODO: Pause game systems
//     // audioManager.pauseAll();
//     // TODO: Set paused state
// }

// void Game::resume()
// {
//     // TODO: Resume game systems
//     // audioManager.resumeAll();
//     // TODO: Reset delta time to avoid large jump
// }

// void Game::quit()
// {
//     // TODO: Initiate shutdown sequence
//     // TODO: Save game state if needed
//     isRunning = false;
// }

// EntityManager& Game::getEntityManager()
// {
//     // TODO: Return reference to entity manager
//     return entityManager;
// }

// RenderManager& Game::getRenderManager()
// {
//     // TODO: Return reference to render manager
//     return renderManager;
// }

// InputManager& Game::getInputManager()
// {
//     // TODO: Return reference to input manager
//     return inputManager;
// }

// AudioManager& Game::getAudioManager()
// {
//     // TODO: Return reference to audio manager
//     return audioManager;
// }

// SceneManager& Game::getSceneManager()
// {
//     // TODO: Return reference to scene manager
//     return sceneManager;
// }

// PhysicsManager& Game::getPhysicsManager()
// {
//     // TODO: Return reference to physics manager
//     return physicsManager;
// }

// ResourceManager& Game::getResourceManager()
// {
//     // TODO: Return reference to resource manager
//     return resourceManager;
// }

// GameState Game::getCurrentState() const
// {
//     return currentState;
// }

// float Game::getDeltaTime() const
// {
//     return deltaTime;
// }

// bool Game::isGameRunning() const
// {
//     return isRunning;
// }

// GLFWwindow* Game::getWindow()
// {
//     return window;
// }

// void Game::onWindowResize(int width, int height)
// {
//     // TODO: Handle window resize
//     // renderManager.setViewport(width, height);
//     // TODO: Update camera aspect ratio
// }

// void Game::onWindowClose()
// {
//     // TODO: Handle window close event
//     quit();
// }

// void Game::loadSettings()
// {
//     // TODO: Load game settings from file
//     // TODO: Apply settings to managers
// }

// void Game::saveSettings()
// {
//     // TODO: Save current settings to file
// }

// void Game::loadGameData()
// {
//     // TODO: Load saved game data
// }

// void Game::saveGameData()
// {
//     // TODO: Save current game progress
// }

void Game::processGameInput(GLFWwindow *window, Camera *camera, float deltaTime, bool &shadows, float &seed, RenderManager *renderManager)
{
    inputManager.processInput(this, window, camera, deltaTime, shadows, seed, renderManager);
}

void Game::setLevel(std::string levelName)
{
    clearTorchAudioEmitters();

    scene = std::make_unique<Scene>(levelName);
    scene->setGame(this);
    scene->setUIManager(uiManager.get());
    scene->setRenderManager(&renderManager);
    renderManager.setScene(scene.get());
    rebuildTorchAudioEmitters();
}

std::unordered_map<int, bool> previousKeyStates;

bool wasKeyJustPressed(int key, GLFWwindow *window)
{
    bool currentlyPressed = (glfwGetKey(window, key) == GLFW_PRESS);
    bool wasPressed = previousKeyStates[key];

    previousKeyStates[key] = currentlyPressed;

    return currentlyPressed && !wasPressed;
}

void Game::handleInput()
{
    // TAB — enter VN mode (test shortcut)
    if (wasKeyJustPressed(GLFW_KEY_TAB, window) && mode == GAME)
    {
        mode = VISUAL_NOVEL;
        vnManager.enter();
    }

    // VN mode controls — consume input and return early
    if (mode == VISUAL_NOVEL)
    {
        if (wasKeyJustPressed(GLFW_KEY_SPACE, window) || wasKeyJustPressed(GLFW_KEY_ENTER, window))
            vnManager.onAdvance();
        if (wasKeyJustPressed(GLFW_KEY_ESCAPE, window))
        {
            vnManager.exit();
            mode = GAME;
        }
        if (wasKeyJustPressed(GLFW_KEY_RIGHT, window))
            vnManager.onNextLocation();
        if (wasKeyJustPressed(GLFW_KEY_LEFT, window))
            vnManager.onPrevLocation();
        return;
    }

    bool shiftHeld = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    // Shift+C — copy selected object to clipboard
    if (shiftHeld && wasKeyJustPressed(GLFW_KEY_C, window))
    {
        scene->copyToClipboard();
    }

    // Shift+V — paste clipboard as new object (auto-selected)
    if (shiftHeld && wasKeyJustPressed(GLFW_KEY_V, window))
    {
        scene->pasteFromClipboard();
    }

    // Ctrl+Z — undo
    bool ctrlHeld = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    if (ctrlHeld && !shiftHeld && wasKeyJustPressed(GLFW_KEY_Z, window))
        scene->undo();

    // Ctrl+Y — redo
    if (ctrlHeld && wasKeyJustPressed(GLFW_KEY_Y, window))
        scene->redo();

    // Arrow keys — move selected object 1 unit at a time (X / Z plane)
    auto sel = scene->getSelectedGameObject();
    if (sel)
    {
        TransformState before = {sel->position, sel->rotation, sel->scale, sel->color};
        bool moved = false;

        if (wasKeyJustPressed(GLFW_KEY_LEFT, window))
        {
            sel->position.x -= 1.0f;
            moved = true;
        }
        if (wasKeyJustPressed(GLFW_KEY_RIGHT, window))
        {
            sel->position.x += 1.0f;
            moved = true;
        }
        if (wasKeyJustPressed(GLFW_KEY_UP, window))
        {
            sel->position.z -= 1.0f;
            moved = true;
        }
        if (wasKeyJustPressed(GLFW_KEY_DOWN, window))
        {
            sel->position.z += 1.0f;
            moved = true;
        }

        if (moved)
        {
            TransformState after = {sel->position, sel->rotation, sel->scale, sel->color};
            scene->pushTransformCommand(sel->ID, before, after);
        }
    }

    // Shoot ball with 'F' key
    if (wasKeyJustPressed(GLFW_KEY_F, window))
    {
        // Find entity with the ball and shoot
        for (auto &entity : scene->getGameEntities())
        {
            if (entity->getHasBall())
            {
                entity->shootBall(glm::vec3(-2.45f, 2.85f, 0.18f));
                break;
            }
        }
    }

    // Pass ball with 'G' key
    if (wasKeyJustPressed(GLFW_KEY_G, window))
    {
        // Find entity with the ball
        std::shared_ptr<GameEntity> ballHolder = nullptr;
        for (auto &entity : scene->getGameEntities())
        {
            if (entity->getHasBall())
            {
                ballHolder = entity;
                break;
            }
        }

        if (ballHolder)
        {
            // Find another capsule entity to pass to
            for (auto &entity : scene->getGameEntities())
            {
                // Pass to any other capsule entity (not the one holding the ball)
                if (entity != ballHolder &&
                    entity->object->name.find("capsule") != std::string::npos)
                {
                    ballHolder->passBall(entity.get());
                    break;
                }
            }
        }
    }
}

void Game::handleTurn()
{
    // if (getGameMode() != GAME)
    // {
    //     return;
    // }
    auto gameEntities = scene->getGameEntities();
    for (auto &entity : gameEntities)
    {
        // entity->onNewTurn();
    }
}

// Check if entity can be selected based on tag
bool Game::isEntitySelectable(std::shared_ptr<GameEntity> entity)
{
    return true;
    if (!entity || !entity->object)
        return false;
    std::string entityTag = entity->object->gameEntity;

    for (const auto &tag : selectableEntityTags)
    {
        if (entityTag.find(tag) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

// Check if entity can move this turn (hasn't moved yet)
bool Game::canEntityMove(std::shared_ptr<GameEntity> entity)
{
    if (!entity)
        return false;
    return !entity->hasMovedThisTurn;
}

// Set the currently selected entity
void Game::setSelectedEntity(std::shared_ptr<GameEntity> entity)
{
    selectedEntity = entity;
    if (entity && entity->object)
    {
        scene->setSelectedObject(entity->object);
        std::cout << "Selected entity: " << entity->object->name << std::endl;
    }
}

// Reset all entity movement flags (called at end of turn)
void Game::resetAllEntityMovement()
{
    auto entities = scene->getGameEntities();
    for (auto &entity : entities)
    {
        entity->hasMovedThisTurn = false;
    }
    std::cout << "All entity movement flags reset" << std::endl;
}

// Hash function for glm::ivec3
struct ivec3Hash
{
    size_t operator()(const glm::ivec3 &v) const
    {
        return std::hash<int>()(v.x) ^
               (std::hash<int>()(v.y) << 1) ^
               (std::hash<int>()(v.z) << 2);
    }
};

// Helper to discretize position to grid
glm::ivec3 discretizeToGrid(const glm::vec3 &pos)
{
    return glm::ivec3(std::floor(pos.x), std::floor(pos.y), std::floor(pos.z));
}

// Start turn execution - trigger all queued movements
void Game::startTurnExecution()
{
    if (turnState != TurnState::PLANNING)
    {
        std::cout << "[Turn] Cannot start execution - not in planning phase" << std::endl;
        return;
    }

    std::cout << "\n=== TURN EXECUTION START ===" << std::endl;

    // Process all entity action buffers
    for (auto &entity : scene->getGameEntities())
    {
        for (const auto &action : entity->getActionBuffer())
        {
            switch (action.type)
            {
            case ActionType::MOVE:
                entity->queueMovement(action.targetPosition);
                break;
            case ActionType::SHOOT:
                entity->shootBall(action.targetPosition);
                break;
            case ActionType::PASS:
                if (action.targetEntity)
                    entity->passBall(action.targetEntity);
                break;
            }
        }
        entity->clearActions();
    }

    turnState = TurnState::EXECUTING;
    allMovementsComplete = false;

    // Count queued movements
    int queuedCount = 0;
    for (auto &entity : scene->getGameEntities())
    {
        if (entity->hasQueuedMovements())
            queuedCount++;
    }
    std::cout << "[Turn] " << queuedCount << " entities with queued movements" << std::endl;
}

// Update turn execution - process movements and detect collisions
void Game::updateTurnExecution()
{
    if (turnState != TurnState::EXECUTING)
        return;

    // Track which entities moved this tick
    std::vector<std::pair<std::shared_ptr<GameEntity>, glm::vec3>> tickMovements;

    // Execute one step for each entity
    for (auto &entity : scene->getGameEntities())
    {

        glm::vec3 oldPos = entity->object->position;
        bool moved = entity->executeQueuedMovement(deltaTime);

        if (moved)
        {
            tickMovements.push_back({entity, oldPos});
        }
    }

    // Collision detection
    if (!tickMovements.empty())
    {
        detectCollisions(tickMovements);
    }

    // Check if all movements complete
    if (checkAllMovementsComplete())
    {
        std::cout << "=== TURN EXECUTION COMPLETE ===" << std::endl;
        turnState = TurnState::PLANNING;
        allMovementsComplete = true;
    }
}

// Check if all entities have completed their movements
bool Game::checkAllMovementsComplete()
{
    for (auto &entity : scene->getGameEntities())
    {
        if (entity->hasQueuedMovements() || entity->hasCurrentCommand)
        {
            return false;
        }
    }
    return true;
}

// Detect collisions between entities
void Game::detectCollisions(const std::vector<std::pair<std::shared_ptr<GameEntity>, glm::vec3>> &tickMovements)
{
    // Build arrivals map: destination -> [entities]
    std::unordered_map<glm::ivec3, std::vector<std::shared_ptr<GameEntity>>, ivec3Hash> arrivals;

    // Track movements for crossing detection
    struct Movement
    {
        std::shared_ptr<GameEntity> entity;
        glm::ivec3 from;
        glm::ivec3 to;
    };
    std::vector<Movement> movements;

    // Gather all movements this tick
    for (const auto &[entity, oldPos] : tickMovements)
    {
        glm::ivec3 fromGrid = discretizeToGrid(oldPos);
        glm::ivec3 toGrid = discretizeToGrid(entity->object->position);

        arrivals[toGrid].push_back(entity);
        movements.push_back({entity, fromGrid, toGrid});
    }

    // 1. Detect same-cell collisions (2+ entities at same position)
    for (const auto &[cell, entities] : arrivals)
    {
        if (entities.size() > 1)
        {
            std::cout << "[Collision] Same-cell: " << entities.size()
                      << " entities at (" << cell.x << ", " << cell.y << ", " << cell.z << ")" << std::endl;
            handleSameCellCollision(cell, entities);
        }
    }

    // 2. Detect crossing collisions (A→B while B→A)
    for (size_t i = 0; i < movements.size(); i++)
    {
        for (size_t j = i + 1; j < movements.size(); j++)
        {
            const auto &a = movements[i];
            const auto &b = movements[j];

            // Check if they swapped positions
            if (a.from == b.to && a.to == b.from)
            {
                std::cout << "[Collision] Crossing: entities swapped positions" << std::endl;
                handleCrossingCollision(a.entity, b.entity);
            }
        }
    }
}

// Handle same-cell collision - stack entities vertically
void Game::handleSameCellCollision(glm::ivec3 cell, const std::vector<std::shared_ptr<GameEntity>> &entities)
{
    // Stack entities vertically
    // For now: random order (future: use weight field)
    float stackHeight = 0.0f;

    for (auto &entity : entities)
    {
        // Set Y position based on stack order
        glm::vec3 pos = entity->object->position;
        pos.y = static_cast<float>(cell.y) + stackHeight;
        entity->object->position = pos;

        std::cout << "[Stacking] Entity at height " << stackHeight << std::endl;

        // TODO: Play jump animation
        // entity->object->model.PlayAnimation("Jump");

        stackHeight += 1.0f;
    }

    // Update scene occupancy map
    scene->updateOccupancyAfterCollision(cell, entities);
}

// Handle crossing collision - entities swap positions
void Game::handleCrossingCollision(std::shared_ptr<GameEntity> entityA, std::shared_ptr<GameEntity> entityB)
{
    // TODO: Play crossing animation
    std::cout << "[Crossing] Entities crossed paths" << std::endl;
    // Could play a "dodge" or "bump" animation
}

// End player turn and advance to next turn
void Game::endPlayerTurn()
{
    std::cout << "=== Ending Player Turn ===" << std::endl;

    // Reset turn state to planning
    turnState = TurnState::PLANNING;
    allMovementsComplete = true;

    // Clear all queued movements
    for (auto &entity : scene->getGameEntities())
    {
        entity->clearMovementQueue();
        entity->clearActions();
    }

    // Reset all entity movement flags
    resetAllEntityMovement();

    // Increment turn counter
    turn++;
    std::cout << "\n=== TURN " << turn << " ===" << std::endl;

    // Clear selection
    selectedEntity = nullptr;
    scene->setSelectedObject(nullptr);

    // Auto-enter VN mode between turns
    mode = VISUAL_NOVEL;
    vnManager.enter();
}
