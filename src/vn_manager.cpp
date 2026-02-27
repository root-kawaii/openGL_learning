#include "vn_manager.h"
#include "render_manager.h"
#include "camera.h"
#include <../json/single_include/nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────
void VNManager::init(RenderManager* renderManager, Camera* cam)
{
    rm     = renderManager;
    camera = cam;
    loadLocations("vn/locations.json");
}

void VNManager::loadLocations(const std::string& jsonPath)
{
    std::ifstream f(jsonPath);
    if (!f.is_open())
    {
        std::cerr << "[VN] Cannot open " << jsonPath << std::endl;
        return;
    }

    json j;
    try { j = json::parse(f); }
    catch (const std::exception& e)
    {
        std::cerr << "[VN] JSON parse error: " << e.what() << std::endl;
        return;
    }

    locations.clear();
    for (const auto& locJ : j["locations"])
    {
        Location loc;
        loc.id   = locJ.value("id",   "");
        loc.name = locJ.value("name", "Unknown");

        if (locJ.contains("camera"))
        {
            const auto& cJ = locJ["camera"];
            loc.cameraPos.x  = cJ.value("x",     0.0f);
            loc.cameraPos.y  = cJ.value("y",     5.0f);
            loc.cameraPos.z  = cJ.value("z",    10.0f);
            loc.cameraYaw    = cJ.value("yaw",  -90.0f);
            loc.cameraPitch  = cJ.value("pitch", -10.0f);
        }

        for (const auto& cJ : locJ["characters"])
        {
            VNCharacter ch;
            ch.id         = cJ.value("id",     "");
            ch.name       = cJ.value("name",   "???");
            ch.spritePath = cJ.value("sprite", "");
            loc.characters.push_back(ch);
        }

        for (const auto& dJ : locJ["dialogue"])
        {
            DialogueLine dl;
            dl.characterId       = dJ.value("character", "");
            dl.text              = dJ.value("text",       "");
            dl.overrideSpritePath = dJ.value("sprite",    "");

            if (dJ.contains("camera"))
            {
                const auto& cJ   = dJ["camera"];
                dl.hasCameraOverride = true;
                dl.cameraPos.x   = cJ.value("x",     0.0f);
                dl.cameraPos.y   = cJ.value("y",     5.0f);
                dl.cameraPos.z   = cJ.value("z",    10.0f);
                dl.cameraYaw     = cJ.value("yaw",  -90.0f);
                dl.cameraPitch   = cJ.value("pitch", -10.0f);
            }

            loc.lines.push_back(dl);
        }

        locations.push_back(std::move(loc));
    }

    std::cout << "[VN] Loaded " << locations.size() << " locations." << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────
void VNManager::enter()
{
    if (locations.empty()) return;
    active = true;
    changeLocation(locationIdx);
    std::cout << "[VN] Entering visual novel mode." << std::endl;
}

void VNManager::exit()
{
    active = false;
    std::cout << "[VN] Exiting visual novel mode." << std::endl;
}

void VNManager::changeLocation(int idx)
{
    if (locations.empty()) return;
    locationIdx = (idx + (int)locations.size()) % (int)locations.size();
    lineIdx     = 0;

    // Lazy-load sprites for this location
    for (auto& ch : locations[locationIdx].characters)
        if (ch.spriteTexture == 0 && !ch.spritePath.empty())
            loadSprite(ch);

    // Start camera lerp toward this location's world position
    if (camera)
    {
        camStartPos    = camera->Position;
        camStartYaw    = camera->Yaw;
        camStartPitch  = camera->Pitch;
        camTargetPos   = locations[locationIdx].cameraPos;
        camTargetYaw   = locations[locationIdx].cameraYaw;
        camTargetPitch = locations[locationIdx].cameraPitch;
        camLerpT       = 0.0f;
    }

    startLine();
}

void VNManager::startLine()
{
    visibleChars = 0;
    typeTimer    = 0.0f;
    lineComplete = false;

    if (locations.empty()) return;
    auto& line = locations[locationIdx].lines[lineIdx];

    // Lazy-load per-line sprite override
    if (!line.overrideSpritePath.empty() && line.overrideSpriteTexture == 0)
        loadLineSprite(line);

    // Kick off camera lerp if this line has its own camera position
    if (camera && line.hasCameraOverride)
    {
        camStartPos    = camera->Position;
        camStartYaw    = camera->Yaw;
        camStartPitch  = camera->Pitch;
        camTargetPos   = line.cameraPos;
        camTargetYaw   = line.cameraYaw;
        camTargetPitch = line.cameraPitch;
        camLerpT       = 0.0f;
    }
}

void VNManager::loadSprite(VNCharacter& ch)
{
    if (!rm) return;
    ch.spriteTexture = rm->loadTexture(ch.id + "_sprite", ch.spritePath.c_str());
    std::cout << "[VN] Loaded char sprite '" << ch.spritePath
              << "' → GL id " << ch.spriteTexture << std::endl;
}

void VNManager::loadLineSprite(DialogueLine& dl)
{
    if (!rm || dl.overrideSpritePath.empty()) return;
    // Use path as cache key to share textures if the same file is reused
    dl.overrideSpriteTexture = rm->loadTexture(
        "vn_line_" + dl.overrideSpritePath, dl.overrideSpritePath.c_str());
    std::cout << "[VN] Loaded line sprite '" << dl.overrideSpritePath
              << "' → GL id " << dl.overrideSpriteTexture << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-frame
// ─────────────────────────────────────────────────────────────────────────────
// Smooth-step easing (cubic)
static float smoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

void VNManager::update(float dt)
{
    if (!active || locations.empty()) return;

    // ── Camera lerp ──────────────────────────────────────────────────────────
    if (camera && camLerpT < 1.0f)
    {
        camLerpT = std::min(camLerpT + dt / CAM_LERP_TIME, 1.0f);
        float t  = smoothStep(camLerpT);

        camera->Position = camStartPos + t * (camTargetPos - camStartPos);
        camera->Yaw      = camStartYaw   + t * (camTargetYaw   - camStartYaw);
        camera->Pitch    = camStartPitch + t * (camTargetPitch  - camStartPitch);
        // ProcessMouseMovement(0,0) is the public way to refresh Front/Right/Up
        camera->ProcessMouseMovement(0.0f, 0.0f);
    }

    // ── Typewriter ───────────────────────────────────────────────────────────
    const auto* loc = getCurrentLocation();
    if (!loc || loc->lines.empty()) return;

    if (!lineComplete)
    {
        typeTimer += dt;
        const std::string& fullText = loc->lines[lineIdx].text;
        int maxChars = (int)fullText.size();
        visibleChars = std::min((int)(typeTimer / CHAR_INTERVAL), maxChars);
        if (visibleChars >= maxChars)
            lineComplete = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────
void VNManager::onAdvance()
{
    if (!active || locations.empty()) return;

    const auto* loc = getCurrentLocation();
    if (!loc) return;

    if (!lineComplete)
    {
        // Reveal all chars immediately
        visibleChars = (int)loc->lines[lineIdx].text.size();
        lineComplete = true;
        return;
    }

    // Advance to next line; stop at the last one (no loop)
    if (lineIdx + 1 < (int)loc->lines.size())
    {
        lineIdx++;
        startLine();
    }
    // else: stay on last line with lineComplete = true so the SPACE hint stays
}

void VNManager::onNextLocation()
{
    changeLocation(locationIdx + 1);
}

void VNManager::onPrevLocation()
{
    changeLocation(locationIdx - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
unsigned int VNManager::getActiveSpriteTexture() const
{
    const auto* loc = getCurrentLocation();
    if (!loc || loc->lines.empty()) return 0;

    // Per-line override takes priority
    unsigned int lineSprite = loc->lines[lineIdx].overrideSpriteTexture;
    if (lineSprite != 0) return lineSprite;

    // Fall back to character's default sprite
    const auto* ch = getCurrentCharacter();
    return ch ? ch->spriteTexture : 0;
}

const VNManager::VNCharacter* VNManager::getCurrentCharacter() const
{
    const auto* loc = getCurrentLocation();
    if (!loc || loc->lines.empty()) return nullptr;

    const std::string& cid = loc->lines[lineIdx].characterId;
    for (const auto& ch : loc->characters)
        if (ch.id == cid) return &ch;
    return loc->characters.empty() ? nullptr : &loc->characters[0];
}

const VNManager::Location* VNManager::getCurrentLocation() const
{
    if (locations.empty()) return nullptr;
    return &locations[locationIdx];
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering
// ─────────────────────────────────────────────────────────────────────────────

// Colour palette (sunset / purple theme)
static const ImVec4 COL_BAR_BG      = { 0.05f, 0.02f, 0.14f, 0.92f };
static const ImVec4 COL_TAB_ACTIVE  = { 0.80f, 0.25f, 0.05f, 1.00f };
static const ImVec4 COL_TAB_IDLE    = { 0.18f, 0.07f, 0.30f, 0.90f };
static const ImVec4 COL_DIALOG_BG   = { 0.04f, 0.02f, 0.12f, 0.90f };
static const ImVec4 COL_NAME_BG     = { 0.78f, 0.22f, 0.04f, 1.00f };
static const ImVec4 COL_TEXT        = { 0.95f, 0.93f, 0.88f, 1.00f };
static const ImVec4 COL_HINT        = { 0.55f, 0.50f, 0.70f, 1.00f };
static const ImVec4 COL_TRANSPARENT = { 0.00f, 0.00f, 0.00f, 0.00f };

static ImVec2 operator+(ImVec2 a, ImVec2 b) { return { a.x + b.x, a.y + b.y }; }

void VNManager::render(int sw, int sh)
{
    if (!active || locations.empty()) return;

    // Disable ImGui window rounding for a cleaner look
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    renderTopBar(sw, sh);
    renderCharSprite(sw, sh);
    renderDialogueBox(sw, sh);

    ImGui::PopStyleVar(3);
}

// ── Top location bar ─────────────────────────────────────────────────────────
void VNManager::renderTopBar(int sw, int sh)
{
    const float barH = (float)sh * 0.062f;

    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ (float)sw, barH });
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, COL_TRANSPARENT);

    ImGui::Begin("##vn_topbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Dark bar background
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({ 0, 0 }, { (float)sw, barH },
                      IM_COL32(13, 5, 36, 235));

    // Accent line at bottom of bar
    dl->AddLine({ 0, barH - 2 }, { (float)sw, barH - 2 },
                IM_COL32(200, 60, 10, 200), 2.0f);

    // Location tab buttons
    const float tabW   = (float)sw * 0.14f;
    const float tabH   = barH - 8.0f;
    const float startX = 12.0f;
    const float startY = 4.0f;

    for (int i = 0; i < (int)locations.size(); ++i)
    {
        bool isActive = (i == locationIdx);
        ImVec4 col  = isActive ? COL_TAB_ACTIVE : COL_TAB_IDLE;
        ImVec2 tl   = { startX + i * (tabW + 6.0f), startY };
        ImVec2 br   = { tl.x + tabW, tl.y + tabH };

        dl->AddRectFilled(tl, br, ImGui::ColorConvertFloat4ToU32(col), 3.0f);
        if (isActive)
            dl->AddRect(tl, br, IM_COL32(220, 80, 20, 200), 3.0f, 0, 1.5f);

        // Clickable invisible button over the tab
        ImGui::SetCursorPos({ tl.x, tl.y });
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,        COL_TRANSPARENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1,1,1,0.08f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1,1,1,0.15f });
        if (ImGui::Button(("##tab" + std::to_string(i)).c_str(), { tabW, tabH }))
            changeLocation(i);
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        // Tab label — centered
        ImVec2 textSz = ImGui::CalcTextSize(locations[i].name.c_str());
        ImVec2 textPos = {
            tl.x + (tabW - textSz.x) * 0.5f,
            tl.y + (tabH - textSz.y) * 0.5f
        };
        ImU32 textCol = isActive
            ? IM_COL32(255, 240, 220, 255)
            : IM_COL32(160, 140, 200, 200);
        dl->AddText(textPos, textCol, locations[i].name.c_str());
    }

    // Exit button (top-right)
    const float exitW  = 90.0f;
    const float exitX  = (float)sw - exitW - 12.0f;
    ImGui::SetCursorPos({ exitX, startY });
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0.55f, 0.10f, 0.05f, 0.85f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.80f, 0.15f, 0.08f, 0.95f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 1.00f, 0.20f, 0.10f, 1.00f });
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    if (ImGui::Button("Back to Game", { exitW, tabH }))
        exit();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::End();
    ImGui::PopStyleColor(); // COL_TRANSPARENT window bg
}

// ── Character sprite ─────────────────────────────────────────────────────────
void VNManager::renderCharSprite(int sw, int sh)
{
    unsigned int texId = getActiveSpriteTexture();
    if (texId == 0) return;

    const float topBarH   = (float)sh * 0.062f;
    const float dialogueH = (float)sh * 0.26f;
    const float availH    = (float)sh - topBarH - dialogueH;
    const float spriteH   = availH * 0.98f;
    const float spriteW   = spriteH * 0.65f;
    const float spriteX   = ((float)sw - spriteW) * 0.5f;
    const float spriteY   = topBarH + (availH - spriteH) * 0.5f;

    ImGui::SetNextWindowPos({ spriteX, spriteY });
    ImGui::SetNextWindowSize({ spriteW, spriteH });
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, COL_TRANSPARENT);

    ImGui::Begin("##vn_sprite", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs);

    ImGui::Image((ImTextureID)(intptr_t)texId, { spriteW, spriteH });

    ImGui::End();
    ImGui::PopStyleColor();
}

// ── Dialogue box ─────────────────────────────────────────────────────────────
void VNManager::renderDialogueBox(int sw, int sh)
{
    const auto* loc = getCurrentLocation();
    const auto* ch  = getCurrentCharacter();
    if (!loc || loc->lines.empty()) return;

    const float dlgH  = (float)sh * 0.26f;
    const float dlgY  = (float)sh - dlgH;
    const float padX  = (float)sw * 0.05f;

    ImGui::SetNextWindowPos({ 0, dlgY });
    ImGui::SetNextWindowSize({ (float)sw, dlgH });
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, COL_TRANSPARENT);

    ImGui::Begin("##vn_dialogue", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wPos    = ImGui::GetWindowPos();

    // Dark panel background with subtle top border
    dl->AddRectFilled(
        { wPos.x, wPos.y },
        { wPos.x + (float)sw, wPos.y + dlgH },
        IM_COL32(10, 4, 30, 230));
    dl->AddLine(
        { wPos.x, wPos.y },
        { wPos.x + (float)sw, wPos.y },
        IM_COL32(200, 60, 10, 180), 2.5f);

    // ── Name plate ───────────────────────────────────────────────────────────
    const float namePlateH = 28.0f;
    const float namePlateY = wPos.y + 10.0f;
    const float namePlateX = wPos.x + padX;
    std::string charName   = ch ? ch->name : "???";
    ImVec2 nameSz          = ImGui::CalcTextSize(charName.c_str());
    float  namePlateW      = nameSz.x + 24.0f;

    // Parallelogram shape via quad
    float slant = 10.0f;
    dl->AddQuadFilled(
        { namePlateX,              namePlateY },
        { namePlateX + namePlateW, namePlateY },
        { namePlateX + namePlateW - slant, namePlateY + namePlateH },
        { namePlateX - slant,              namePlateY + namePlateH },
        IM_COL32(200, 56, 10, 245));
    // Accent edge
    dl->AddLine(
        { namePlateX, namePlateY },
        { namePlateX + namePlateW, namePlateY },
        IM_COL32(255, 200, 80, 220), 2.0f);

    // Larger font for name plate
    float baseSize = ImGui::GetStyle().FontSizeBase;
    ImGui::PushFont(nullptr, baseSize * 1.25f);
    dl->AddText(
        { namePlateX + 12.0f, namePlateY + (namePlateH - ImGui::GetTextLineHeight()) * 0.5f },
        IM_COL32(255, 245, 220, 255),
        charName.c_str());
    ImGui::PopFont();

    // ── Dialogue text (typewriter) ────────────────────────────────────────────
    const std::string& fullText = loc->lines[lineIdx].text;
    std::string shown = fullText.substr(0, (size_t)visibleChars);

    ImGui::PushFont(nullptr, baseSize * 1.35f);
    ImGui::SetCursorPos({ padX, namePlateY - wPos.y + namePlateH + 12.0f });
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT);

    float wrapWidth = (float)sw - padX * 2.0f;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
    ImGui::TextWrapped("%s", shown.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();

    // ── Progress indicator ────────────────────────────────────────────────────
    float progressX = wPos.x + (float)sw - padX;
    float progressY = wPos.y + dlgH - 22.0f;

    if (lineComplete)
    {
        // Blinking triangle prompt
        float alpha = 0.55f + 0.45f * sinf((float)glfwGetTime() * 4.5f);
        ImU32 hintCol = IM_COL32(220, 100, 30, (int)(alpha * 255));
        dl->AddTriangleFilled(
            { progressX - 14.0f, progressY },
            { progressX,         progressY },
            { progressX - 7.0f,  progressY + 10.0f },
            hintCol);

        // "SPACE" hint
        ImVec2 hintSz = ImGui::CalcTextSize("SPACE");
        dl->AddText(
            { progressX - hintSz.x - 20.0f, progressY - 1.0f },
            IM_COL32(140, 120, 180, (int)(alpha * 220)),
            "SPACE");
    }
    else
    {
        // Progress bar showing typewriter completion
        float progress = (float)visibleChars / std::max(1.0f, (float)fullText.size());
        float barW     = 80.0f;
        dl->AddRectFilled(
            { progressX - barW, progressY + 2.0f },
            { progressX,        progressY + 6.0f },
            IM_COL32(50, 30, 80, 180), 2.0f);
        dl->AddRectFilled(
            { progressX - barW, progressY + 2.0f },
            { progressX - barW + barW * progress, progressY + 6.0f },
            IM_COL32(200, 60, 10, 220), 2.0f);
    }

    // ── Line counter ──────────────────────────────────────────────────────────
    char counter[32];
    snprintf(counter, sizeof(counter), "%d / %d",
             lineIdx + 1, (int)loc->lines.size());
    ImVec2 cntSz = ImGui::CalcTextSize(counter);
    dl->AddText(
        { wPos.x + (float)sw * 0.5f - cntSz.x * 0.5f, wPos.y + dlgH - 20.0f },
        IM_COL32(100, 80, 140, 160),
        counter);

    ImGui::End();
    ImGui::PopStyleColor();
}
