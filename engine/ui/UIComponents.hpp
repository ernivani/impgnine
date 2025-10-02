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
        Slider,
        TreeNode
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
        std::function<void(const std::string&)> onCommit { nullptr };
        size_t cursorPosition { 0 };
        size_t selectionStart { 0 };
        size_t selectionEnd { 0 };
        size_t maxLength { 256 };
        size_t scrollStart { 0 }; // first visible character index for horizontal scroll

        glm::vec4 backgroundColor { 0.25f, 0.25f, 0.3f, 1.0f };
        glm::vec4 focusedColor { 0.3f, 0.3f, 0.4f, 1.0f };
        glm::vec4 hoverColor { 0.35f, 0.35f, 0.45f, 1.0f };
        glm::vec4 textColor { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 placeholderColor { 0.6f, 0.6f, 0.6f, 1.0f };
        glm::vec4 cursorColor { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 selectionColor { 0.4f, 0.6f, 0.9f, 0.5f };
        glm::vec4 borderColor { 0.15f, 0.15f, 0.2f, 1.0f };
        glm::vec4 borderHoverColor { 0.35f, 0.55f, 0.9f, 1.0f };
        glm::vec4 borderFocusedColor { 0.3f, 0.6f, 1.0f, 1.0f };

        float padding { 8.0f };
        float borderWidth { 2.0f };

        UIInputField() {
            type = UIComponentType::InputField;
        }

        inline void notifyCommit() {
            if (onCommit) {
                onCommit(text.empty() ? std::string("0") : text);
            }
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
            notifyCommit();
        }

        void insertChar(char c) {
            if (hasSelection()) {
                deleteSelection();
            }
            if (text.length() < maxLength && cursorPosition <= text.length()) {
                text.insert(cursorPosition, 1, c);
                cursorPosition++;
                clearSelection();
                notifyCommit();
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
            notifyCommit();
        }

        void deleteChar() {
            if (hasSelection()) {
                deleteSelection();
            } else if (cursorPosition > 0 && !text.empty()) {
                text.erase(cursorPosition - 1, 1);
                cursorPosition--;
                notifyCommit();
            }
        }

        void deleteForward() {
            if (hasSelection()) {
                deleteSelection();
            } else if (cursorPosition < text.length()) {
                text.erase(cursorPosition, 1);
                notifyCommit();
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
            if (state == UIComponentState::Focused) return focusedColor;
            if (state == UIComponentState::Hovered) return hoverColor;
            return backgroundColor;
        }
    };

    struct UILabel : public UIComponent {
        std::string text { "Label" };
        glm::vec4 textColor { 1.0f, 1.0f, 1.0f, 1.0f };
        float fontSize { 16.0f };

        UILabel() {
            type = UIComponentType::Label;
            isEnabled = false;  // Labels don't need hover/click interaction
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

    struct UITreeNode : public UIComponent {
        std::string label { "TreeNode" };
        uint32_t entityId { 0 };
        int depth { 0 };
        bool isExpanded { true };
        bool isSelected { false };
        bool isDragging { false };
        bool isDropTarget { false };
        bool hasChildren { false };

        std::function<void(uint32_t)> onClick { nullptr };
        std::function<void(uint32_t, uint32_t)> onDrop { nullptr };  // (draggedEntity, targetEntity)
        std::function<void()> onToggleExpand { nullptr };

        glm::vec4 normalColor { 0.22f, 0.22f, 0.22f, 1.0f };
        glm::vec4 hoverColor { 0.28f, 0.28f, 0.28f, 1.0f };
        glm::vec4 selectedColor { 0.17f, 0.36f, 0.53f, 1.0f };  // Unity blue
        glm::vec4 selectedHoverColor { 0.19f, 0.39f, 0.58f, 1.0f };
        glm::vec4 dropTargetColor { 0.30f, 0.50f, 0.30f, 1.0f };
        glm::vec4 textColor { 0.85f, 0.85f, 0.85f, 1.0f };
        glm::vec4 selectedTextColor { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 arrowColor { 0.6f, 0.6f, 0.6f, 1.0f };

        float indentSize { 14.0f };
        float arrowSize { 8.0f };
        float iconSize { 14.0f };

        UITreeNode() {
            type = UIComponentType::TreeNode;
            size = glm::vec2(200.0f, 18.0f);
        }

        glm::vec4 getCurrentColor() const {
            if (isDropTarget) return dropTargetColor;
            if (isSelected && state == UIComponentState::Hovered) return selectedHoverColor;
            if (isSelected) return selectedColor;
            if (state == UIComponentState::Hovered) return hoverColor;
            return normalColor;
        }

        glm::vec4 getCurrentTextColor() const {
            return isSelected ? selectedTextColor : textColor;
        }

        float getIndentOffset() const {
            return depth * indentSize;
        }

        bool isPointInArrow(const glm::vec2& point) const {
            float arrowX = position.x + getIndentOffset();
            return point.x >= arrowX && point.x <= arrowX + arrowSize + 4.0f &&
                   point.y >= position.y && point.y <= position.y + size.y;
        }
    };

} // namespace impgine
