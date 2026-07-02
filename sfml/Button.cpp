#include "Button.hpp"

Button::Button(sf::Vector2f position, sf::Vector2f size, const sf::Font& font, const sf::String& str, std::function<void()> onClick)
    : shape(size), text(font, str, 28), normal_color(sf::Color(80, 80, 80)), hover_color(sf::Color(120, 120, 120)), click_color(sf::Color(160, 160, 160)), callback(std::move(onClick)) {
    shape.setPosition(position);
    shape.setFillColor(normal_color);
    text.setFillColor(sf::Color::White);
    centerText();
}
Button::Button(sf::Vector2f position, sf::Vector2f size, const sf::Font& font, const sf::String& str, unsigned int text_size, std::function<void()> onClick)
    : shape(size), text(font, str, text_size), normal_color(sf::Color(80, 80, 80)), hover_color(sf::Color(120, 120, 120)), click_color(sf::Color(160, 160, 160)), callback(std::move(onClick)) {
    shape.setPosition(position);
    shape.setFillColor(normal_color);
    text.setFillColor(sf::Color::White);
    centerText();
}
Button::Button(sf::Vector2f position, sf::Vector2f size, const sf::Font& font, const sf::String& str, sf::Color normal, sf::Color hover, sf::Color click, unsigned int text_size, sf::Color text_color, std::function<void()> onClick)
    : shape(size), text(font, str, text_size), normal_color(normal), hover_color(hover), click_color(click), callback(std::move(onClick)) {
    shape.setPosition(position);
    shape.setFillColor(normal_color);
    text.setFillColor(text_color);
    centerText();
}
void Button::update(const sf::RenderWindow& window) {
    bool mouse_pressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    hovered = shape.getGlobalBounds().contains(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
    if (hovered && !mouse_pressed && clicked) {
        was_mouse_pressed = hovered = clicked = false;
        callback();
    }
    else if (!hovered && !mouse_pressed && clicked) {
        clicked = false;
        shape.setFillColor(normal_color);
    }
    else if (hovered && mouse_pressed && !was_mouse_pressed){
        clicked = true;
    }
    if (clicked) {
        shape.setFillColor(click_color);
    }
    else if (hovered && !clicked) {
        shape.setFillColor(hover_color);
    }
    else {//!hovered && !clicked
        shape.setFillColor(normal_color);
    }
    was_mouse_pressed = mouse_pressed;
}
void Button::draw(sf::RenderTarget& target,sf::RenderStates) const{
    target.draw(shape);
    target.draw(text);
}

bool Button::wasClicked() const{
    return clicked;
}

bool Button::isHovered() const{
    return hovered;
}

void Button::setText(const sf::String& str) {
    text.setString(str);
    centerText();
}

void Button::setPosition(sf::Vector2f position) {
    shape.setPosition(position);
    centerText();
}

void Button::centerText() {
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setOrigin({ textBounds.position.x + textBounds.size.x / 2.f, textBounds.position.y + textBounds.size.y / 2.f });
    sf::Vector2f buttonPos = shape.getPosition();
    sf::Vector2f buttonSize = shape.getSize();
    text.setPosition({ buttonPos.x + buttonSize.x / 2.f, buttonPos.y + buttonSize.y / 2.f });
}