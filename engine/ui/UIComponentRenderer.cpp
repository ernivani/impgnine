#include "UIComponentRenderer.hpp"
#include "UIComponents.hpp"
#include <iostream>

namespace impgine {

    void UIComponentRenderer::renderButton(UIRenderer& renderer, UIButton& button, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        glm::vec2 absolutePos = windowContentPos + button.position;
        glm::vec4 color = button.getCurrentColor();

        // Draw button background
        renderer.drawRect(absolutePos, button.size, color);

        // Draw button text centered
        if (!button.label.empty()) {
            // Calculate text position (center of button)
            float textWidth = button.label.length() * 10.0f; // Rough estimate
            float textX = absolutePos.x + (button.size.x - textWidth) * 0.5f;
            float textY = absolutePos.y + (button.size.y - 16.0f) * 0.5f; // 16 is font size estimate

            renderer.renderText(button.label, glm::vec2(textX, textY), 0.4f, button.textColor, verts);
        }
    }

    void UIComponentRenderer::renderInputField(UIRenderer& renderer, UIInputField& input, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        glm::vec2 absolutePos = windowContentPos + input.position;
        glm::vec4 bgColor = input.getCurrentBackgroundColor();

        // Draw input field background and border
        renderer.drawRect(absolutePos, input.size, bgColor);
        // Border color depends on state
        glm::vec4 borderCol = input.borderColor;
        if (input.state == UIComponentState::Hovered) borderCol = input.borderHoverColor;
        if (input.state == UIComponentState::Focused) borderCol = input.borderFocusedColor;
        if (input.borderWidth > 0.0f) {
            // Top
            renderer.drawRect(absolutePos, { input.size.x, input.borderWidth }, borderCol);
            // Bottom
            renderer.drawRect({ absolutePos.x, absolutePos.y + input.size.y - input.borderWidth }, { input.size.x, input.borderWidth }, borderCol);
            // Left
            renderer.drawRect(absolutePos, { input.borderWidth, input.size.y }, borderCol);
            // Right
            renderer.drawRect({ absolutePos.x + input.size.x - input.borderWidth, absolutePos.y }, { input.borderWidth, input.size.y }, borderCol);
        }

        // Draw text or placeholder
        glm::vec2 textPos = absolutePos + glm::vec2(input.padding, (input.size.y - 16.0f) * 0.5f);

        if (!input.text.empty()) {
            // Draw selection highlight if there is any
            if (input.hasSelection() && input.state == UIComponentState::Focused) {
                size_t selStart = std::min(input.selectionStart, input.selectionEnd);
                size_t selEnd = std::max(input.selectionStart, input.selectionEnd);

                float selectionX = textPos.x;
                float selectionEndX = textPos.x;
                auto textRenderer = renderer.getTextRenderer();
                if (textRenderer) {
                    float scale = 0.5f;
                    for (size_t i = 0; i < input.text.length(); ++i) {
                        const Character* ch = textRenderer->getCharacter(input.text[i]);
                        if (ch) {
                            float advance = (ch->advance >> 6) * scale;
                            if (i < selStart) {
                                selectionX += advance;
                                selectionEndX += advance;
                            } else if (i < selEnd) {
                                selectionEndX += advance;
                            }
                        }
                    }
                }

                float selectionWidth = selectionEndX - selectionX;
                glm::vec2 selectionPos(selectionX, absolutePos.y + 4.0f);
                glm::vec2 selectionSize(selectionWidth, input.size.y - 8.0f);
                renderer.drawRect(selectionPos, selectionSize, input.selectionColor);
            }

            // Clip/scroll horizontally by adjusting starting character based on available width
            auto textRenderer = renderer.getTextRenderer();
            size_t startIndex = std::min(input.scrollStart, input.text.length());
            if (textRenderer) {
                float scale = 0.5f;
                float available = input.size.x - 2.0f * input.padding;

                float widthFromStartToCursor = 0.0f;
                for (size_t i = startIndex; i < input.cursorPosition && i < input.text.length(); ++i) {
                    const Character* ch = textRenderer->getCharacter(input.text[i]);
                    if (ch) widthFromStartToCursor += (ch->advance >> 6) * scale;
                }
                // Scroll right if cursor beyond visible area
                while (widthFromStartToCursor > available && startIndex < input.text.length()) {
                    const Character* ch = textRenderer->getCharacter(input.text[startIndex]);
                    if (!ch) break;
                    widthFromStartToCursor -= (ch->advance >> 6) * scale;
                    startIndex++;
                }
                // Scroll left if there is space and we scrolled too far
                while (startIndex > 0 && widthFromStartToCursor + 20.0f < available) { // small padding
                    const Character* ch = textRenderer->getCharacter(input.text[startIndex - 1]);
                    if (!ch) break;
                    float adv = (ch->advance >> 6) * scale;
                    if (widthFromStartToCursor + adv > available) break;
                    startIndex--; widthFromStartToCursor += adv;
                }
            }
            input.scrollStart = std::min(startIndex, input.text.length());

            std::string visible = input.text.substr(input.scrollStart);
            renderer.renderText(visible, textPos, 0.5f, input.textColor, verts);
        }

        // Draw cursor if focused (don't show cursor when there's a selection)
        if (input.state == UIComponentState::Focused && !input.hasSelection()) {
            // Calculate actual text width up to cursor position
            float cursorX = textPos.x;
            auto textRenderer = renderer.getTextRenderer();
            if (textRenderer) {
                float scale = 0.5f;
                // Start from scrollStart to place cursor within visible region
                size_t startIndex = std::min(input.scrollStart, input.text.length());
                for (size_t i = startIndex; i < input.cursorPosition && i < input.text.length(); ++i) {
                    const Character* ch = textRenderer->getCharacter(input.text[i]);
                    if (ch) {
                        cursorX += (ch->advance >> 6) * scale;
                    }
                }
            }

            glm::vec2 cursorPos(cursorX, absolutePos.y + 4.0f);
            glm::vec2 cursorSize(2.0f, input.size.y - 8.0f);
            renderer.drawRect(cursorPos, cursorSize, input.cursorColor);
        }
    }

    void UIComponentRenderer::renderLabel(UIRenderer& renderer, UILabel& label, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        glm::vec2 absolutePos = windowContentPos + label.position;

        if (!label.text.empty()) {
            renderer.renderText(label.text, absolutePos, 0.4f, label.textColor, verts);
        }
    }

    void UIComponentRenderer::renderCheckbox(UIRenderer& renderer, UICheckbox& checkbox, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        (void)verts; // Not using text yet
        glm::vec2 absolutePos = windowContentPos + checkbox.position;

        // Draw checkbox box
        glm::vec2 boxSize(checkbox.boxSize, checkbox.boxSize);
        renderer.drawRect(absolutePos, boxSize, checkbox.boxColor);

        // Draw checkmark if checked
        if (checkbox.isChecked) {
            glm::vec2 checkPos = absolutePos + glm::vec2(4.0f, 4.0f);
            glm::vec2 checkSize = boxSize - glm::vec2(8.0f, 8.0f);
            renderer.drawRect(checkPos, checkSize, checkbox.checkColor);
        }

        // Draw label
        if (!checkbox.label.empty()) {
            glm::vec2 textPos = absolutePos + glm::vec2(checkbox.boxSize + checkbox.spacing, 2.0f);
            renderer.renderText(checkbox.label, textPos, 0.4f, checkbox.textColor, verts);
        }
    }

    void UIComponentRenderer::renderSlider(UIRenderer& renderer, UISlider& slider, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        (void)verts; // Not using text yet
        glm::vec2 absolutePos = windowContentPos + slider.position;

        // Calculate track position (centered vertically)
        float trackY = absolutePos.y + (slider.size.y - slider.trackHeight) * 0.5f;
        glm::vec2 trackPos(absolutePos.x, trackY);
        glm::vec2 trackSize(slider.size.x, slider.trackHeight);

        // Draw track
        renderer.drawRect(trackPos, trackSize, slider.trackColor);

        // Draw fill (from left to handle position)
        float fillWidth = slider.size.x * slider.getNormalizedValue();
        glm::vec2 fillSize(fillWidth, slider.trackHeight);
        renderer.drawRect(trackPos, fillSize, slider.fillColor);

        // Draw handle
        float handleX = absolutePos.x + fillWidth;
        float handleY = absolutePos.y + slider.size.y * 0.5f;
        glm::vec2 handlePos(handleX - slider.handleRadius, handleY - slider.handleRadius);
        glm::vec2 handleSize(slider.handleRadius * 2.0f, slider.handleRadius * 2.0f);
        renderer.drawRect(handlePos, handleSize, slider.handleColor);
    }

    void UIComponentRenderer::renderTreeNode(UIRenderer& renderer, UITreeNode& treeNode, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        glm::vec2 absolutePos = windowContentPos + treeNode.position;
        glm::vec4 color = treeNode.getCurrentColor();

        // Draw background
        renderer.drawRect(absolutePos, treeNode.size, color);

        // Calculate positions with indentation
        float indent = treeNode.getIndentOffset();
        float arrowX = absolutePos.x + indent;
        float textX = arrowX + treeNode.arrowSize + 4.0f;
        float textY = absolutePos.y + (treeNode.size.y - 16.0f) * 0.5f;

        // Draw expand/collapse arrow (simple triangle)
        float arrowY = absolutePos.y + (treeNode.size.y - treeNode.arrowSize) * 0.5f;
        glm::vec2 arrowPos(arrowX, arrowY);
        glm::vec2 arrowSize(treeNode.arrowSize, treeNode.arrowSize);

        // Draw arrow as a simple indicator (right for expanded, down for collapsed)
        if (treeNode.isExpanded) {
            // Down arrow (▼)
            renderer.drawRect(arrowPos + glm::vec2(treeNode.arrowSize * 0.5f - 1.0f, 0.0f),
                            glm::vec2(2.0f, treeNode.arrowSize), treeNode.arrowColor);
            renderer.drawRect(arrowPos + glm::vec2(0.0f, treeNode.arrowSize * 0.5f - 1.0f),
                            glm::vec2(treeNode.arrowSize, 2.0f), treeNode.arrowColor);
        } else {
            // Right arrow (▶)
            renderer.drawRect(arrowPos + glm::vec2(0.0f, treeNode.arrowSize * 0.5f - 1.0f),
                            glm::vec2(treeNode.arrowSize, 2.0f), treeNode.arrowColor);
            renderer.drawRect(arrowPos + glm::vec2(treeNode.arrowSize * 0.5f - 1.0f, 0.0f),
                            glm::vec2(2.0f, treeNode.arrowSize), treeNode.arrowColor);
        }

        // Draw label text
        if (!treeNode.label.empty()) {
            renderer.renderText(treeNode.label, glm::vec2(textX, textY), 0.4f, treeNode.textColor, verts);
        }

        // Draw drop indicator if dragging over
        if (treeNode.isDragging) {
            // Draw a semi-transparent overlay
            renderer.drawRect(absolutePos, treeNode.size, glm::vec4(0.5f, 0.5f, 0.5f, 0.3f));
        }
    }

    void UIComponentRenderer::renderComponent(UIRenderer& renderer, UIComponent& component, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts) {
        if (!component.isVisible) return;

        switch (component.type) {
            case UIComponentType::Button:
                renderButton(renderer, static_cast<UIButton&>(component), windowContentPos, verts);
                break;
            case UIComponentType::InputField:
                renderInputField(renderer, static_cast<UIInputField&>(component), windowContentPos, verts);
                break;
            case UIComponentType::Label:
                renderLabel(renderer, static_cast<UILabel&>(component), windowContentPos, verts);
                break;
            case UIComponentType::Checkbox:
                renderCheckbox(renderer, static_cast<UICheckbox&>(component), windowContentPos, verts);
                break;
            case UIComponentType::Slider:
                renderSlider(renderer, static_cast<UISlider&>(component), windowContentPos, verts);
                break;
            case UIComponentType::TreeNode:
                renderTreeNode(renderer, static_cast<UITreeNode&>(component), windowContentPos, verts);
                break;
        }
    }

} // namespace impgine
