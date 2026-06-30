#pragma once
#include "ConsoleChat.hpp"

ConsoleChat::ConsoleChat(Context& context) : State(context) {
	console.Print("console started . . .");
}
void ConsoleChat::process_imput() {
	while (const std::optional event = context.window.pollEvent()) {
		ImGui::SFML::ProcessEvent(context.window, *event);
		context.process_window_event(*event);
	}
	while (const mpv::Optional<std::string> msg = context.poll_message()) {
		console.Print(*msg);
	}
}
void ConsoleChat::update(){
	ImGui::SFML::Update(context.window, dt.restart());
	console.Draw();
	if (auto msg = console.PollMessage()) {
		if (!is_valid_username(msg->targetUser) && msg->targetUser.size() != 0) puts("Invalid target user");
		else switch (msg->type) {
		case ImGuiConsole::MessageType::Message:
			if (!msg->targetUser.empty()) console.Print("to <" + msg->targetUser + ">: " + msg->text);
			context.send_message(msg->targetUser, msg->text);
			break;
		case ImGuiConsole::MessageType::Command:
			context.send_command(msg->targetUser, msg->text);
			break;
		case ImGuiConsole::MessageType::File:
			if (msg->targetUser.empty()) puts("Must enter a user to send the file");
			else context.send_file_header(msg->targetUser, msg->text);
			break;
		}
	}
}
void ConsoleChat::draw(){
	context.window.clear(sf::Color(30, 30, 30));
	ImGui::SFML::Render(context.window);
	context.window.display();
}