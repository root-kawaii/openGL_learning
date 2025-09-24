#include "ui.h"

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
    // Quad vertices (normalized coordinates from 0 to 1)
    float vertices[] = {
        // positions
        0.0f, 1.0f, // top left
        1.0f, 1.0f, // top right
        1.0f, 0.0f, // bottom right
        0.0f, 0.0f  // bottom left
    };

    unsigned int indices[] = {
        0, 1, 2, // first triangle
        2, 3, 0  // second triangle
    };

    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glGenBuffers(1, &uiEBO);

    glBindVertexArray(uiVAO);

    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uiEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void UIManager::renderUIBBox(float width, float height, float x_pos,
                             float y_pos)
{
    // Default white color
    renderUIBBox(width, height, x_pos, y_pos, 1.0f, 1.0f, 0.5f, 0.5f);
}

void UIManager::renderUIBBox(float width, float height, float x_pos,
                             float y_pos, float r, float g, float b,
                             float alpha)
{
    glUseProgram(uiShaderProgram);

    // Convert from bottom-left origin to top-left origin for the shader
    float converted_y = screenHeight - y_pos - height;

    // Set uniforms
    int positionLoc = glGetUniformLocation(uiShaderProgram, "position");
    int sizeLoc = glGetUniformLocation(uiShaderProgram, "size");
    int screenSizeLoc = glGetUniformLocation(uiShaderProgram, "screenSize");
    int colorLoc = glGetUniformLocation(uiShaderProgram, "color");

    glUniform2f(positionLoc, x_pos, converted_y);
    glUniform2f(sizeLoc, width, height);
    glUniform2f(screenSizeLoc, screenWidth, screenHeight);
    glUniform4f(colorLoc, r, g, b, alpha);

    // Render the quad
    glBindVertexArray(uiVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void UIManager::setProjectionMatrix(const glm::mat4 &projectionMatrix)
{
    glUseProgram(uiShaderProgram);

    int projectionLoc = glGetUniformLocation(uiShaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
}

void UIManager::RenderText(std::string text, float x, float y, float scale, glm::vec3 color)
{
    // activate corresponding render state
    textShader.use();
    // Use bottom-left origin projection matrix to match our coordinate system
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(screenWidth), 0.0f, static_cast<float>(screenHeight));
    glUniformMatrix4fv(glGetUniformLocation(textShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(textShader.ID, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    // iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++)
    {
        Character ch = characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;
        // update VBO for each character
        float vertices[6][4] = {
            {xpos, ypos + h, 0.0f, 0.0f},
            {xpos, ypos, 0.0f, 1.0f},
            {xpos + w, ypos, 1.0f, 1.0f},

            {xpos, ypos + h, 0.0f, 0.0f},
            {xpos + w, ypos, 1.0f, 1.0f},
            {xpos + w, ypos + h, 1.0f, 0.0f}};
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64)
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void UIManager::renderGameMenu()
{
    // Menu configuration - position at the very bottom of screen
    float menuHeight = screenHeight * 0.25f; // Bottom 25% of screen
    float menuWidth = screenWidth * 0.9f;    // 90% of screen width
    float menuCenterX = screenWidth * 0.5f;  // Center X of screen
    float menuCenterY = menuHeight * 0.5f;   // Center Y at bottom (half menu height from bottom)

    // Main menu background with semi-transparent dark blue
    renderUIBBox(menuWidth, menuHeight, menuCenterX, menuCenterY, 0.1f, 0.1f, 0.3f, 0.85f);

    // Decorative border
    float borderWidth = 4.0f;
    // Top border
    renderUIBBox(menuWidth, borderWidth, menuCenterX, menuCenterY + menuHeight / 2 - borderWidth / 2, 0.8f, 0.7f, 0.2f, 1.0f);
    // Bottom border
    renderUIBBox(menuWidth, borderWidth, menuCenterX, menuCenterY - menuHeight / 2 + borderWidth / 2, 0.8f, 0.7f, 0.2f, 1.0f);
    // Left border
    renderUIBBox(borderWidth, menuHeight, menuCenterX - menuWidth / 2 + borderWidth / 2, menuCenterY, 0.8f, 0.7f, 0.2f, 1.0f);
    // Right border
    renderUIBBox(borderWidth, menuHeight, menuCenterX + menuWidth / 2 - borderWidth / 2, menuCenterY, 0.8f, 0.7f, 0.2f, 1.0f);

    // Action buttons configuration
    struct MenuOption
    {
        std::string text;
        float centerX, centerY, width, height;
        bool isSelected;
    };

    // Define menu options
    std::vector<MenuOption> menuOptions = {
        {"ATTACK", 0, 0, 0, 0, false},
        {"MAGIC", 0, 0, 0, 0, false},
        {"ITEMS", 0, 0, 0, 0, false},
        {"DEFEND", 0, 0, 0, 0, true}, // Example: Defend is selected
        {"SPECIAL", 0, 0, 0, 0, false},
        {"RUN", 0, 0, 0, 0, false}};

    // Calculate button dimensions and positions
    float buttonWidth = (menuWidth - 1.0f) / 3.0f;    // 3 columns with more padding
    float buttonHeight = (menuHeight - 60.0f) / 2.0f; // 2 rows with padding
    float buttonSpacingX = 20.0f;
    float buttonSpacingY = 15.0f;

    // Calculate the grid starting position
    float totalGridWidth = 3 * buttonWidth + 2 * buttonSpacingX;
    float totalGridHeight = 2 * buttonHeight + buttonSpacingY;

    // Center the grid within the menu
    float gridStartX = menuCenterX - totalGridWidth / 2 + buttonWidth / 2;
    float gridStartY = menuCenterY + totalGridHeight / 2 - buttonHeight / 2;

    // Position buttons in a 3x2 grid
    for (size_t i = 0; i < menuOptions.size() && i < 6; ++i)
    {
        int col = i % 3;
        int row = i / 3;

        menuOptions[i].centerX = gridStartX + col * (buttonWidth + buttonSpacingX);
        menuOptions[i].centerY = gridStartY - row * (buttonHeight + buttonSpacingY);
        menuOptions[i].width = buttonWidth;
        menuOptions[i].height = buttonHeight;
    }

    // Render each menu option
    for (const auto &option : menuOptions)
    {
        if (option.text.empty())
            continue;

        // Button background colors
        float r, g, b, alpha;
        if (option.isSelected)
        {
            // Selected button - bright blue with glow effect
            r = 0.2f;
            g = 0.5f;
            b = 1.0f;
            alpha = 0.9f;

            // Glow effect - render a slightly larger box behind
            float glowExpansion = 8.0f;
            renderUIBBox(option.width + glowExpansion, option.height + glowExpansion,
                         option.centerX + 1, option.centerY + 1,
                         0.4f, 0.7f, 1.0f, 0.4f);
        }
        else
        {
            // Unselected button - darker blue
            r = 0.15f;
            g = 0.25f;
            b = 0.4f;
            alpha = 0.8f;
        }

        // Render button background
        renderUIBBox(option.width, option.height, option.centerX + 1, option.centerY + 1, r, g, b, alpha);

        // Button border
        float btnBorderWidth = 2.0f;
        float borderR = option.isSelected ? 1.0f : 0.6f;
        float borderG = option.isSelected ? 0.8f : 0.6f;
        float borderB = option.isSelected ? 0.2f : 0.6f;

        // Render borders
        renderUIBBox(option.width, btnBorderWidth, option.centerX + 1, +1 + option.centerY + option.height / 2 - btnBorderWidth / 2,
                     borderR, borderG, borderB, 1.0f);
        renderUIBBox(option.width, btnBorderWidth, option.centerX + 1, option.centerY - option.height / 2 + btnBorderWidth / 2,
                     borderR, borderG, borderB, 1.0f);
        renderUIBBox(btnBorderWidth, option.height, +1 + option.centerX - option.width / 2 + btnBorderWidth / 2, +1 + option.centerY,
                     borderR, borderG, borderB, 1.0f);
        renderUIBBox(btnBorderWidth, option.height, +1 + option.centerX + option.width / 2 - btnBorderWidth / 2, +1 + option.centerY,
                     borderR, borderG, borderB, 1.0f);

        // Calculate text position (RenderText uses bottom-left coordinates)
        float textScale = 0.6f;                       // Smaller scale for better fit
        float estimatedCharWidth = 20.0f * textScale; // More accurate character width estimation
        float textWidth = option.text.length() * estimatedCharWidth;
        float textHeight = 48.0f * textScale; // Font height

        // Center the text in the button
        float textX = option.centerX - textWidth / 2;
        float textY = option.centerY - textHeight / 4; // Adjust for better vertical centering with baseline

        // Text color
        glm::vec3 textColor = option.isSelected ? glm::vec3(1.0f, 1.0f, 0.9f) : glm::vec3(0.9f, 0.9f, 0.9f);

        // Render button text
        RenderText(option.text, textX, textY, textScale, textColor);
    }

    // // Add HP/MP status bars in top-right corner
    // renderStatusBars();

    // // Add decorative elements
    // renderMenuDecorations(menuCenterX, menuCenterY, menuWidth, menuHeight);
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