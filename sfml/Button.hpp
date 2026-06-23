#pragma once
#include <SFML/Graphics.hpp>
#include <functional>

class Button : public sf::Drawable{
private:
    sf::RectangleShape shape;
    sf::Text text;

    sf::Color normal_color;
    sf::Color hover_color;
    sf::Color click_color;

    bool hovered = false;
    bool clicked = false;
    bool was_mouse_pressed = false;
    std::function<void()> callback;

    void centerText();
public:
    Button(sf::Vector2f, sf::Vector2f, const sf::Font&, const sf::String&, std::function<void()>);
    Button(sf::Vector2f, sf::Vector2f, const sf::Font&, const sf::String&, unsigned int, std::function<void()>);
    Button(sf::Vector2f, sf::Vector2f, const sf::Font&, const sf::String&, sf::Color, sf::Color, sf::Color, unsigned int, sf::Color, std::function<void()>);

    void update(const sf::RenderWindow&);
    virtual void draw(sf::RenderTarget&, sf::RenderStates)const override;

    bool wasClicked() const;
    bool isHovered() const;

    void setText(const sf::String&);
    void setPosition(sf::Vector2f);


};