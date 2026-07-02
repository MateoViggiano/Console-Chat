#pragma once
#include "Menu.hpp"
#include "LoginMenu.hpp"
#include "SigninMenu.hpp"
Menu::Menu(Context& context) : State(context), title(context.asset_manager.get_font(TITLE), "Chat") {
	title.setCharacterSize(96);
	title.setFillColor(sf::Color::Black);
	sf::Vector2f winSize = context.window.getView().getSize();
	title.setPosition({ winSize.x / 2.f - 180.f, winSize.y * 0.2f });
	back.setFillColor(sf::Color(0,150,0));
	back.setSize({ 600.f,winSize.y * 0.8f});
	back.setPosition({winSize.x / 2.f - back.getSize().x / 2,winSize.y * 0.1f});
	
	buttons.emplace_back(sf::Vector2f(winSize.x / 2.f - 150.f, winSize.y * 0.35f), sf::Vector2f(300.f, 70.f), context.asset_manager.get_font(BUTTON), "Log in",[&context]() {
		context.state_manager.push(new LoginMenu(context));
	});
	buttons.emplace_back(sf::Vector2f(winSize.x / 2.f - 150.f, winSize.y * 0.45f), sf::Vector2f(300.f, 70.f), context.asset_manager.get_font(BUTTON), "Sign in", [&context]() {
		context.state_manager.push(new SigninMenu(context));
	});
	buttons.emplace_back(sf::Vector2f(winSize.x / 2.f - 150.f, winSize.y * 0.55f), sf::Vector2f(300.f, 70.f), context.asset_manager.get_font(BUTTON), "Exit", [&context]() {
		context.window.close();
	});
}

void Menu::process_imput() {
	while (const std::optional event = context.window.pollEvent()) {
		context.process_window_event(*event);
	}
	for (Button& button : buttons) {
		button.update(context.window);
	}
}

void Menu::update() {}

void Menu::draw() {
	context.window.clear(sf::Color::Black);
	context.window.draw(back);
	context.window.draw(title);
	for (const Button& button : buttons)
		context.window.draw(button);
	context.window.display();
}

