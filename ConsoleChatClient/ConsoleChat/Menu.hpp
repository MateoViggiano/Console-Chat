#pragma once
#include "Context.hpp"
#include "State.hpp"
#include <Button.hpp>
class Menu : public State {
public:
	Menu(Context&);
	void process_imput()override;
	void update()override;
	void draw()override;

private:
	sf::Text title;
	sf::RectangleShape back;
	mpv::Vector<Button> buttons;
};
