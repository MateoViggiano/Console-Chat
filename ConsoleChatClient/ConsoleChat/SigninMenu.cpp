#pragma once
#include "SigninMenu.hpp"

SigninMenu::SigninMenu(Context& context) : State(context), title(context.asset_manager.get_font(TITLE), "Sign in") {
	sf::Vector2f winSize = context.window.getView().getSize();
	back.setFillColor(sf::Color(0, 150, 0));
	back.setSize({ 600.f,winSize.y * 0.8f });
	back.setPosition({ winSize.x / 2.f - back.getSize().x / 2,winSize.y * 0.1f });
	title.setCharacterSize(40);
	title.setFillColor(sf::Color::Black);
	title.setPosition({ back.getPosition().x + back.getSize().x * 0.1f, winSize.y * 0.15f});

	// Etiqueta y TextBox para Usuario
	sf::Text user_label(context.asset_manager.get_font(BUTTON), "Username");
	user_label.setCharacterSize(16);
	user_label.setFillColor(sf::Color::White);
	user_label.setPosition({ back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.25f });
	texts.emplace_back(user_label);

	text_boxes.emplace_back(
		sf::Vector2f(back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.35f),
		sf::Vector2f(back.getSize().x * 0.8f, 40.f),
		context.asset_manager.get_font(BUTTON),
		"",
		20,
		sf::Color::White,
		sf::Color(120,120,120),
		sf::Color::Yellow
	);

	sf::Text password_label(context.asset_manager.get_font(BUTTON), "Password");
	password_label.setCharacterSize(16);
	password_label.setFillColor(sf::Color::White);
	password_label.setPosition({ back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.5f });
	texts.emplace_back(password_label);

	text_boxes.emplace_back(
		sf::Vector2f(back.getPosition().x + back.getSize().x * 0.1f, back.getPosition().y + back.getSize().y * 0.6f),
		sf::Vector2f(back.getSize().x * 0.8f, 40.f),
		context.asset_manager.get_font(BUTTON),
		"",
		20,
		sf::Color::White,
		sf::Color(120, 120, 120),
		sf::Color::Yellow
	);

	buttons.emplace_back(sf::Vector2f(winSize.x / 2.f - 150.f, back.getPosition().y + back.getSize().y * 0.85f), sf::Vector2f(300.f, 70.f), context.asset_manager.get_font(BUTTON), "submit", [this]() {
		this->context.username = text_boxes[0].get_string().toAnsiString();
		std::string password = text_boxes[1].get_string().toAnsiString();
		if (!is_valid_username(this->context.username)) puts("Invalid username");
		else if (password == "") puts("Invalid password");
		else this->context.add_user(password);
		
	});
	buttons.emplace_back(back.getPosition() + back.getSize() * 0.01f, sf::Vector2f(80.f, 30.f), context.asset_manager.get_font(BUTTON), "<", [&context]() {
		context.state_manager.pop();
	});

}
void SigninMenu::process_imput() {
	while (const std::optional event = context.window.pollEvent()) {
		context.process_window_event(*event);
		for (TextBox& tb : text_boxes) {
			tb.update_selected(context.window);
			tb.process_key(*event);
		}
		if (!text_boxes.any([](const TextBox& tb) {return tb.is_selected();}) && event->is<sf::Event::KeyPressed>()) {
			if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape) {
				context.state_manager.pop();
			}
		}
	}
	for(Button& button:buttons) button.update(context.window);
}
void SigninMenu::update(){}
void SigninMenu::draw(){
	context.window.clear(sf::Color::Black);
	context.window.draw(back);
	context.window.draw(title);
	for (const sf::Text& text : texts)
		context.window.draw(text);
	for (const TextBox& tb : text_boxes)
		context.window.draw(tb);
	for (const Button& button : buttons)
		context.window.draw(button);
	context.window.display();
}