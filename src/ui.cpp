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

void UIManager::renderUIBBox(float width, float height, float x_pos, float y_pos,
                             float r, float g, float b, float alpha)
{
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

    // Convert from bottom-left origin to top-left origin for the shader
    float converted_y = screenHeight - scaledY - scaledHeight;

    // Set uniforms
    int positionLoc = glGetUniformLocation(uiShaderProgram, "position");
    int sizeLoc = glGetUniformLocation(uiShaderProgram, "size");
    int screenSizeLoc = glGetUniformLocation(uiShaderProgram, "screenSize");
    int colorLoc = glGetUniformLocation(uiShaderProgram, "color");

    glUniform2f(positionLoc, scaledX, converted_y);
    glUniform2f(sizeLoc, scaledWidth, scaledHeight);
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
    float selectedHeight = 100.0f;
    renderUIBBox(400, 400, 0.0f, 0.0f, 1.0f, 1.0f, 0.02f, 1.0f);
    // renderUIBBox(500, 50, 250.0f, 740.0f, 1.0f, 0.0f, 0.00f, 1.0f);
    RenderText("HP", 50, 50, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    RenderText("HP", 50, 100, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
    RenderText("HP", 50, 150, 1.0f, glm::vec3(0.0f, 0.0f, 1.0f));
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