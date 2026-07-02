#include "Button.hpp"
#include "TextBox.hpp"
#include <viggiano.hpp>

class Console : public sf::Drawable{
	mpv::Vector<sf::String> history;
	unsigned textsize;
	sf::Font font = sf::Font(L"C:/Windows/Fonts/CascadiaCode.ttf");
	sf::RectangleShape background;
	sf::RectangleShape historyBackground;
	TextBox textbar;
	sf::Vector2f consoleSize;
	sf::Vector2f consolePos;
	static constexpr unsigned MAX_VISIBLE_LINES = 30;
	size_t scrollOffset;

public:
	Console(sf::Vector2f size, sf::Vector2f pos = sf::Vector2f(0, 0), unsigned textsize = 18);

	sf::String getInputText() const;

	void clearInput();
	void printMessage(const sf::String& message);

	sf::String getAndClearInput();
	bool text_sent(const sf::Event& event) const;
	// Actualiza la consola (maneja eventos de entrada)
	void update(const sf::Event& event);
	void update_selected(const sf::RenderWindow& window);
	virtual void draw(sf::RenderTarget& target, sf::RenderStates) const override;
};