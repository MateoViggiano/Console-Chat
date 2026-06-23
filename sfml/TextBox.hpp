#pragma once
#include <SFML/Graphics.hpp>

class TextBox : public sf::Drawable {
private:
    sf::RectangleShape shape;
    sf::Text text;
    sf::String str;
    bool selected = false;
    bool was_mouse_pressed = false;
    sf::Color unselected_outline_color, selected_outline_color;
public:
    TextBox(sf::Vector2f position, sf::Vector2f size, const sf::Font& font, const sf::String& default_text, unsigned int text_size, sf::Color shape_color, sf::Color unselected_outline_color, sf::Color selected_outline_color, sf::Color text_color = sf::Color::Black);
    void update_selected(const sf::RenderWindow&);
    void process_ignoring_newline(sf::Event input);
    void process_key(sf::Event input);
    void process_digit(sf::Event input);
    virtual void draw(sf::RenderTarget&,sf::RenderStates) const override;
    bool is_selected()const;
    void select();
    void deselect();
    const sf::String& get_string()const;
    void setPosition(sf::Vector2f position);
    void clear();
};