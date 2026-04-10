#include "ui.h"
#include "game.h"
// Tracy profiler
#include "../tracy/public/tracy/Tracy.hpp"
#include "../tracy/public/tracy/TracyOpenGL.hpp"

#include <algorithm>
#include <cctype>

namespace
{
std::string formatUiLabel(std::string value)
{
    const size_t colonPos = value.find(':');
    if (colonPos != std::string::npos)
        value = value.substr(colonPos + 1);

    bool capitalize = true;
    for (char &c : value)
    {
        if (c == '_' || c == '-' || c == ':')
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

// =============================================================================
// CALLBACKS
// =============================================================================
void UIManager::onMovePressed(std::string value)
{
    std::cout << "Move button pressed" << std::endl;
    isCharacterMoving = true;
    isPassing = false;
    passTargetEntity = nullptr;
}

void UIManager::onShootPressed(std::string value)
{
    std::cout << "Shoot button pressed" << std::endl;
    if (!gameInstance)
        return;
    auto selected = gameInstance->getSelectedEntity();
    if (!selected || !selected->getHasBall())
        return;

    BufferedAction action;
    action.type = ActionType::SHOOT;
    action.targetPosition = glm::vec3(-2.45f, 2.85f, 0.18f);
    action.description = "Shoot";
    selected->bufferAction(action);

    isPassing = false;
    isCharacterMoving = false;
}

void UIManager::onPassPressed(std::string value)
{
    std::cout << "Pass button pressed" << std::endl;
    isPassing = true;
    isCharacterMoving = false;
    passTargetEntity = nullptr;
}

void UIManager::onWaitPressed(std::string value)
{
    std::cout << "Wait button pressed" << std::endl;
    if (!gameInstance)
        return;
    auto selected = gameInstance->getSelectedEntity();
    if (selected)
    {
        selected->hasMovedThisTurn = true;
        std::cout << selected->object->name << " is waiting." << std::endl;
    }
}

void UIManager::onEndTurnPressed(std::string value)
{
    std::cout << "End Turn button pressed" << std::endl;
    if (gameInstance)
    {
        gameInstance->endPlayerTurn();
    }
}

void UIManager::onExecuteTurnPressed(std::string value)
{
    std::cout << "Execute Turn button pressed" << std::endl;
    if (!gameInstance)
        return;

    isPassing = false;
    isCharacterMoving = false;
    passTargetEntity = nullptr;

    gameInstance->startTurnExecution();
}

void UIManager::onToggleInventoryPressed(std::string value)
{
    if (gameInstance)
        gameInstance->toggleInventoryScreen();
}

void UIManager::onLootChestPressed(std::string value)
{
    if (gameInstance)
        gameInstance->lootActiveChest();
}

void UIManager::onCloseGameplayPanelPressed(std::string value)
{
    if (gameInstance)
        gameInstance->closeGameplayModalUI();
}

// =============================================================================
// CONSTRUCTOR / DESTRUCTOR
// =============================================================================
UIManager::UIManager(unsigned int height, unsigned int width)
    : uiShader("shaders/ui_box_shader.vs", "shaders/ui_box_shader.fs"),
      textShader("shaders/text.vs", "shaders/text.fs")
{
    screenHeight = height;
    screenWidth = width;
    textShaderProgram = textShader.ID;
    uiShaderProgram = uiShader.ID;
    setupQuadGeometry();
    setUpFont();
}

UIManager::~UIManager()
{
    glDeleteProgram(uiShaderProgram);
}

// =============================================================================
// OPENGL SETUP
// =============================================================================
unsigned int UIManager::setUpFont()
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, std::filesystem::path("assets/fonts/BodoniXT.ttf").c_str(), 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        FT_Done_FreeType(ft);
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                     face->glyph->bitmap.width, face->glyph->bitmap.rows,
                     0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)};
        characters.insert(std::pair<char, Character>(c, character));
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return 0;
}

void UIManager::setupQuadGeometry()
{
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glGenBuffers(1, &uiEBO);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);

    unsigned int indices[] = {0, 1, 2, 2, 3, 0};

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uiEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// =============================================================================
// LOW-LEVEL RENDERING (SCREEN coordinates - already scaled)
// =============================================================================
void UIManager::renderBoxScreen(float x, float y, float w, float h,
                                float r, float g, float b, float a)
{
    GLboolean depthTest, blend, cullFace;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_BLEND, &blend);
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(uiShaderProgram);

    float vertices[4][4] = {
        {x, y + h, 0.0f, 1.0f},
        {x + w, y + h, 1.0f, 1.0f},
        {x + w, y, 1.0f, 0.0f},
        {x, y, 0.0f, 0.0f}};

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(screenWidth),
                                      0.0f, static_cast<float>(screenHeight));

    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform4f(glGetUniformLocation(uiShaderProgram, "color"), r, g, b, a);
    glUniform1i(glGetUniformLocation(uiShaderProgram, "useTexture"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(uiVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    if (depthTest)
        glEnable(GL_DEPTH_TEST);
    if (cullFace)
        glEnable(GL_CULL_FACE);
    if (!blend)
        glDisable(GL_BLEND);
    else
        glBlendFunc(blendSrc, blendDst);
}

void UIManager::renderTextScreen(const std::string &text, float x, float y,
                                 float scale, glm::vec3 color)
{
    GLboolean depthTest, blend, cullFace;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_BLEND, &blend);
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    textShader.use();
    glUniform1i(glGetUniformLocation(textShader.ID, "text"), 0);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(screenWidth),
                                      0.0f, static_cast<float>(screenHeight));
    glUniformMatrix4fv(glGetUniformLocation(textShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(textShader.ID, "textColor"), color.x, color.y, color.z);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    float currentX = x;
    for (char c : text)
    {
        Character ch = characters[c];
        float xpos = currentX + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            {xpos, ypos + h, 0.0f, 0.0f},
            {xpos, ypos, 0.0f, 1.0f},
            {xpos + w, ypos, 1.0f, 1.0f},
            {xpos, ypos + h, 0.0f, 0.0f},
            {xpos + w, ypos, 1.0f, 1.0f},
            {xpos + w, ypos + h, 1.0f, 0.0f}};

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        currentX += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (depthTest)
        glEnable(GL_DEPTH_TEST);
    if (cullFace)
        glEnable(GL_CULL_FACE);
    if (!blend)
        glDisable(GL_BLEND);
    else
        glBlendFunc(blendSrc, blendDst);
}

// =============================================================================
// PUBLIC RENDERING API (REFERENCE coordinates - 1440x1440)
// =============================================================================
void UIManager::renderUIBBox(float width, float height, float x_pos, float y_pos)
{
    renderUIBBox(width, height, x_pos, y_pos, 1.0f, 1.0f, 0.5f, 0.5f);
}

void UIManager::renderUIBBox(float width, float height, float x_pos, float y_pos,
                             float r, float g, float b, float alpha)
{
    // Convert from reference coordinates to screen coordinates
    renderBoxScreen(scaleX(x_pos), scaleY(y_pos), scaleX(width), scaleY(height), r, g, b, alpha);
}

void UIManager::RenderText(std::string text, float x, float y, float scale, glm::vec3 color)
{
    // Convert from reference coordinates to screen coordinates
    float screenX = scaleX(x);
    float screenY = scaleY(y);
    float screenScale = scale * (static_cast<float>(screenWidth) / REFERENCE_WIDTH);
    renderTextScreen(text, screenX, screenY, screenScale, color);
}

void UIManager::setProjectionMatrix(const glm::mat4 &projectionMatrix)
{
    glUseProgram(uiShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(uiShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));
}

// =============================================================================
// INPUT HANDLING (works with SCREEN coordinates)
// =============================================================================
bool UIManager::isMouseOver(float mouseX, float mouseY, float width, float height,
                            float x_pos, float y_pos)
{
    // Convert mouse Y from top-left origin (GLFW) to bottom-left origin (OpenGL)
    float flippedMouseY = static_cast<float>(screenHeight) - mouseY;

    // UIElements store SCREEN coordinates, so compare directly
    return (mouseX >= x_pos && mouseX <= x_pos + width &&
            flippedMouseY >= y_pos && flippedMouseY <= y_pos + height);
}

bool UIManager::isPressed(UIElement element)
{
    return glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
}

bool UIManager::executeUI(UIElement element)
{
    if (element.functionPtr != nullptr)
    {
        element.functionPtr("clicked");
    }
    return true;
}

// =============================================================================
// UI ELEMENT MANAGEMENT
// =============================================================================
void UIManager::clearUIElements()
{
    uiElements.clear();
}

// =============================================================================
// UI BUILDING (all values in REFERENCE coordinates - 1440x1440)
// addBox() and addText() in header automatically scale to screen coords
// =============================================================================
void UIManager::buildGameMenu()
{
    clearUIElements();

    if (!gameInstance)
        return;

    // Colors
    const glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    const glm::vec3 accent(0.9f, 0.8f, 0.3f);
    const glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    const float border = 2.0f;

    // === TOP CENTER: Playable Entity Selector ===
    auto entities = gameInstance->getScene()->getGameEntities();
    auto selectedEntity = gameInstance->getSelectedEntity();

    float btnW = 160.0f;
    float btnH = 50.0f;
    float btnGap = 10.0f;
    float totalW = entities.size() * btnW + (entities.size() - 1) * btnGap;
    float startX = (1440.0f - totalW) / 2.0f;
    float barY = 1370.0f;

    // Background bar
    addBox(totalW + 20.0f, btnH + 20.0f, startX - 10.0f, barY - 10.0f, bgDark.r, bgDark.g, bgDark.b, 0.85f);
    addBox(totalW + 20.0f, border, startX - 10.0f, barY + btnH + 10.0f, accent.r, accent.g, accent.b, 1.0f);

    for (size_t i = 0; i < entities.size(); i++)
    {
        float btnX = startX + i * (btnW + btnGap);
        bool isSelected = (selectedEntity && selectedEntity == entities[i]);

        // Button box - gold if selected, dark if not
        if (isSelected)
        {
            addBox(btnW, btnH, btnX, barY, accent.r, accent.g, accent.b, 0.9f);
            addText(entities[i]->object->name, btnX + 15, barY + 15, 0.45f, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            auto entity = entities[i];
            addBox(btnW, btnH, btnX, barY, 0.15f, 0.15f, 0.2f, 0.9f,
                   [this, entity](std::string v)
                   {
                       gameInstance->setSelectedEntity(entity);
                       isPassing = false;
                       isCharacterMoving = false;
                   });
            addText(entities[i]->object->name, btnX + 15, barY + 15, 0.45f, textWhite.r, textWhite.g, textWhite.b);
        }
    }
}

void UIManager::buildActionBufferUI()
{
    if (!gameInstance)
        return;

    auto selected = gameInstance->getSelectedEntity();
    if (!selected)
        return;

    const auto &buffer = selected->getActionBuffer();
    if (buffer.empty())
        return;

    const glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    const glm::vec3 accent(0.9f, 0.8f, 0.3f);
    const glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    const float border = 2.0f;

    float itemH = 35.0f;
    float itemGap = 5.0f;
    float panelW = 350.0f;
    float panelH = buffer.size() * (itemH + itemGap) + 40.0f;
    float panelX = (1440.0f - panelW) / 2.0f;
    float panelY = 1370.0f - panelH - 10.0f;

    // Background
    addBox(panelW, panelH, panelX, panelY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    // Top border
    addBox(panelW, border, panelX, panelY + panelH, accent.r, accent.g, accent.b, 1.0f);

    // Title
    addText("Queued Actions", panelX + 10.0f, panelY + panelH - 25.0f, 0.4f,
            accent.r, accent.g, accent.b);

    float xBtnSize = 25.0f;
    for (size_t i = 0; i < buffer.size(); i++)
    {
        float rowY = panelY + panelH - 45.0f - i * (itemH + itemGap);

        // Row background
        addBox(panelW - 20.0f, itemH, panelX + 10.0f, rowY,
               0.15f, 0.15f, 0.2f, 1.0f);

        // Action text
        std::string label = std::to_string(i + 1) + ". " + buffer[i].description;
        addText(label, panelX + 20.0f, rowY + 8.0f, 0.38f,
                textWhite.r, textWhite.g, textWhite.b);

        // X button
        float xBtnX = panelX + panelW - 20.0f - xBtnSize;
        int capturedIndex = static_cast<int>(i);
        auto entityPtr = selected;
        addBox(xBtnSize, xBtnSize, xBtnX, rowY + (itemH - xBtnSize) / 2.0f,
               0.6f, 0.15f, 0.15f, 1.0f,
               [entityPtr, capturedIndex](std::string v)
               {
                   if (entityPtr)
                   {
                       entityPtr->removeAction(capturedIndex);
                   }
               });
        addText("X", xBtnX + 7.0f, rowY + (itemH - xBtnSize) / 2.0f + 4.0f, 0.35f,
                1.0f, 1.0f, 1.0f);
    }
}

void UIManager::buildBottomCenterMenu()
{
    const glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    const glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    const glm::vec3 accent(0.9f, 0.8f, 0.3f);
    const glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    const glm::vec3 textGray(0.7f, 0.7f, 0.7f);
    const float border = 2.0f;

    float menuW = 200.0f, menuH = 250.0f;
    float menuX = (1440.0f - menuW) / 2.0f, menuY = 80.0f;

    // Background
    addBox(menuW, menuH, menuX, menuY, bgDark.r, bgDark.g, bgDark.b, 0.95f);

    // Borders
    addBox(menuW, border, menuX, menuY + menuH, accent.r, accent.g, accent.b, 1.0f);
    addBox(menuW, border, menuX, menuY, accent.r, accent.g, accent.b, 1.0f);
    addBox(border, menuH, menuX, menuY, accent.r, accent.g, accent.b, 1.0f);
    addBox(border, menuH, menuX + menuW - border, menuY, accent.r, accent.g, accent.b, 1.0f);

    // Title
    addText("Actions", menuX + 55, menuY + menuH - 30, 0.5f, textGray.r, textGray.g, textGray.b);

    // Buttons
    float btnW = menuW - 40.0f, btnH = 32.0f;
    float btnX = menuX + 20.0f;
    float btnGap = 6.0f;
    float startY = menuY + menuH - 65.0f;

    // Move button
    float moveY = startY;
    addBox(btnW, btnH, btnX, moveY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onMovePressed(v); });
    addText("Move", btnX + 15, moveY + 8, 0.4f, textWhite.r, textWhite.g, textWhite.b);

    // Shoot button
    float shootY = startY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, shootY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onShootPressed(v); });
    addText("Shoot", btnX + 15, shootY + 8, 0.4f, textWhite.r, textWhite.g, textWhite.b);

    // Pass button
    float passY = shootY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, passY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onPassPressed(v); });
    addText("Pass", btnX + 15, passY + 8, 0.4f, textWhite.r, textWhite.g, textWhite.b);

    // Wait button
    float waitY = passY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, waitY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onWaitPressed(v); });
    addText("Wait", btnX + 15, waitY + 8, 0.4f, textWhite.r, textWhite.g, textWhite.b);

    // Execute Turn button
    float executeTurnY = waitY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, executeTurnY, 0.2f, 0.35f, 0.2f, 1.0f,
           [this](std::string v)
           { onExecuteTurnPressed(v); });
    addText("Execute Turn", btnX + 15, executeTurnY + 8, 0.4f, textWhite.r, textWhite.g, textWhite.b);

    // End Turn button
    float endTurnY = executeTurnY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, endTurnY, 0.35f, 0.15f, 0.15f, 1.0f,
           [this](std::string v)
           { onEndTurnPressed(v); });
    addText("End Turn", btnX + 15, endTurnY + 8, 0.4f, textWhite.r, textWhite.g, textWhite.b);
}

void UIManager::buildGameplayHUD()
{
    if (!gameInstance)
        return;

    const Player &player = gameInstance->getPrimaryPlayer();
    const glm::vec3 panelColor(0.08f, 0.10f, 0.13f);
    const glm::vec3 accent(0.92f, 0.82f, 0.30f);
    const glm::vec3 white(1.0f, 1.0f, 1.0f);
    const glm::vec3 muted(0.72f, 0.74f, 0.78f);

    float statusX = 28.0f;
    float statusY = 1260.0f;
    float statusW = 320.0f;
    float statusH = 132.0f;
    addBox(statusW, statusH, statusX, statusY, panelColor.r, panelColor.g, panelColor.b, 0.88f);
    addBox(statusW, 3.0f, statusX, statusY + statusH - 3.0f, accent.r, accent.g, accent.b, 1.0f);
    addText(player.getName(), statusX + 18.0f, statusY + 92.0f, 0.46f, white.r, white.g, white.b);

    float barX = statusX + 18.0f;
    float healthBarY = statusY + 58.0f;
    float shieldBarY = statusY + 26.0f;
    float barW = statusW - 36.0f;
    float barH = 16.0f;
    float healthFill = player.getMaxHealth() > 0.0f ? player.getHealth() / player.getMaxHealth() : 0.0f;
    float shieldFill = player.getMaxShields() > 0.0f ? player.getShields() / player.getMaxShields() : 0.0f;

    addText("Health", barX, healthBarY + 20.0f, 0.28f, muted.r, muted.g, muted.b);
    addBox(barW, barH, barX, healthBarY, 0.18f, 0.18f, 0.20f, 1.0f);
    addBox(barW * std::clamp(healthFill, 0.0f, 1.0f), barH, barX, healthBarY, 0.78f, 0.20f, 0.20f, 1.0f);
    addText(std::to_string(static_cast<int>(player.getHealth())) + " / " +
                std::to_string(static_cast<int>(player.getMaxHealth())),
            barX + 4.0f, healthBarY + 2.0f, 0.24f, white.r, white.g, white.b);

    addText("Shields", barX, shieldBarY + 20.0f, 0.28f, muted.r, muted.g, muted.b);
    addBox(barW, barH, barX, shieldBarY, 0.18f, 0.18f, 0.20f, 1.0f);
    addBox(barW * std::clamp(shieldFill, 0.0f, 1.0f), barH, barX, shieldBarY, 0.22f, 0.55f, 0.92f, 1.0f);
    addText(std::to_string(static_cast<int>(player.getShields())) + " / " +
                std::to_string(static_cast<int>(player.getMaxShields())),
            barX + 4.0f, shieldBarY + 2.0f, 0.24f, white.r, white.g, white.b);

    float keysX = 1090.0f;
    float keysY = 1228.0f;
    float keysW = 322.0f;
    float keysH = std::max(102.0f, 54.0f + 28.0f * static_cast<float>(player.getKeys().size()));
    addBox(keysW, keysH, keysX, keysY, panelColor.r, panelColor.g, panelColor.b, 0.88f);
    addBox(keysW, 3.0f, keysX, keysY + keysH - 3.0f, accent.r, accent.g, accent.b, 1.0f);
    addText("Keys", keysX + 18.0f, keysY + keysH - 30.0f, 0.40f, white.r, white.g, white.b);

    float keyRowY = keysY + keysH - 62.0f;
    if (player.getKeys().empty())
    {
        addText("No keys collected", keysX + 18.0f, keyRowY, 0.28f, muted.r, muted.g, muted.b);
    }
    else
    {
        int shown = 0;
        for (const auto &[keyId, count] : player.getKeys())
        {
            addText(formatUiLabel(keyId) + "  x" + std::to_string(count),
                    keysX + 18.0f, keyRowY - shown * 28.0f, 0.28f, white.r, white.g, white.b);
            shown++;
            if (shown >= 6)
                break;
        }
    }

    const WeaponSlot *equipped = player.getEquippedWeapon();
    std::string weaponLabel = equipped ? equipped->displayName : "Unarmed";
    addBox(330.0f, 54.0f, 28.0f, 70.0f, panelColor.r, panelColor.g, panelColor.b, 0.82f);
    addText("Weapon", 46.0f, 102.0f, 0.28f, muted.r, muted.g, muted.b);
    addText(weaponLabel, 46.0f, 78.0f, 0.36f, white.r, white.g, white.b);

    addBox(150.0f, 44.0f, 1262.0f, 70.0f, 0.14f, 0.18f, 0.24f, 0.90f,
           [this](std::string v)
           { onToggleInventoryPressed(v); });
    addText("I  Inventory", 1282.0f, 84.0f, 0.34f, white.r, white.g, white.b);

    std::string prompt = gameInstance->getGameplayInteractionPrompt();
    if (!prompt.empty())
    {
        float promptW = 440.0f;
        float promptH = 54.0f;
        float promptX = (1440.0f - promptW) * 0.5f;
        float promptY = 84.0f;
        addBox(promptW, promptH, promptX, promptY, panelColor.r, panelColor.g, panelColor.b, 0.86f);
        addBox(promptW, 3.0f, promptX, promptY + promptH - 3.0f, accent.r, accent.g, accent.b, 1.0f);
        addText(prompt, promptX + 20.0f, promptY + 16.0f, 0.34f, white.r, white.g, white.b);
    }

    addText("1 Revolver  |  2 Sword  |  Shift Run  |  Ctrl Crouch  |  M Engine", 28.0f, 28.0f, 0.24f,
            muted.r, muted.g, muted.b);
}

void UIManager::buildInventoryPanel()
{
    if (!gameInstance)
        return;

    const Player &player = gameInstance->getPrimaryPlayer();
    const glm::vec3 overlay(0.02f, 0.03f, 0.05f);
    const glm::vec3 panel(0.08f, 0.10f, 0.13f);
    const glm::vec3 accent(0.92f, 0.82f, 0.30f);
    const glm::vec3 white(1.0f, 1.0f, 1.0f);
    const glm::vec3 muted(0.72f, 0.74f, 0.78f);

    addBox(1440.0f, 1440.0f, 0.0f, 0.0f, overlay.r, overlay.g, overlay.b, 0.62f);

    float panelW = 900.0f;
    float panelH = 860.0f;
    float panelX = (1440.0f - panelW) * 0.5f;
    float panelY = (1440.0f - panelH) * 0.5f;
    addBox(panelW, panelH, panelX, panelY, panel.r, panel.g, panel.b, 0.96f);
    addBox(panelW, 3.0f, panelX, panelY + panelH - 3.0f, accent.r, accent.g, accent.b, 1.0f);
    addText("Inventory", panelX + 28.0f, panelY + panelH - 46.0f, 0.62f, white.r, white.g, white.b);
    addText("ESC or I to close", panelX + 30.0f, panelY + panelH - 82.0f, 0.24f, muted.r, muted.g, muted.b);

    addBox(120.0f, 42.0f, panelX + panelW - 150.0f, panelY + panelH - 70.0f,
           0.22f, 0.18f, 0.18f, 1.0f,
           [this](std::string v)
           { onCloseGameplayPanelPressed(v); });
    addText("Close", panelX + panelW - 122.0f, panelY + panelH - 58.0f, 0.32f, white.r, white.g, white.b);

    float leftX = panelX + 32.0f;
    float rightX = panelX + panelW * 0.56f;
    float topY = panelY + panelH - 150.0f;

    addText("Vitals", leftX, topY, 0.40f, accent.r, accent.g, accent.b);
    addText("Health: " + std::to_string(static_cast<int>(player.getHealth())) + " / " +
                std::to_string(static_cast<int>(player.getMaxHealth())),
            leftX, topY - 42.0f, 0.30f, white.r, white.g, white.b);
    addText("Shields: " + std::to_string(static_cast<int>(player.getShields())) + " / " +
                std::to_string(static_cast<int>(player.getMaxShields())),
            leftX, topY - 76.0f, 0.30f, white.r, white.g, white.b);

    addText("Weapons", leftX, topY - 150.0f, 0.40f, accent.r, accent.g, accent.b);
    float weaponY = topY - 192.0f;
    if (player.getWeapons().empty())
    {
        addText("No weapons collected", leftX, weaponY, 0.28f, muted.r, muted.g, muted.b);
    }
    else
    {
        int shown = 0;
        for (const auto &weapon : player.getWeapons())
        {
            std::string line = weapon.displayName;
            if (weapon.equipped)
                line += "  [Equipped]";
            if (weapon.ammo >= 0)
                line += "  Ammo " + std::to_string(weapon.ammo);
            addText(line, leftX, weaponY - shown * 30.0f, 0.28f, white.r, white.g, white.b);
            shown++;
            if (shown >= 8)
                break;
        }
    }

    addText("Keys", rightX, topY, 0.40f, accent.r, accent.g, accent.b);
    float keyY = topY - 42.0f;
    if (player.getKeys().empty())
    {
        addText("No keys collected", rightX, keyY, 0.28f, muted.r, muted.g, muted.b);
    }
    else
    {
        int shown = 0;
        for (const auto &[keyId, count] : player.getKeys())
        {
            addText(formatUiLabel(keyId) + "  x" + std::to_string(count),
                    rightX, keyY - shown * 30.0f, 0.28f, white.r, white.g, white.b);
            shown++;
            if (shown >= 8)
                break;
        }
    }

    addText("Items", rightX, topY - 210.0f, 0.40f, accent.r, accent.g, accent.b);
    float itemY = topY - 252.0f;
    if (player.getInventory().empty())
    {
        addText("Inventory empty", rightX, itemY, 0.28f, muted.r, muted.g, muted.b);
    }
    else
    {
        int shown = 0;
        for (const auto &item : player.getInventory())
        {
            addText(item.displayName + "  x" + std::to_string(item.count),
                    rightX, itemY - shown * 30.0f, 0.28f, white.r, white.g, white.b);
            shown++;
            if (shown >= 10)
                break;
        }
    }
}

void UIManager::buildChestLootPanel()
{
    if (!gameInstance)
        return;

    const glm::vec3 overlay(0.02f, 0.03f, 0.05f);
    const glm::vec3 panel(0.08f, 0.10f, 0.13f);
    const glm::vec3 accent(0.92f, 0.82f, 0.30f);
    const glm::vec3 white(1.0f, 1.0f, 1.0f);
    const glm::vec3 muted(0.72f, 0.74f, 0.78f);

    addBox(1440.0f, 1440.0f, 0.0f, 0.0f, overlay.r, overlay.g, overlay.b, 0.54f);

    float panelW = 640.0f;
    float panelH = 420.0f;
    float panelX = (1440.0f - panelW) * 0.5f;
    float panelY = (1440.0f - panelH) * 0.5f;
    addBox(panelW, panelH, panelX, panelY, panel.r, panel.g, panel.b, 0.96f);
    addBox(panelW, 3.0f, panelX, panelY + panelH - 3.0f, accent.r, accent.g, accent.b, 1.0f);
    addText("Chest Loot", panelX + 26.0f, panelY + panelH - 44.0f, 0.56f, white.r, white.g, white.b);
    addText("E or button to take all", panelX + 28.0f, panelY + panelH - 80.0f, 0.24f, muted.r, muted.g, muted.b);

    auto loot = gameInstance->getActiveChestLoot();
    float listY = panelY + panelH - 132.0f;
    if (loot.empty())
    {
        addText("Chest is empty", panelX + 28.0f, listY, 0.32f, muted.r, muted.g, muted.b);
    }
    else
    {
        int shown = 0;
        for (const auto &item : loot)
        {
            addText("- " + formatUiLabel(item), panelX + 28.0f, listY - shown * 34.0f,
                    0.32f, white.r, white.g, white.b);
            shown++;
            if (shown >= 7)
                break;
        }
    }

    addBox(190.0f, 46.0f, panelX + 26.0f, panelY + 28.0f,
           0.18f, 0.32f, 0.20f, 1.0f,
           [this](std::string v)
           { onLootChestPressed(v); });
    addText("Take All", panelX + 74.0f, panelY + 42.0f, 0.34f, white.r, white.g, white.b);

    addBox(150.0f, 46.0f, panelX + panelW - 176.0f, panelY + 28.0f,
           0.22f, 0.18f, 0.18f, 1.0f,
           [this](std::string v)
           { onCloseGameplayPanelPressed(v); });
    addText("Close", panelX + panelW - 132.0f, panelY + 42.0f, 0.34f, white.r, white.g, white.b);
}

void UIManager::buildCurrentUI()
{
    clearUIElements();

    if (!gameInstance)
        return;

    if (gameInstance->getGameMode() == GAME)
    {
        buildGameplayHUD();
        if (gameInstance->isInventoryOpen())
            buildInventoryPanel();
        if (gameInstance->isChestOpen())
            buildChestLootPanel();
        return;
    }

    if (gameInstance->getGameMode() == ENGINE)
    {
        buildGameMenu();
        buildActionBufferUI();
        buildBottomCenterMenu();
    }
}

// =============================================================================
// MAIN RENDER LOOP
// =============================================================================
void UIManager::renderAllUIElements(float mouseX, float mouseY)
{
    ZoneScoped;

    buildCurrentUI();

    bool mouseDown = window && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool justClicked = mouseDown && !leftMousePressedLastFrame;
    leftMousePressedLastFrame = mouseDown;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (auto &element : uiElements)
    {
        if (element.elementType == BOX)
        {
            // Elements are stored in screen coordinates
            bool hovered = isMouseOver(mouseX, mouseY, element.width, element.height,
                                       element.x_pos, element.y_pos);

            if (hovered && element.functionPtr && justClicked)
            {
                executeUI(element);
            }

            if (hovered && element.functionPtr)
            {
                // Highlight color when hovered
                renderBoxScreen(element.x_pos, element.y_pos, element.width, element.height,
                                1.0f, 0.6f, 0.0f, element.a);
            }
            else
            {
                renderBoxScreen(element.x_pos, element.y_pos, element.width, element.height,
                                element.r, element.g, element.b, element.a);
            }
        }
        else if (element.elementType == TEXT)
        {
            // Text elements store screen coords and pre-scaled font size
            renderTextScreen(element.text, element.x_pos, element.y_pos, element.scale,
                             glm::vec3(element.r, element.g, element.b));
        }
    }

    glDisable(GL_BLEND);
}

// =============================================================================
// LEGACY STANDALONE RENDER FUNCTIONS (use REFERENCE coordinates)
// =============================================================================
void UIManager::renderGameMenu()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    const glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    const glm::vec3 accent(0.9f, 0.8f, 0.3f);
    const glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    const glm::vec3 textGray(0.7f, 0.7f, 0.7f);
    const float border = 2.0f;

    // === TOP LEFT: Power Grid Panel ===
    float pwrX = 20.0f, pwrY = 1320.0f, pwrW = 500.0f, pwrH = 100.0f;
    renderUIBBox(pwrW, pwrH, pwrX, pwrY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(pwrW, border, pwrX, pwrY + pwrH, accent.r, accent.g, accent.b, 1.0f);
    RenderText("POWER GRID", pwrX + 20, pwrY + pwrH - 30, 0.5f, textWhite);

    float barX = pwrX + 150, barY = pwrY + 40, barW = 30.0f, barH = 40.0f, barGap = 10.0f;
    for (int i = 0; i < 7; i++)
    {
        renderUIBBox(barW, barH, barX + i * (barW + barGap), barY, 0.9f, 0.5f, 0.2f, 1.0f);
    }
    RenderText("CLOCK ATB", pwrX + 340, pwrY + 50, 0.4f, textGray);
    RenderText("15%", pwrX + 440, pwrY + 25, 0.6f, accent);

    // === TOP RIGHT: Victory Timer ===
    float vicW = 280.0f, vicH = 70.0f, vicX = 1440.0f - vicW - 20.0f, vicY = 1350.0f;
    renderUIBBox(vicW, vicH, vicX, vicY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(vicW, border, vicX, vicY + vicH, accent.r, accent.g, accent.b, 1.0f);
    RenderText("Victory in", vicX + 20, vicY + 40, 0.5f, textGray);
    RenderText("7", vicX + 180, vicY + 30, 1.2f, textWhite);
    RenderText("turns", vicX + 230, vicY + 40, 0.5f, textGray);

    // === LEFT SIDE: Unit Selection Panel ===
    float unitX = 20.0f, unitY = 1100.0f, unitW = 150.0f, unitH = 200.0f;
    renderUIBBox(unitW, unitH, unitX, unitY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(unitW, border, unitX, unitY + unitH, accent.r, accent.g, accent.b, 1.0f);

    float iconSz = 60.0f;
    renderUIBBox(iconSz, iconSz, unitX + 10, unitY + unitH - 70, 0.2f, 0.3f, 0.4f, 1.0f);
    renderUIBBox(iconSz - 4, 4, unitX + 12, unitY + unitH - 74, 0.3f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(iconSz, iconSz, unitX + 80, unitY + unitH - 70, 0.2f, 0.4f, 0.3f, 1.0f);
    renderUIBBox(iconSz - 4, 4, unitX + 82, unitY + unitH - 74, 0.3f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(unitW - 20, 40, unitX + 10, unitY + 20, bgLight.r, bgLight.g, bgLight.b, 1.0f);
    RenderText("Cycle Unit", unitX + 25, unitY + 32, 0.4f, textWhite);

    // === BOTTOM LEFT: Combat Mech Panel ===
    float mechX = 20.0f, mechY = 20.0f, mechW = 380.0f, mechH = 220.0f;
    renderUIBBox(mechW, mechH, mechX, mechY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(mechW, border, mechX, mechY + mechH, accent.r, accent.g, accent.b, 1.0f);
    RenderText("Combat Mech", mechX + 80, mechY + mechH - 30, 0.6f, textWhite);

    renderUIBBox(140, 140, mechX + 10, mechY + 10, 0.15f, 0.2f, 0.25f, 1.0f);

    float wpnX = mechX + 170, wpnSz = 60.0f, wpnGap = 8.0f;
    for (int i = 0; i < 3; i++)
    {
        float sx = wpnX + i * (wpnSz + wpnGap);
        renderUIBBox(wpnSz, wpnSz, sx, mechY + 20, 0.2f, 0.25f, 0.3f, 1.0f);
        if (i == 0)
        {
            renderUIBBox(wpnSz - 10, 6, sx + 5, mechY + 25, 0.3f, 0.7f, 0.9f, 1.0f);
        }
    }
    RenderText("3", mechX + 240, mechY + mechH - 60, 0.8f, glm::vec3(0.3f, 0.8f, 0.3f));

    // === BOTTOM RIGHT: Ground Tile Info ===
    float tileW = 260.0f, tileH = 120.0f, tileX = 1440.0f - tileW - 20.0f, tileY = 20.0f;
    renderUIBBox(tileW, tileH, tileX, tileY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(tileW, border, tileX, tileY + tileH, accent.r, accent.g, accent.b, 1.0f);
    renderUIBBox(50, 50, tileX + 20, tileY + 50, 0.4f, 0.5f, 0.3f, 1.0f);

    // === CENTER BOTTOM: Action Button ===
    float actW = 150.0f, actH = 45.0f, actX = (1440.0f - actW) / 2.0f, actY = 30.0f;
    renderUIBBox(actW, actH, actX, actY, bgLight.r, bgLight.g, bgLight.b, 1.0f);
    renderUIBBox(actW, border, actX, actY + actH, accent.r, accent.g, accent.b, 1.0f);

    glDisable(GL_BLEND);
}

void UIManager::renderStatusBars()
{
    // Status bars use actual screen coordinates (top-right corner)
    float barWidth = 200.0f;
    float barHeight = 16.0f;
    float barX = screenWidth - barWidth - 30.0f;
    float hpBarY = screenHeight - 30.0f;
    float mpBarY = screenHeight - 55.0f;

    renderBoxScreen(barX, hpBarY, barWidth, barHeight, 0.2f, 0.2f, 0.2f, 0.9f);
    renderBoxScreen(barX, hpBarY, barWidth * 0.75f, barHeight - 4.0f, 0.8f, 0.2f, 0.2f, 1.0f);

    renderBoxScreen(barX, mpBarY, barWidth, barHeight, 0.2f, 0.2f, 0.2f, 0.9f);
    renderBoxScreen(barX, mpBarY, barWidth * 0.5f, barHeight - 4.0f, 0.2f, 0.2f, 0.8f, 1.0f);

    float textScale = 0.4f * (static_cast<float>(screenWidth) / REFERENCE_WIDTH);
    renderTextScreen("HP", barX - 40.0f, hpBarY + 4.0f, textScale, glm::vec3(1.0f));
    renderTextScreen("MP", barX - 40.0f, mpBarY + 4.0f, textScale, glm::vec3(1.0f));
}

void UIManager::renderMenuDecorations(float menuCenterX, float menuCenterY, float menuWidth, float menuHeight)
{
    float decorSize = 25.0f;
    float decorThickness = 3.0f;
    float decorOffset = 15.0f;

    float leftX = menuCenterX - menuWidth / 2 + decorOffset;
    float rightX = menuCenterX + menuWidth / 2 - decorOffset;
    float topY = menuCenterY + menuHeight / 2 - decorOffset;
    float bottomY = menuCenterY - menuHeight / 2 + decorOffset;

    // Top-left L
    renderUIBBox(decorSize, decorThickness, leftX + decorSize / 2, topY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, leftX, topY - decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);
    // Top-right L
    renderUIBBox(decorSize, decorThickness, rightX - decorSize / 2, topY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, rightX, topY - decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);
    // Bottom-left L
    renderUIBBox(decorSize, decorThickness, leftX + decorSize / 2, bottomY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, leftX, bottomY + decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);
    // Bottom-right L
    renderUIBBox(decorSize, decorThickness, rightX - decorSize / 2, bottomY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, rightX, bottomY + decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);
}

void UIManager::renderPauseMenu()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    // Full screen overlay (screen coordinates)
    renderBoxScreen(0, 0, screenWidth, screenHeight, 0.1f, 0.1f, 0.15f, 0.95f);

    float panelWidth = screenWidth * 0.4f;
    float panelHeight = screenHeight * 0.6f;
    float panelX = (screenWidth - panelWidth) / 2.0f;
    float panelY = (screenHeight - panelHeight) / 2.0f;

    renderBoxScreen(panelX, panelY, panelWidth, panelHeight, 0.2f, 0.2f, 0.25f, 1.0f);
    renderBoxScreen(panelX, panelY + panelHeight, panelWidth, 3.0f, 0.9f, 0.8f, 0.3f, 1.0f);

    float scale = static_cast<float>(screenHeight) / REFERENCE_HEIGHT;
    renderTextScreen("PAUSED", panelX + panelWidth * 0.35f, panelY + panelHeight - 60.0f * scale,
                     1.5f * scale, glm::vec3(0.9f, 0.8f, 0.3f));

    float itemY = panelY + panelHeight - 150.0f * scale;
    float itemSpacing = 60.0f * scale;

    renderTextScreen("Resume", panelX + panelWidth * 0.4f, itemY, scale, glm::vec3(1.0f));
    renderTextScreen("Settings", panelX + panelWidth * 0.4f, itemY - itemSpacing, scale, glm::vec3(1.0f));
    renderTextScreen("Exit", panelX + panelWidth * 0.4f, itemY - itemSpacing * 2, scale, glm::vec3(1.0f));

    glDisable(GL_BLEND);
}
