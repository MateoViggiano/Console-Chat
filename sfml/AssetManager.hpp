#pragma once
#include "viggiano.hpp"
#include <SFML/Graphics/Font.hpp>
class AssetManager {
private:
	mpv::Map<int, mpv::uPtr<sf::Font>> fonts;
public:
	void add_font(int id, const char* path) {
		mpv::uPtr<sf::Font> font(new sf::Font);
		if (font->openFromFile(path)) {
			fonts[id] = mpv::move(font);
		}
		else {
			puts("abort() will be called");
			abort();
		}
	}
	const sf::Font& get_font(int id)const {
		return *fonts.at(id);
	}
};
enum FontID{BUTTON,TEXTBOX,TITLE};