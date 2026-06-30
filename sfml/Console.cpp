#pragma once
#include "Console.hpp"
Console::Console(sf::Vector2f size, sf::Vector2f pos, unsigned textsize)
	: background(size), historyBackground({ size.x, size.y * 0.95f }), textbar({}, { size.x, size.y * 0.05f }, font, "", textsize, sf::Color::Black, sf::Color(120, 120, 120), sf::Color::White), consoleSize(size), consolePos(pos), textsize(textsize), scrollOffset(0) {
	background.setPosition(pos);
	background.setFillColor(sf::Color(30, 30, 30));
	historyBackground.setPosition(pos);
	historyBackground.setFillColor(sf::Color(15, 15, 15));
	textbar.setPosition({ pos.x, pos.y + size.y * 0.95f });
	history.push_back("Terminal iniciado . . .");
}
sf::String Console::getInputText() const {
	return textbar.get_string();
}
void Console::clearInput() {
	textbar.clear();
}

void Console::printMessage(const sf::String& message) {
	history.push_back(message);
	if (history.size() > MAX_VISIBLE_LINES) {// Auto-scroll al final
		scrollOffset = history.size() - MAX_VISIBLE_LINES;
	}
}

sf::String Console::getAndClearInput() {
	sf::String text = getInputText();
	clearInput();
	return text;
}
bool Console::text_sent(const sf::Event& event)const {
	return textbar.is_selected() && event.is<sf::Event::KeyPressed>() && event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Enter;
}
void Console::update(const sf::Event& event) {
	textbar.process_ignoring_newline(event);
}
void Console::update_selected(const sf::RenderWindow& window) {
	textbar.update_selected(window);
}
void Console::draw(sf::RenderTarget& target, sf::RenderStates) const{
	target.draw(background);
	target.draw(historyBackground);

	size_t startLine = scrollOffset;
	size_t endLine = std::min(history.size(), startLine + MAX_VISIBLE_LINES);

	float lineHeight = consoleSize.y * 0.95f / MAX_VISIBLE_LINES;

	for (size_t i = startLine; i < endLine; ++i) {
		sf::Text historyText(font, history[i], textsize);
		historyText.setFillColor(sf::Color::White);
		historyText.setPosition({
			consolePos.x + 10,
			consolePos.y + (i - startLine) * lineHeight + 5
			});
		target.draw(historyText);
	}
	target.draw(textbar);
}