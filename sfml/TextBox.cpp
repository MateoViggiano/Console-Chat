#pragma once
#include "TextBox.hpp"
#define ESCAPE_KEY char(27)
TextBox::TextBox(sf::Vector2f position, sf::Vector2f size,const sf::Font& font, const sf::String& default_text,unsigned int text_size,sf::Color shape_color,sf::Color unselected_outline_color,sf::Color selected_outline_color,sf::Color text_color) : shape(size), text(font,default_text,text_size),unselected_outline_color(unselected_outline_color), selected_outline_color(selected_outline_color) {
	shape.setPosition(position);
	shape.setFillColor(shape_color);
	shape.setOutlineThickness(2.f);
	shape.setOutlineColor(unselected_outline_color);
	str = default_text;
	text.setString(str);
	text.setPosition({position.x,position.y + size.y / 2 - text.getGlobalBounds().size.y / 2});
	text.setFillColor(text_color);
}
void TextBox::setPosition(sf::Vector2f position) {
	shape.setPosition(position);
	text.setPosition({ position.x,position.y + shape.getSize().y / 2 - text.getGlobalBounds().size.y / 2});
}
void TextBox::update_selected(const sf::RenderWindow& window) { // Updates mouse selection
    bool mouse_pressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool hovered = shape.getGlobalBounds().contains(window.mapPixelToCoords(sf::Mouse::getPosition(window)));
	if (mouse_pressed && !was_mouse_pressed) {
		selected = hovered;
	}
	shape.setOutlineColor(selected ? selected_outline_color : unselected_outline_color);
}
void TextBox::select() {
	selected = true;
	shape.setOutlineColor(selected_outline_color);
}
void TextBox::deselect() {
	selected = false;
	shape.setOutlineColor(unselected_outline_color);
}
void TextBox::process_ignoring_newline(sf::Event input) {
	if (selected && input.is<sf::Event::TextEntered>()) {
		char32_t key = input.getIf<sf::Event::TextEntered>()->unicode;
		switch (key) {
		case '\b':
			if (!str.isEmpty()) {
				str.erase(str.getSize() - 1, str.getSize());
				text.setString(str);
			}
			break;
		case '\n':
		case '\r':break;
		case ESCAPE_KEY:
			selected = false;
			break;
		default:
			str += key;
			text.setString(str);
		}
	}
}
void TextBox::process_key(sf::Event input) {
	if (selected && input.is<sf::Event::TextEntered>()) {
		char32_t key = input.getIf<sf::Event::TextEntered>()->unicode;
		switch (key) {
		case '\b':
			if (!str.isEmpty()) {
				str.erase(str.getSize() - 1, str.getSize());
				text.setString(str);
			}
			break;
		case '\n':
		case '\r':
		case ESCAPE_KEY:
			selected = false;
			break;
		default:
			str += key;
			text.setString(str);
		}
	}
}
void TextBox::process_digit(sf::Event input) {
	if (selected && input.is<sf::Event::TextEntered>()) {
		char32_t key = input.getIf<sf::Event::TextEntered>()->unicode;
		switch (key) {
		case '\b':
			if (!str.isEmpty()) {
				str.erase(str.getSize() - 1, str.getSize());
				text.setString(str);
			}
			break;
		case '\n':
		case '\r':
		case ESCAPE_KEY:
			selected = false;
			break;
		default:
			if (std::isdigit(key)) {
				str += key;
				text.setString(str);
			}
		}
	}
}
void TextBox::clear() {
	str.clear();
	text.setString(str);
}
bool TextBox::is_selected()const {
	return selected;
}
const sf::String& TextBox::get_string()const {
	return str;
}

void TextBox::draw(sf::RenderTarget& target, sf::RenderStates) const {
	target.draw(shape);
	target.draw(text);
}