#pragma once
#include "Context.hpp"
#include "State.hpp"
#include <Button.hpp>
#include <TextBox.hpp>
class UserMenu :public State {
private:
	sf::Text title;
	sf::RectangleShape back;
	mpv::Vector<Button> buttons;
	mpv::Vector<TextBox> text_boxes;
	mpv::Vector<sf::Text> texts;
public:
	UserMenu(Context&);
	void process_imput()override;
	void update()override;
	void draw()override;

};
