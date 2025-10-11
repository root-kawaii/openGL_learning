#include "ui.h"

void UIManager::onMovePressed(std::string value)
{
    std::cout << "Move button pressed" << std::endl;
    isCharacterMoving = true;
}

void UIManager::onActPressed(std::string value)
{
    std::cout << "Act button pressed" << std::endl;
    // Add your act logic here
}

void UIManager::onWaitPressed(std::string value)
{
    std::cout << "Wait button pressed" << std::endl;
    // Add your wait logic here
}

void UIManager::onStatusPressed(std::string value)
{
    std::cout << "Status button pressed" << std::endl;
    // Add your status logic here
}

void UIManager::onAutoBattlePressed(std::string value)
{
    std::cout << "Auto-battle button pressed" << std::endl;
    // Add your auto-battle logic here
}

UIManager::UIManager(unsigned int height, unsigned int width) : uiShader("shaders/ui_box_shader.vs", "shaders/ui_box_shader.fs"),
                                                                textShader("shaders/text.vs", "shaders/text.fs")
{
    // Initialize OpenGL objects
    screenHeight = height;
    screenWidth = width;
    textShaderProgram = textShader.ID;
    uiShaderProgram = uiShader.ID;
    setupQuadGeometry();
    setUpFont();
}

UIManager::~UIManager()
{
    // Clean up OpenGL resources
    // glDeleteVertexArrays(1, &VAO);
    // glDeleteBuffers(1, &VBO);
    // glDeleteBuffers(1, &EBO);
    glDeleteProgram(uiShaderProgram);
}

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
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++)
    {
        // load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
            continue;
        }

        // generate texture
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer);

        // set texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // now store character for later use
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            face->glyph->advance.x};
        characters.insert(std::pair<char, Character>(c, character));
    }

    // Clean up resources after processing all characters
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore default alignment
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

    return 0; // success
}

void UIManager::setupQuadGeometry()
{
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glGenBuffers(1, &uiEBO);

    glBindVertexArray(uiVAO);

    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    // Allocate buffer for dynamic data
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);

    unsigned int indices[] = {
        0, 1, 2, // first triangle
        2, 3, 0  // second triangle
    };

    glGenBuffers(1, &uiEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uiEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Vertex attribute: vec4 (pos.x, pos.y, tex.x, tex.y)
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}
void UIManager::renderUIBBox(float width, float height, float x_pos,
                             float y_pos)
{
    // Default white color
    renderUIBBox(width, height, x_pos, y_pos, 1.0f, 1.0f, 0.5f, 0.5f);
}

void UIManager::renderUIBBox(float width, float height, float x_pos, float y_pos,
                             float r, float g, float b, float alpha)
{

    // Save state
    GLboolean depthTest, blend, cullFace;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_BLEND, &blend);
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

    // Set up state for text rendering
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(uiShaderProgram);

    // Calculate resolution-independent scale
    const float REFERENCE_WIDTH = 1440.0f;
    const float REFERENCE_HEIGHT = 1440.0f;
    float scaleX = static_cast<float>(screenWidth) / REFERENCE_WIDTH;
    float scaleY = static_cast<float>(screenHeight) / REFERENCE_HEIGHT;

    // Scale dimensions and position
    float scaledWidth = width * scaleX;
    float scaledHeight = height * scaleY;
    float scaledX = x_pos * scaleX;
    float scaledY = y_pos * scaleY;

    // Calculate actual vertex positions (like RenderText does)
    float xpos = scaledX;
    float ypos = scaledY;
    float w = scaledWidth;
    float h = scaledHeight;

    // Create vertices with actual screen coordinates
    float vertices[4][4] = {
        {xpos, ypos + h, 0.0f, 1.0f},     // top left
        {xpos + w, ypos + h, 1.0f, 1.0f}, // top right
        {xpos + w, ypos, 1.0f, 0.0f},     // bottom right
        {xpos, ypos, 0.0f, 0.0f}          // bottom left
    };

    // Create projection matrix (bottom-left origin)
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(screenWidth),
                                      0.0f, static_cast<float>(screenHeight));

    // Set uniforms
    int projectionLoc = glGetUniformLocation(uiShaderProgram, "projection");
    int colorLoc = glGetUniformLocation(uiShaderProgram, "color");
    int useTextureLoc = glGetUniformLocation(uiShaderProgram, "useTexture");

    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform4f(colorLoc, r, g, b, alpha);
    glUniform1i(useTextureLoc, 0); // Not using texture

    // Update vertex buffer with calculated positions
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Render the quad
    glBindVertexArray(uiVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Restore previous state
    if (depthTest)
        glEnable(GL_DEPTH_TEST);
    if (cullFace)
        glEnable(GL_CULL_FACE);
    if (!blend)
        glDisable(GL_BLEND);
    else
        glBlendFunc(blendSrc, blendDst);
}

void UIManager::setProjectionMatrix(const glm::mat4 &projectionMatrix)
{
    glUseProgram(uiShaderProgram);

    int projectionLoc = glGetUniformLocation(uiShaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
}

void UIManager::RenderText(std::string text, float x, float y, float scale, glm::vec3 color)
{
    // Save state
    GLboolean depthTest, blend, cullFace;
    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_BLEND, &blend);
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    GLint blendSrc, blendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

    // Set up state for text rendering
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    textShader.use();
    glUniform1i(glGetUniformLocation(textShader.ID, "text"), 0);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(screenWidth), 0.0f, static_cast<float>(screenHeight));
    glUniformMatrix4fv(glGetUniformLocation(textShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(textShader.ID, "textColor"), color.x, color.y, color.z);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    const float REFERENCE_WIDTH = 1440.0f;
    float resolutionScale = static_cast<float>(screenWidth) / REFERENCE_WIDTH;
    float finalScale = scale * resolutionScale;

    for (std::string::const_iterator c = text.begin(); c != text.end(); c++)
    {
        Character ch = characters[*c];
        float xpos = x + ch.Bearing.x * finalScale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * finalScale;
        float w = ch.Size.x * finalScale;
        float h = ch.Size.y * finalScale;

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

        x += (ch.Advance >> 6) * finalScale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Restore previous state
    if (depthTest)
        glEnable(GL_DEPTH_TEST);
    if (cullFace)
        glEnable(GL_CULL_FACE);
    if (!blend)
        glDisable(GL_BLEND);
    else
        glBlendFunc(blendSrc, blendDst);
}

void UIManager::renderGameMenu()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Reference dimensions (assuming 1440x1440 reference)
    const float screenH = 1440.0f;

    // Colors
    glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    glm::vec3 accent(0.9f, 0.8f, 0.3f); // Yellow accent
    glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    glm::vec3 textGray(0.7f, 0.7f, 0.7f);

    // === TOP LEFT: Power Grid Panel ===
    float powerPanelX = 20.0f;
    float powerPanelY = screenH - 120.0f; // 20px from top
    float powerPanelW = 500.0f;
    float powerPanelH = 100.0f;

    // Background
    renderUIBBox(powerPanelW, powerPanelH, powerPanelX, powerPanelY,
                 bgDark.r, bgDark.g, bgDark.b, 0.9f);

    // Border
    float borderThick = 2.0f;
    renderUIBBox(powerPanelW, borderThick, powerPanelX, powerPanelY + powerPanelH,
                 accent.r, accent.g, accent.b, 1.0f);

    // "POWER GRID" label
    RenderText("POWER GRID", powerPanelX + 20, powerPanelY + powerPanelH - 30,
               0.5f, textWhite);

    // Power bars (7 bars like in the image)
    float barStartX = powerPanelX + 150;
    float barY = powerPanelY + 40;
    float barW = 30.0f;
    float barH = 40.0f;
    float barSpacing = 10.0f;

    for (int i = 0; i < 7; i++)
    {
        float barX = barStartX + i * (barW + barSpacing);
        renderUIBBox(barW, barH, barX, barY,
                     0.9f, 0.5f, 0.2f, 1.0f); // Orange bars
    }

    // "GRID DEFENSE 15%" text
    RenderText("CLOCK ATB", powerPanelX + 340, powerPanelY + 50,
               0.4f, textGray);
    RenderText("15%", powerPanelX + 440, powerPanelY + 25,
               0.6f, accent);

    // === TOP RIGHT: Victory Timer ===
    float victoryPanelW = 280.0f;
    float victoryPanelH = 70.0f;
    float victoryPanelX = 1440.0f - victoryPanelW - 20.0f;
    float victoryPanelY = screenH - 90.0f;

    renderUIBBox(victoryPanelW, victoryPanelH, victoryPanelX, victoryPanelY,
                 bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(victoryPanelW, borderThick, victoryPanelX, victoryPanelY + victoryPanelH,
                 accent.r, accent.g, accent.b, 1.0f);

    RenderText("Victory in", victoryPanelX + 20, victoryPanelY + 40,
               0.5f, textGray);
    RenderText("7", victoryPanelX + 180, victoryPanelY + 30,
               1.2f, textWhite);
    RenderText("turns", victoryPanelX + 230, victoryPanelY + 40,
               0.5f, textGray);

    // === LEFT SIDE: Unit Selection Panel ===
    float unitPanelX = 20.0f;
    float unitPanelY = screenH - 340.0f;
    float unitPanelW = 150.0f;
    float unitPanelH = 200.0f;

    renderUIBBox(unitPanelW, unitPanelH, unitPanelX, unitPanelY,
                 bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(unitPanelW, borderThick, unitPanelX, unitPanelY + unitPanelH,
                 accent.r, accent.g, accent.b, 1.0f);

    // Unit icons (2 units shown)
    float iconSize = 60.0f;
    float iconSpacing = 10.0f;

    // Unit 1
    renderUIBBox(iconSize, iconSize, unitPanelX + 10, unitPanelY + unitPanelH - 70,
                 0.2f, 0.3f, 0.4f, 1.0f);
    renderUIBBox(iconSize - 4, 4, unitPanelX + 12, unitPanelY + unitPanelH - 74,
                 0.3f, 0.8f, 0.3f, 1.0f); // Green HP bar

    // Unit 2
    renderUIBBox(iconSize, iconSize, unitPanelX + 80, unitPanelY + unitPanelH - 70,
                 0.2f, 0.4f, 0.3f, 1.0f);
    renderUIBBox(iconSize - 4, 4, unitPanelX + 82, unitPanelY + unitPanelH - 74,
                 0.3f, 0.8f, 0.3f, 1.0f); // Green HP bar

    // "Cycle Unit" button
    float cycleButtonY = unitPanelY + 20;
    renderUIBBox(unitPanelW - 20, 40, unitPanelX + 10, cycleButtonY,
                 bgLight.r, bgLight.g, bgLight.b, 1.0f);
    RenderText("Cycle Unit", unitPanelX + 25, cycleButtonY + 12,
               0.4f, textWhite);

    // === BOTTOM LEFT: Combat Mech Panel ===
    float mechPanelX = 20.0f;
    float mechPanelY = 20.0f; // Bottom of screen
    float mechPanelW = 380.0f;
    float mechPanelH = 220.0f;

    renderUIBBox(mechPanelW, mechPanelH, mechPanelX, mechPanelY,
                 bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(mechPanelW, borderThick, mechPanelX, mechPanelY + mechPanelH,
                 accent.r, accent.g, accent.b, 1.0f);

    // "Combat Mech" title
    RenderText("Combat Mech", mechPanelX + 80, mechPanelY + mechPanelH - 30,
               0.6f, textWhite);

    // Mech icon
    float mechIconSize = 140.0f;
    renderUIBBox(mechIconSize, mechIconSize, mechPanelX + 10, mechPanelY + 10,
                 0.15f, 0.2f, 0.25f, 1.0f);

    // Weapon slots (3 weapons)
    float weaponSlotX = mechPanelX + 170;
    float weaponSlotSize = 60.0f;
    float weaponSpacing = 8.0f;

    for (int i = 0; i < 3; i++)
    {
        float slotX = weaponSlotX + i * (weaponSlotSize + weaponSpacing);
        renderUIBBox(weaponSlotSize, weaponSlotSize, slotX, mechPanelY + 20,
                     0.2f, 0.25f, 0.3f, 1.0f);

        // Weapon icon placeholder
        if (i == 0) // First weapon with ammo indicator
        {
            renderUIBBox(weaponSlotSize - 10, 6, slotX + 5, mechPanelY + 25,
                         0.3f, 0.7f, 0.9f, 1.0f); // Blue ammo bar
        }
    }

    // Stats
    RenderText("3", mechPanelX + 240, mechPanelY + mechPanelH - 60,
               0.8f, glm::vec3(0.3f, 0.8f, 0.3f)); // Move stat

    // === BOTTOM RIGHT: Ground Tile Info ===
    float tilePanelW = 260.0f;
    float tilePanelH = 120.0f;
    float tilePanelX = 1440.0f - tilePanelW - 20.0f;
    float tilePanelY = 20.0f;

    renderUIBBox(tilePanelW, tilePanelH, tilePanelX, tilePanelY,
                 bgDark.r, bgDark.g, bgDark.b, 0.9f);
    renderUIBBox(tilePanelW, borderThick, tilePanelX, tilePanelY + tilePanelH,
                 accent.r, accent.g, accent.b, 1.0f);

    // Tile icon
    float tileIconSize = 50.0f;
    renderUIBBox(tileIconSize, tileIconSize, tilePanelX + 20, tilePanelY + 50,
                 0.4f, 0.5f, 0.3f, 1.0f); // Green tile

    // === CENTER BOTTOM: Action Button ===
    float actionButtonW = 150.0f;
    float actionButtonH = 45.0f;
    float actionButtonX = (1440.0f - actionButtonW) / 2.0f;
    float actionButtonY = 30.0f;

    renderUIBBox(actionButtonW, actionButtonH, actionButtonX, actionButtonY,
                 bgLight.r, bgLight.g, bgLight.b, 1.0f);
    renderUIBBox(actionButtonW, borderThick, actionButtonX, actionButtonY + actionButtonH,
                 accent.r, accent.g, accent.b, 1.0f);

    glDisable(GL_BLEND);
}

void UIManager::renderStatusBars()
{
    float barWidth = 200.0f;
    float barHeight = 16.0f;
    float barCenterX = screenWidth - barWidth / 2 - 30.0f;
    float hpBarCenterY = screenHeight - 30.0f;
    float mpBarCenterY = screenHeight - 55.0f;

    // HP Bar background and fill
    renderUIBBox(barWidth, barHeight, barCenterX, hpBarCenterY, 0.2f, 0.2f, 0.2f, 0.9f);
    float hpFillWidth = barWidth * 0.75f;
    float hpFillCenterX = barCenterX - (barWidth - hpFillWidth) / 2;
    renderUIBBox(hpFillWidth, barHeight - 4.0f, hpFillCenterX, hpBarCenterY, 0.8f, 0.2f, 0.2f, 1.0f);

    // HP text (bottom-left coordinates) - position to the left of the bar
    float hpTextX = barCenterX - barWidth / 2 - 40.0f;
    float hpTextY = hpBarCenterY - 8.0f;
    RenderText("HP", hpTextX, hpTextY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));

    // MP Bar background and fill
    renderUIBBox(barWidth, barHeight, barCenterX, mpBarCenterY, 0.2f, 0.2f, 0.2f, 0.9f);
    float mpFillWidth = barWidth * 0.5f;
    float mpFillCenterX = barCenterX - (barWidth - mpFillWidth) / 2;
    renderUIBBox(mpFillWidth, barHeight - 4.0f, mpFillCenterX, mpBarCenterY, 0.2f, 0.2f, 0.8f, 1.0f);

    // MP text (bottom-left coordinates) - position to the left of the bar
    float mpTextX = barCenterX - barWidth / 2 - 40.0f;
    float mpTextY = mpBarCenterY - 8.0f;
    RenderText("MP", mpTextX, mpTextY, 0.4f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void UIManager::renderMenuDecorations(float menuCenterX, float menuCenterY, float menuWidth, float menuHeight)
{
    // Corner decorations - simple L-shaped brackets
    float decorSize = 25.0f;
    float decorThickness = 3.0f;
    float decorOffset = 15.0f; // Distance from corners

    // Calculate corner positions
    float leftX = menuCenterX - menuWidth / 2 + decorOffset;
    float rightX = menuCenterX + menuWidth / 2 - decorOffset;
    float topY = menuCenterY + menuHeight / 2 - decorOffset;
    float bottomY = menuCenterY - menuHeight / 2 + decorOffset;

    // Top-left corner L
    renderUIBBox(decorSize, decorThickness, leftX + decorSize / 2, topY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, leftX, topY - decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);

    // Top-right corner L
    renderUIBBox(decorSize, decorThickness, rightX - decorSize / 2, topY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, rightX, topY - decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);

    // Bottom-left corner L
    renderUIBBox(decorSize, decorThickness, leftX + decorSize / 2, bottomY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, leftX, bottomY + decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);

    // Bottom-right corner L
    renderUIBBox(decorSize, decorThickness, rightX - decorSize / 2, bottomY, 0.9f, 0.8f, 0.3f, 1.0f);
    renderUIBBox(decorThickness, decorSize, rightX, bottomY + decorSize / 2, 0.9f, 0.8f, 0.3f, 1.0f);
}

void UIManager::renderPauseMenu()
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    float selectedHeight = 100.0f;
    renderUIBBox(1440, 1440, -720.0f, 720.0f, 1.0f, 1.0f, 0.02f, 1.0f);
    renderUIBBox(500, 50, 250.0f, 740.0f, 1.0f, 0.0f, 0.00f, 1.0f);
    RenderText("HP", 720, 720, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    RenderText("HP", 720, 780, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    RenderText("HP", 720, 840, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    glDisable(GL_BLEND);
}

void UIManager::clearUIElements()
{
    uiElements.clear();
}

void UIManager::renderAllUIElements(float mouseX, float mouseY)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (auto &element : uiElements)
    {
        if (element.elementType == BOX)
        {
            if (isMouseOver(mouseX, mouseY, element.width, element.height,
                            element.x_pos, element.y_pos))
            {
                if (isPressed(element))
                {
                    executeUI(element);
                }
                float selectedColor_R = 1.0;
                float selectedColor_G = 0.6f;
                float selectedColor_B = 0.0f;
                renderUIBBox(element.width, element.height,
                             element.x_pos, element.y_pos,
                             selectedColor_R, selectedColor_G, selectedColor_B, element.a);
            }
            else
            {
                renderUIBBox(element.width, element.height,
                             element.x_pos, element.y_pos,
                             element.r, element.g, element.b, element.a);
            }
        }
        else if (element.elementType == TEXT)
        {
            RenderText(element.text, element.x_pos, element.y_pos,
                       element.scale, glm::vec3(element.r, element.g, element.b));
        }
    }

    glDisable(GL_BLEND);
}

// Refactored buildGameMenu - creates elements instead of rendering
void UIManager::buildGameMenu()
{
    clearUIElements();

    const float screenH = 1440.0f;

    // Colors
    glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    glm::vec3 accent(0.9f, 0.8f, 0.3f);
    glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    glm::vec3 textGray(0.7f, 0.7f, 0.7f);

    // === TOP LEFT: Power Grid Panel ===
    float powerPanelX = 20.0f;
    float powerPanelY = screenH - 120.0f;
    float powerPanelW = 500.0f;
    float powerPanelH = 100.0f;

    addBox(powerPanelW, powerPanelH, powerPanelX, powerPanelY,
           bgDark.r, bgDark.g, bgDark.b, 0.9f);

    float borderThick = 2.0f;
    addBox(powerPanelW, borderThick, powerPanelX, powerPanelY + powerPanelH,
           accent.r, accent.g, accent.b, 1.0f);

    addText("POWER GRID", powerPanelX + 20, powerPanelY + powerPanelH - 30,
            0.5f, textWhite.r, textWhite.g, textWhite.b);

    // Power bars
    float barStartX = powerPanelX + 150;
    float barY = powerPanelY + 40;
    float barW = 30.0f;
    float barH = 40.0f;
    float barSpacing = 10.0f;

    for (int i = 0; i < 7; i++)
    {
        float barX = barStartX + i * (barW + barSpacing);
        addBox(barW, barH, barX, barY, 0.9f, 0.5f, 0.2f, 1.0f);
    }

    addText("CLOCK ATB", powerPanelX + 340, powerPanelY + 50,
            0.4f, textGray.r, textGray.g, textGray.b);
    addText("15%", powerPanelX + 440, powerPanelY + 25,
            0.6f, accent.r, accent.g, accent.b);

    // === TOP RIGHT: Victory Timer ===
    float victoryPanelW = 280.0f;
    float victoryPanelH = 70.0f;
    float victoryPanelX = 1440.0f - victoryPanelW - 20.0f;
    float victoryPanelY = screenH - 90.0f;

    addBox(victoryPanelW, victoryPanelH, victoryPanelX, victoryPanelY,
           bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(victoryPanelW, borderThick, victoryPanelX, victoryPanelY + victoryPanelH,
           accent.r, accent.g, accent.b, 1.0f);

    addText("Victory in", victoryPanelX + 20, victoryPanelY + 40,
            0.5f, textGray.r, textGray.g, textGray.b);
    addText("7", victoryPanelX + 180, victoryPanelY + 30,
            1.2f, textWhite.r, textWhite.g, textWhite.b);
    addText("turns", victoryPanelX + 230, victoryPanelY + 40,
            0.5f, textGray.r, textGray.g, textGray.b);

    // === LEFT SIDE: Unit Selection Panel ===
    float unitPanelX = 20.0f;
    float unitPanelY = screenH - 340.0f;
    float unitPanelW = 150.0f;
    float unitPanelH = 200.0f;

    addBox(unitPanelW, unitPanelH, unitPanelX, unitPanelY,
           bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(unitPanelW, borderThick, unitPanelX, unitPanelY + unitPanelH,
           accent.r, accent.g, accent.b, 1.0f);

    float iconSize = 60.0f;

    // Unit 1
    addBox(iconSize, iconSize, unitPanelX + 10, unitPanelY + unitPanelH - 70,
           0.2f, 0.3f, 0.4f, 1.0f);
    addBox(iconSize - 4, 4, unitPanelX + 12, unitPanelY + unitPanelH - 74,
           0.3f, 0.8f, 0.3f, 1.0f);

    // Unit 2
    addBox(iconSize, iconSize, unitPanelX + 80, unitPanelY + unitPanelH - 70,
           0.2f, 0.4f, 0.3f, 1.0f);
    addBox(iconSize - 4, 4, unitPanelX + 82, unitPanelY + unitPanelH - 74,
           0.3f, 0.8f, 0.3f, 1.0f);

    // "Cycle Unit" button
    float cycleButtonY = unitPanelY + 20;
    addBox(unitPanelW - 20, 40, unitPanelX + 10, cycleButtonY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f);
    addText("Cycle Unit", unitPanelX + 25, cycleButtonY + 12,
            0.4f, textWhite.r, textWhite.g, textWhite.b);

    // === BOTTOM LEFT: Combat Mech Panel ===
    float mechPanelX = 20.0f;
    float mechPanelY = 20.0f;
    float mechPanelW = 380.0f;
    float mechPanelH = 220.0f;

    addBox(mechPanelW, mechPanelH, mechPanelX, mechPanelY,
           bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(mechPanelW, borderThick, mechPanelX, mechPanelY + mechPanelH,
           accent.r, accent.g, accent.b, 1.0f);

    addText("Combat Mech", mechPanelX + 80, mechPanelY + mechPanelH - 30,
            0.6f, textWhite.r, textWhite.g, textWhite.b);

    // Mech icon
    float mechIconSize = 140.0f;
    addBox(mechIconSize, mechIconSize, mechPanelX + 10, mechPanelY + 10,
           0.15f, 0.2f, 0.25f, 1.0f);

    // Weapon slots
    float weaponSlotX = mechPanelX + 170;
    float weaponSlotSize = 60.0f;
    float weaponSpacing = 8.0f;

    for (int i = 0; i < 3; i++)
    {
        float slotX = weaponSlotX + i * (weaponSlotSize + weaponSpacing);
        addBox(weaponSlotSize, weaponSlotSize, slotX, mechPanelY + 20,
               0.2f, 0.25f, 0.3f, 1.0f);

        if (i == 0)
        {
            addBox(weaponSlotSize - 10, 6, slotX + 5, mechPanelY + 25,
                   0.3f, 0.7f, 0.9f, 1.0f);
        }
    }

    addText("3", mechPanelX + 240, mechPanelY + mechPanelH - 60,
            0.8f, 0.3f, 0.8f, 0.3f);

    // === BOTTOM RIGHT: Ground Tile Info ===
    float tilePanelW = 260.0f;
    float tilePanelH = 120.0f;
    float tilePanelX = 1440.0f - tilePanelW - 20.0f;
    float tilePanelY = 20.0f;

    addBox(tilePanelW, tilePanelH, tilePanelX, tilePanelY,
           bgDark.r, bgDark.g, bgDark.b, 0.9f);
    addBox(tilePanelW, borderThick, tilePanelX, tilePanelY + tilePanelH,
           accent.r, accent.g, accent.b, 1.0f);

    float tileIconSize = 50.0f;
    addBox(tileIconSize, tileIconSize, tilePanelX + 20, tilePanelY + 50,
           0.4f, 0.5f, 0.3f, 1.0f);

    addText("Ground Tile", tilePanelX + 85, tilePanelY + 80,
            0.5f, textWhite.r, textWhite.g, textWhite.b);
    addText("No special effect.", tilePanelX + 85, tilePanelY + 50,
            0.35f, textGray.r, textGray.g, textGray.b);

    // === CENTER BOTTOM: Action Button ===
    float actionButtonW = 150.0f;
    float actionButtonH = 45.0f;
    float actionButtonX = (1440.0f - actionButtonW) / 2.0f;
    float actionButtonY = 30.0f;

    addBox(actionButtonW, actionButtonH, actionButtonX, actionButtonY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f);
    addBox(actionButtonW, borderThick, actionButtonX, actionButtonY + actionButtonH,
           accent.r, accent.g, accent.b, 1.0f);

    addText("(A) Move Unit", actionButtonX + 15, actionButtonY + 15,
            0.45f, textWhite.r, textWhite.g, textWhite.b);
}

bool UIManager::isMouseOver(float mouseX, float mouseY, float element_width, float element_height,
                            float element_x_pos, float element_y_pos)
{
    // Flip mouseY to match OpenGL's bottom-left coordinate system
    mouseY = screenHeight - mouseY;

    // Apply the same scaling used in renderUIBBox
    const float REFERENCE_WIDTH = 1440.0f;
    const float REFERENCE_HEIGHT = 1440.0f;
    float scaleX = static_cast<float>(screenWidth) / REFERENCE_WIDTH;
    float scaleY = static_cast<float>(screenHeight) / REFERENCE_HEIGHT;

    // Scale dimensions and position (matching renderUIBBox logic)
    float scaledWidth = element_width * scaleX;
    float scaledHeight = element_height * scaleY;
    float scaledX = element_x_pos * scaleX;
    float scaledY = element_y_pos * scaleY;

    // Check if mouse is within the scaled bounds
    if (mouseX > scaledX && mouseX < scaledX + scaledWidth &&
        mouseY > scaledY && mouseY < scaledY + scaledHeight)
    {
        return true;
    }
    return false;
}

bool UIManager::isPressed(UIElement element)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        // std::cout << element.elementType + " pressed" + std::to_string(glfwGetTime()) << std::endl;
        return true;
    }
    return element.pressed;
}

bool UIManager::executeUI(UIElement element)
{
    if (element.functionPtr != nullptr)
    {
        element.functionPtr("ciao");
    }
    return true;
}

void UIManager::buildBottomCenterMenu()
{
    const float screenH = 1440.0f;
    const float screenW = 1440.0f;

    glm::vec3 bgDark(0.1f, 0.1f, 0.15f);
    glm::vec3 bgLight(0.15f, 0.15f, 0.2f);
    glm::vec3 accent(0.9f, 0.8f, 0.3f);
    glm::vec3 textWhite(1.0f, 1.0f, 1.0f);
    glm::vec3 textGray(0.7f, 0.7f, 0.7f);

    float menuWidth = 200.0f;
    float menuHeight = 280.0f;
    float menuX = (screenW - menuWidth) / 2.0f;
    float menuY = 80.0f;

    // Background panel
    addBox(menuWidth, menuHeight, menuX, menuY,
           bgDark.r, bgDark.g, bgDark.b, 0.95f);

    // Borders
    float borderThick = 2.0f;
    addBox(menuWidth, borderThick, menuX, menuY + menuHeight,
           accent.r, accent.g, accent.b, 1.0f); // Top
    addBox(menuWidth, borderThick, menuX, menuY,
           accent.r, accent.g, accent.b, 1.0f); // Bottom
    addBox(borderThick, menuHeight, menuX, menuY,
           accent.r, accent.g, accent.b, 1.0f); // Left
    addBox(borderThick, menuHeight, menuX + menuWidth, menuY,
           accent.r, accent.g, accent.b, 1.0f); // Right

    // Menu title
    addText("Menu", menuX + 20, menuY + menuHeight - 30,
            0.5f, textGray.r, textGray.g, textGray.b);

    // Button dimensions
    float buttonWidth = menuWidth - 40.0f;
    float buttonHeight = 38.0f;
    float buttonX = menuX + 20.0f;
    float buttonSpacing = 8.0f;
    float startY = menuY + menuHeight - 70.0f;

    // Move button
    auto moveCallback = [this](std::string value)
    { this->onMovePressed(value); };
    addBox(buttonWidth, buttonHeight, buttonX, startY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f, moveCallback);
    addText("Move", buttonX + 15, startY + 10,
            0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Act button
    float actY = startY - (buttonHeight + buttonSpacing);
    auto actCallback = [this](std::string value)
    { this->onActPressed(value); };
    addBox(buttonWidth, buttonHeight, buttonX, actY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f, actCallback);
    addText("Act", buttonX + 15, actY + 10,
            0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Wait button
    float waitY = actY - (buttonHeight + buttonSpacing);
    auto waitCallback = [this](std::string value)
    { this->onWaitPressed(value); };
    addBox(buttonWidth, buttonHeight, buttonX, waitY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f, waitCallback);
    addText("Wait", buttonX + 15, waitY + 10,
            0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Status button
    float statusY = waitY - (buttonHeight + buttonSpacing);
    auto statusCallback = [this](std::string value)
    { this->onStatusPressed(value); };
    addBox(buttonWidth, buttonHeight, buttonX, statusY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f, statusCallback);
    addText("Status", buttonX + 15, statusY + 10,
            0.45f, textWhite.r, textWhite.g, textWhite.b);

    // Auto-battle button
    float autoY = statusY - (buttonHeight + buttonSpacing);
    auto autoCallback = [this](std::string value)
    { this->onAutoBattlePressed(value); };
    addBox(buttonWidth, buttonHeight, buttonX, autoY,
           bgLight.r, bgLight.g, bgLight.b, 1.0f, autoCallback);
    addText("Auto-battle", buttonX + 15, autoY + 10,
            0.45f, textWhite.r, textWhite.g, textWhite.b);
}
