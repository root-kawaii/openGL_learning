#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Forward declarations
class RenderManager;
class Camera;

// ─────────────────────────────────────────────────────────────────────────────
// VNManager — Visual Novel overlay system (Persona-style)
//
// Renders on top of the live 3D scene (scene keeps rendering underneath).
// Data-driven via vn/locations.json.
// Controls:
//   SPACE / ENTER  — advance dialogue (or complete typewriter)
//   TAB            — enter VN (test shortcut)
//   ESC            — exit VN, return to GAME mode
// ─────────────────────────────────────────────────────────────────────────────
class VNManager
{
public:
    // ── Data types ──────────────────────────────────────────────────────────
    struct VNCharacter
    {
        std::string id;
        std::string name;
        std::string spritePath;
        unsigned int spriteTexture = 0; // GL texture ID (0 = not loaded)
    };

    struct DialogueLine
    {
        std::string characterId;
        std::string text;

        // Optional per-line sprite (overrides character's default sprite)
        std::string  overrideSpritePath;
        unsigned int overrideSpriteTexture = 0;

        // Optional per-line camera tweak
        bool      hasCameraOverride = false;
        glm::vec3 cameraPos         = glm::vec3(0.0f);
        float     cameraYaw         = -90.0f;
        float     cameraPitch       = -10.0f;
    };

    struct Location
    {
        std::string id;
        std::string name;
        std::vector<VNCharacter> characters;
        std::vector<DialogueLine> lines;

        // 3-D world position for this scene
        glm::vec3 cameraPos   = glm::vec3(0.0f, 5.0f, 10.0f);
        float     cameraYaw   = -90.0f; // degrees
        float     cameraPitch = -10.0f; // degrees
    };

    // ── Lifecycle ────────────────────────────────────────────────────────────
    void init(RenderManager* rm, Camera* cam);
    void loadLocations(const std::string& jsonPath);

    // ── State ────────────────────────────────────────────────────────────────
    void enter();
    void exit();
    bool isActive() const { return active; }

    // ── Per-frame ────────────────────────────────────────────────────────────
    void update(float dt);
    void render(int screenW, int screenH);

    // ── Input forwarding (called from game input handler) ────────────────────
    void onAdvance();        // SPACE / ENTER
    void onNextLocation();   // → arrow
    void onPrevLocation();   // ← arrow

private:
    // ── Typewriter state ─────────────────────────────────────────────────────
    static constexpr float CHAR_INTERVAL  = 0.028f; // seconds per character
    static constexpr float CAM_LERP_TIME  = 0.7f;   // seconds to reach new position

    RenderManager*        rm           = nullptr;
    Camera*               camera       = nullptr;
    std::vector<Location> locations;

    int   locationIdx  = 0;
    int   lineIdx      = 0;
    bool  active       = false;

    float typeTimer    = 0.0f;
    int   visibleChars = 0;
    bool  lineComplete = false;

    // ── Camera lerp state ────────────────────────────────────────────────────
    glm::vec3 camStartPos   = glm::vec3(0.0f);
    glm::vec3 camTargetPos  = glm::vec3(0.0f);
    float     camStartYaw   = -90.0f;
    float     camTargetYaw  = -90.0f;
    float     camStartPitch = -10.0f;
    float     camTargetPitch= -10.0f;
    float     camLerpT      = 1.0f;  // 0→1; 1 = lerp done

    // ── Helpers ──────────────────────────────────────────────────────────────
    void loadSprite(VNCharacter& ch);
    void loadLineSprite(DialogueLine& dl);
    void changeLocation(int idx);
    void startLine();
    unsigned int      getActiveSpriteTexture() const;
    const VNCharacter* getCurrentCharacter() const;
    const Location*    getCurrentLocation()  const;

    // ── Rendering helpers ────────────────────────────────────────────────────
    void renderTopBar     (int sw, int sh);
    void renderCharSprite (int sw, int sh);
    void renderDialogueBox(int sw, int sh);
};
