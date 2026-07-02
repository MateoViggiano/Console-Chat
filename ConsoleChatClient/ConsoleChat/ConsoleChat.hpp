#pragma once
#include "Context.hpp"
#include "State.hpp"
#include "ImGuiConsole.hpp"
class ConsoleChat :public State {
private:
	ImGuiConsole console;
	sf::Clock dt;
public:
	ConsoleChat(Context&);
	void process_imput()override;
	void update()override;
	void draw()override;

};
