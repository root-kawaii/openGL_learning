#include "ui.h"
#include "game.h"
// Tracy profiler
#include "../tracy/public/tracy/Tracy.hpp"
#include "../tracy/public/tracy/TracyOpenGL.hpp"

// =============================================================================
// CALLBACKS
// =============================================================================
void UIManager::onMovePressed(std::string value)
{
    std::cout << "Move button pressed" << std::endl;
    isCharacterMoving = true;
}

void UIManager::onActPressed(std::string value)
{
    std::cout << "Act button pressed" << std::endl;
}

void UIManager::onWaitPressed(std::string value)
{
    std::cout << "Wait button pressed" << std::endl;
}

void UIManager::onStatusPressed(std::string value)
{
    std::cout << "Status button pressed" << std::endl;
}

void UIManager::onAutoBattlePressed(std::string value)
{
    std::cout << "Auto-battle button pressed" << std::endl;
}

void UIManager::onEndTurnPressed(std::string value)
{
    std::cout << "End Turn button pressed" << std::endl;
    if (gameInstance)
    {
        gameInstance->endPlayerTurn();
    }
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
            face->glyph->advance.x};
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

    // Colors
    const glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    const glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    const glm::vec3 accent(0.9f, 0.8f, 0.3f);
    const glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    const glm::vec3 textGray(0.7f, 0.7f, 0.7f);
    const float border = 2.0f;

    // === TOP LEFT: Power Grid Panel ===
    // Y position: 1440 - 120 = 1320 (panel bottom-left Y in reference coords)
    float pwrX = 20.0f, pwrY = 1320.0f, pwrW = 500.0f, pwrH = 100.0f;
    addBox(pwrW, pwrH, pwrX, pwrY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(pwrW, border, pwrX, pwrY + pwrH, accent.r, accent.g, accent.b, 1.0f);
    addText("POWER GRID", pwrX + 20, pwrY + pwrH - 30, 0.5f, textWhite.r, textWhite.g, textWhite.b);

    // Power bars
    float barX = pwrX + 150, barY = pwrY + 40, barW = 30.0f, barH = 40.0f, barGap = 10.0f;
    for (int i = 0; i < 7; i++)
    {
        addBox(barW, barH, barX + i * (barW + barGap), barY, 0.9f, 0.5f, 0.2f, 1.0f);
    }
    addText("CLOCK ATB", pwrX + 340, pwrY + 50, 0.4f, textGray.r, textGray.g, textGray.b);
    addText("15%", pwrX + 440, pwrY + 25, 0.6f, accent.r, accent.g, accent.b);

    // === TOP RIGHT: Victory Timer ===
    float vicW = 280.0f, vicH = 70.0f, vicX = 1440.0f - vicW - 20.0f, vicY = 1350.0f;
    addBox(vicW, vicH, vicX, vicY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(vicW, border, vicX, vicY + vicH, accent.r, accent.g, accent.b, 1.0f);
    addText("Victory in", vicX + 20, vicY + 40, 0.5f, textGray.r, textGray.g, textGray.b);
    addText("7", vicX + 180, vicY + 30, 1.2f, textWhite.r, textWhite.g, textWhite.b);
    addText("turns", vicX + 230, vicY + 40, 0.5f, textGray.r, textGray.g, textGray.b);

    // === LEFT SIDE: Unit Selection Panel ===
    float unitX = 20.0f, unitY = 1100.0f, unitW = 150.0f, unitH = 200.0f;
    addBox(unitW, unitH, unitX, unitY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(unitW, border, unitX, unitY + unitH, accent.r, accent.g, accent.b, 1.0f);

    float iconSz = 60.0f;
    // Unit 1
    addBox(iconSz, iconSz, unitX + 10, unitY + unitH - 70, 0.2f, 0.3f, 0.4f, 1.0f);
    addBox(iconSz - 4, 4, unitX + 12, unitY + unitH - 74, 0.3f, 0.8f, 0.3f, 1.0f);
    // Unit 2
    addBox(iconSz, iconSz, unitX + 80, unitY + unitH - 70, 0.2f, 0.4f, 0.3f, 1.0f);
    addBox(iconSz - 4, 4, unitX + 82, unitY + unitH - 74, 0.3f, 0.8f, 0.3f, 1.0f);
    // Cycle button
    addBox(unitW - 20, 40, unitX + 10, unitY + 20, bgLight.r, bgLight.g, bgLight.b, 1.0f);
    addText("Cycle Unit", unitX + 25, unitY + 32, 0.4f, textWhite.r, textWhite.g, textWhite.b);

    // === BOTTOM LEFT: Combat Mech Panel ===
    float mechX = 20.0f, mechY = 20.0f, mechW = 380.0f, mechH = 220.0f;
    addBox(mechW, mechH, mechX, mechY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(mechW, border, mechX, mechY + mechH, accent.r, accent.g, accent.b, 1.0f);
    addText("Combat Mech", mechX + 80, mechY + mechH - 30, 0.6f, textWhite.r, textWhite.g, textWhite.b);

    // Mech icon
    addBox(140, 140, mechX + 10, mechY + 10, 0.15f, 0.2f, 0.25f, 1.0f);

    // Weapon slots
    float wpnX = mechX + 170, wpnSz = 60.0f, wpnGap = 8.0f;
    for (int i = 0; i < 3; i++)
    {
        float sx = wpnX + i * (wpnSz + wpnGap);
        addBox(wpnSz, wpnSz, sx, mechY + 20, 0.2f, 0.25f, 0.3f, 1.0f);
        if (i == 0)
        {
            addBox(wpnSz - 10, 6, sx + 5, mechY + 25, 0.3f, 0.7f, 0.9f, 1.0f);
        }
    }
    addText("3", mechX + 240, mechY + mechH - 60, 0.8f, 0.3f, 0.8f, 0.3f);

    // === BOTTOM RIGHT: Ground Tile Info ===
    float tileW = 260.0f, tileH = 120.0f, tileX = 1440.0f - tileW - 20.0f, tileY = 20.0f;
    addBox(tileW, tileH, tileX, tileY, bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(tileW, border, tileX, tileY + tileH, accent.r, accent.g, accent.b, 1.0f);
    addBox(50, 50, tileX + 20, tileY + 50, 0.4f, 0.5f, 0.3f, 1.0f);
    addText("Ground Tile", tileX + 85, tileY + 80, 0.5f, textWhite.r, textWhite.g, textWhite.b);
    addText("No special effect.", tileX + 85, tileY + 50, 0.35f, textGray.r, textGray.g, textGray.b);

    // === CENTER BOTTOM: Action Button ===
    float actW = 150.0f, actH = 45.0f, actX = (1440.0f - actW) / 2.0f, actY = 30.0f;
    addBox(actW, actH, actX, actY, bgLight.r, bgLight.g, bgLight.b, 1.0f);
    addBox(actW, border, actX, actY + actH, accent.r, accent.g, accent.b, 1.0f);
    addText("(A) Move Unit", actX + 15, actY + 15, 0.45f, textWhite.r, textWhite.g, textWhite.b);
}

void UIManager::buildBottomCenterMenu()
{
    const glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    const glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    const glm::vec3 accent(0.9f, 0.8f, 0.3f);
    const glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    const glm::vec3 textGray(0.7f, 0.7f, 0.7f);
    const float border = 2.0f;

    float menuW = 200.0f, menuH = 280.0f;
    float menuX = (1440.0f - menuW) / 2.0f, menuY = 80.0f;

    // Background
    addBox(menuW, menuH, menuX, menuY, bgDark.r, bgDark.g, bgDark.b, 0.95f);

    // Borders
    addBox(menuW, border, menuX, menuY + menuH, accent.r, accent.g, accent.b, 1.0f);
    addBox(menuW, border, menuX, menuY, accent.r, accent.g, accent.b, 1.0f);
    addBox(border, menuH, menuX, menuY, accent.r, accent.g, accent.b, 1.0f);
    addBox(border, menuH, menuX + menuW - border, menuY, accent.r, accent.g, accent.b, 1.0f);

    // Title
    addText("Menu", menuX + 70, menuY + menuH - 30, 0.5f, textGray.r, textGray.g, textGray.b);

    // Buttons
    float btnW = menuW - 40.0f, btnH = 38.0f;
    float btnX = menuX + 20.0f;
    float btnGap = 8.0f;
    float startY = menuY + menuH - 70.0f;

    // Move button
    float moveY = startY;
    addBox(btnW, btnH, btnX, moveY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onMovePressed(v); });
    addText("Move", btnX + 15, moveY + 10, 0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Act button
    float actY = startY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, actY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onActPressed(v); });
    addText("Act", btnX + 15, actY + 10, 0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Wait button
    float waitY = actY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, waitY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onWaitPressed(v); });
    addText("Wait", btnX + 15, waitY + 10, 0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Status button
    float statusY = waitY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, statusY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onStatusPressed(v); });
    addText("Status", btnX + 15, statusY + 10, 0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Auto-battle button
    float autoY = statusY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, autoY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onAutoBattlePressed(v); });
    addText("Auto-battle", btnX + 15, autoY + 10, 0.45f, textWhite.r, textWhite.g, textWhite.b);

    // End Turn button
    float endTurnY = autoY - (btnH + btnGap);
    addBox(btnW, btnH, btnX, endTurnY, bgLight.r, bgLight.g, bgLight.b, 1.0f,
           [this](std::string v)
           { onEndTurnPressed(v); });
    addText("End Turn", btnX + 15, endTurnY + 10, 0.45f, textWhite.r, textWhite.g, textWhite.b);
}

// =============================================================================
// MAIN RENDER LOOP
// =============================================================================
void UIManager::renderAllUIElements(float mouseX, float mouseY)
{
    ZoneScoped;

    buildGameMenu();
    buildBottomCenterMenu();

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

            if (hovered && isPressed(element))
            {
                executeUI(element);
            }

            if (hovered)
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