#pragma once

#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace impgine {

    enum class UIComponentType {
        Button,
        InputField,
        Label,
        Checkbox,
        Slider
    };

    enum class UIComponentState {
        Normal,
        Hovered,
        Pressed,
        Disabled,
        Focused
    };

    struct UIComponent {
        UIComponentType type;
        UIComponentState state { UIComponentState::Normal };

        glm::vec2 position { 0.0f, 0.0f };  // Relative to window content area
        glm::vec2 size { 100.0f, 30.0f };

        bool isVisible { true };
        bool isEnabled { true };

        virtual ~UIComponent() = default;

        bool isPointInComponent(const glm::vec2& point) const {
            return point.x >= position.x && point.x <= position.x + size.x &&
                   point.y >= position.y && point.y <= position.y + size.y;
        }
    };

    struct UIButton : public UIComponent {
        std::string label { "Button" };
        std::function<void()> onClick { nullptr };

        glm::vec4 normalColor { 0.4f, 0.5f, 0.7f, 1.0f };
        glm::vec4 hoverColor { 0.5f, 0.6f, 0.8f, 1.0f };
        glm::vec4 pressedColor { 0.3f, 0.4f, 0.6f, 1.0f };
        glm::vec4 disabledColor { 0.2f, 0.2f, 0.2f, 0.5f };
        glm::vec4 textColor { 1.0f, 1.0f, 1.0f, 1.0f };

        UIButton() {
            type = UIComponentType::Button;
        }

        glm::vec4 getCurrentColor() const {
            if (!isEnabled) return disabledColor;
            switch (state) {
                case UIComponentState::Pressed: return pressedColor;
                case UIComponentState::Hovered: return hoverColor;
                default: return normalColor;
            }
        }
    };

    struct UIInputField : public UIComponent {
        std::string text { "" };
        std::string placeholder { "Enter text..." };
        size_t cursorPosition { 0 };
        size_t selectionStart { 0 };
        size_t selectionEnd { 0 };
        size_t maxLength { 256 };

        glm::vec4 backgroundColor { 0.25f, 0.25f, 0.3f, 1.0f };
        glm::vec4 focusedColor { 0.3f, 0.3f, 0.4f, 1.0f };
        glm::vec4 textColor { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 placeholderColor { 0.6f, 0.6f, 0.6f, 1.0f };
        glm::vec4 cursorColor { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 selectionColor { 0.4f, 0.6f, 0.9f, 0.5f };

        float padding { 8.0f };

        UIInputField() {
            type = UIComponentType::InputField;
        }

        bool hasSelection() const {
            return selectionStart != selectionEnd;
        }

        void clearSelection() {
            selectionStart = selectionEnd = cursorPosition;
        }

        std::string getSelectedText() const {
            if (!hasSelection()) return "";
            size_t start = std::min(selectionStart, selectionEnd);
            size_t end = std::max(selectionStart, selectionEnd);
            return text.substr(start, end - start);
        }

        void deleteSelection() {
            if (!hasSelection()) return;
            size_t start = std::min(selectionStart, selectionEnd);
            size_t end = std::max(selectionStart, selectionEnd);
            text.erase(start, end - start);
            cursorPosition = start;
            clearSelection();
        }

        void insertChar(char c) {
            if (hasSelection()) {
                deleteSelection();
            }
            if (text.length() < maxLength && cursorPosition <= text.length()) {
                text.insert(cursorPosition, 1, c);
                cursorPosition++;
                clearSelection();
            }
        }

        void insertText(const std::string& str) {
            if (hasSelection()) {
                deleteSelection();
            }
            for (char c : str) {
                if (text.length() >= maxLength) break;
                if (cursorPosition <= text.length()) {
                    text.insert(cursorPosition, 1, c);
                    cursorPosition++;
                }
            }
            clearSelection();
        }

        void deleteChar() {
            if (hasSelection()) {
                deleteSelection();
            } else if (cursorPosition > 0 && !text.empty()) {
                text.erase(cursorPosition - 1, 1);
                cursorPosition--;
            }
        }

        void deleteForward() {
            if (hasSelection()) {
                deleteSelection();
            } else if (cursorPosition < text.length()) {
                text.erase(cursorPosition, 1);
            }
        }

        void moveCursorLeft(bool shiftHeld = false) {
            if (!shiftHeld && hasSelection()) {
                cursorPosition = std::min(selectionStart, selectionEnd);
                clearSelection();
            } else {
                if (cursorPosition > 0) cursorPosition--;
                if (shiftHeld) {
                    selectionEnd = cursorPosition;
                } else {
                    clearSelection();
                }
            }
        }

        void moveCursorRight(bool shiftHeld = false) {
            if (!shiftHeld && hasSelection()) {
                cursorPosition = std::max(selectionStart, selectionEnd);
                clearSelection();
            } else {
                if (cursorPosition < text.length()) cursorPosition++;
                if (shiftHeld) {
                    selectionEnd = cursorPosition;
                } else {
                    clearSelection();
                }
            }
        }

        void moveCursorToStart(bool shiftHeld = false) {
            cursorPosition = 0;
            if (shiftHeld) {
                selectionEnd = cursorPosition;
            } else {
                clearSelection();
            }
        }

        void moveCursorToEnd(bool shiftHeld = false) {
            cursorPosition = text.length();
            if (shiftHeld) {
                selectionEnd = cursorPosition;
            } else {
                clearSelection();
            }
        }

        void selectAll() {
            selectionStart = 0;
            selectionEnd = text.length();
            cursorPosition = text.length();
        }

        // Get cursor position from pixel offset (approximate for now)
        size_t getCursorFromPixelOffset(float pixelOffset) const {
            // Simple approximation - will be improved when we have access to TextRenderer
            float approxCharWidth = 10.0f;
            size_t pos = static_cast<size_t>(std::max(0.0f, pixelOffset / approxCharWidth + 0.5f));
            return std::min(pos, text.length());
        }

        glm::vec4 getCurrentBackgroundColor() const {
            return state == UIComponentState::Focused ? focusedColor : backgroundColor;
        }
    };

    struct UILabel : public UIComponent {
        std::string text { "Label" };
        glm::vec4 textColor { 1.0f, 1.0f, 1.0f, 1.0f };
        float fontSize { 16.0f };

        UILabel() {
            type = UIComponentType::Label;
        }
    };

    struct UICheckbox : public UIComponent {
        bool isChecked { false };
        std::string label { "Checkbox" };
        std::function<void(bool)> onToggle { nullptr };

        glm::vec4 boxColor { 0.2f, 0.2f, 0.2f, 1.0f };
        glm::vec4 checkColor { 0.3f, 0.7f, 0.3f, 1.0f };
        glm::vec4 textColor { 1.0f, 1.0f, 1.0f, 1.0f };

        float boxSize { 20.0f };
        float spacing { 8.0f };

        UICheckbox() {
            type = UIComponentType::Checkbox;
            size = glm::vec2(150.0f, 20.0f);
        }

        void toggle() {
            isChecked = !isChecked;
            if (onToggle) {
                onToggle(isChecked);
            }
        }
    };

    struct UISlider : public UIComponent {
        float value { 0.5f };
        float minValue { 0.0f };
        float maxValue { 1.0f };
        std::function<void(float)> onValueChanged { nullptr };

        glm::vec4 trackColor { 0.2f, 0.2f, 0.2f, 1.0f };
        glm::vec4 fillColor { 0.3f, 0.6f, 0.9f, 1.0f };
        glm::vec4 handleColor { 0.9f, 0.9f, 0.9f, 1.0f };

        float trackHeight { 4.0f };
        float handleRadius { 8.0f };

        UISlider() {
            type = UIComponentType::Slider;
            size = glm::vec2(200.0f, 20.0f);
        }

        void setValue(float newValue) {
            value = glm::clamp(newValue, minValue, maxValue);
            if (onValueChanged) {
                onValueChanged(value);
            }
        }

        float getNormalizedValue() const {
            return (value - minValue) / (maxValue - minValue);
        }
    };

} // namespace impgine
