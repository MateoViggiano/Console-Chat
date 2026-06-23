#pragma once
#include "Context.hpp"
#include "State.hpp"
#include <Button.hpp>
#include <TextBox.hpp>
class LoginMenu :public State {
private:
	sf::Text title;
	sf::RectangleShape back;
	mpv::Vector<Button> buttons;
	mpv::Vector<TextBox> text_boxes;
	mpv::Vector<sf::Text> texts;
public:
	LoginMenu(Context&);
	void process_imput()override;
	void update()override;
	void draw()override;

};
