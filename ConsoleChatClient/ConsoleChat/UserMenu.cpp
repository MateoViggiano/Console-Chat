#pragma once
#include "UserMenu.hpp"
#include "ConsoleChat.hpp"
UserMenu::UserMenu(Context& context) : State(context), title(context.asset_manager.get_font(TITLE), "User Menu") {
	sf::Vector2f winSize = context.window.getView().getSize();
	back.setFillColor(sf::Color(0, 150, 0));
	back.setSize({ 600.f,winSize.y * 0.8f });
	back.setPosition({ winSize.x / 2.f - back.getSize().x / 2,winSize.y * 0.1f });
	title.setCharacterSize(40);
	title.setFillColor(sf::Color::Black);
	title.setPosition({ back.getPosition().x + back.getSize().x * 0.1f, winSize.y * 0.15f});

	sf::Text user_text(context.asset_manager.get_font(BUTTON), "User: " + context.username);
	user_text.setCharacterSize(18);
	user_text.setFillColor(sf::Color::White);
	user_text.setPosition({ back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.15f });
	texts.emplace_back(user_text);

	buttons.emplace_back(
		sf::Vector2f(back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.35f),
		sf::Vector2f(back.getSize().x * 0.8f, 50.f),
		context.asset_manager.get_font(BUTTON),
		"Start chat",
		20,
		[&context]() { context.state_manager.push(new ConsoleChat(context)); }
	);

	buttons.emplace_back(
		sf::Vector2f(back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.55f),
		sf::Vector2f(back.getSize().x * 0.8f, 50.f),
		context.asset_manager.get_font(BUTTON),
		"Log out",
		20,
		[&context]() { context.log_out_request(); }
	);

	buttons.emplace_back(
		sf::Vector2f(back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.75f),
		sf::Vector2f(back.getSize().x * 0.8f, 50.f),
		context.asset_manager.get_font(BUTTON),
		"Delete user",
		20,
		[&context]() { context.delete_user_request(); }
	);

	buttons.emplace_back(back.getPosition() + back.getSize() * 0.01f, sf::Vector2f(80.f, 30.f), context.asset_manager.get_font(BUTTON), "<", [&context]() {
		context.state_manager.pop();
	});

}
void UserMenu::process_imput() {
	while (const std::optional event = context.window.pollEvent()) {
		context.process_window_event(*event);
	}
	for(Button& button:buttons) button.update(context.window);
}
void UserMenu::update(){}
void UserMenu::draw(){
	context.window.clear(sf::Color::Black);
	context.window.draw(back);
	context.window.draw(title);
	for (const sf::Text& text : texts)
		context.window.draw(text);
	for (const Button& button : buttons)
		context.window.draw(button);
	context.window.display();
}