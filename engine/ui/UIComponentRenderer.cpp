#include "UIComponentRenderer.hpp"
#include "UIComponents.hpp"

namespace impgine {

    void UIComponentRenderer::renderButton(UIRenderer& renderer, UIButton& button, const glm::vec2& windowContentPos) {
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

            renderer.drawText(button.label, glm::vec2(textX, textY), button.textColor);
        }
    }

    void UIComponentRenderer::renderInputField(UIRenderer& renderer, UIInputField& input, const glm::vec2& windowContentPos) {
        glm::vec2 absolutePos = windowContentPos + input.position;
        glm::vec4 bgColor = input.getCurrentBackgroundColor();

        // Draw input field background
        renderer.drawRect(absolutePos, input.size, bgColor);

        // Draw text or placeholder
        glm::vec2 textPos = absolutePos + glm::vec2(input.padding, (input.size.y - 16.0f) * 0.5f);

        if (!input.text.empty()) {
            renderer.drawText(input.text, textPos, input.textColor);
        } else if (!input.placeholder.empty()) {
            renderer.drawText(input.placeholder, textPos, input.placeholderColor);
        }

        // Draw cursor if focused
        if (input.state == UIComponentState::Focused) {
            float cursorX = textPos.x + input.cursorPosition * 10.0f; // Rough character width
            glm::vec2 cursorPos(cursorX, absolutePos.y + 4.0f);
            glm::vec2 cursorSize(2.0f, input.size.y - 8.0f);
            renderer.drawRect(cursorPos, cursorSize, input.cursorColor);
        }
    }

    void UIComponentRenderer::renderLabel(UIRenderer& renderer, UILabel& label, const glm::vec2& windowContentPos) {
        glm::vec2 absolutePos = windowContentPos + label.position;

        if (!label.text.empty()) {
            renderer.drawText(label.text, absolutePos, label.textColor);
        }
    }

    void UIComponentRenderer::renderCheckbox(UIRenderer& renderer, UICheckbox& checkbox, const glm::vec2& windowContentPos) {
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
            renderer.drawText(checkbox.label, textPos, checkbox.textColor);
        }
    }

    void UIComponentRenderer::renderSlider(UIRenderer& renderer, UISlider& slider, const glm::vec2& windowContentPos) {
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

    void UIComponentRenderer::renderComponent(UIRenderer& renderer, UIComponent& component, const glm::vec2& windowContentPos) {
        if (!component.isVisible) return;

        switch (component.type) {
            case UIComponentType::Button:
                renderButton(renderer, static_cast<UIButton&>(component), windowContentPos);
                break;
            case UIComponentType::InputField:
                renderInputField(renderer, static_cast<UIInputField&>(component), windowContentPos);
                break;
            case UIComponentType::Label:
                renderLabel(renderer, static_cast<UILabel&>(component), windowContentPos);
                break;
            case UIComponentType::Checkbox:
                renderCheckbox(renderer, static_cast<UICheckbox&>(component), windowContentPos);
                break;
            case UIComponentType::Slider:
                renderSlider(renderer, static_cast<UISlider&>(component), windowContentPos);
                break;
        }
    }

} // namespace impgine
