#pragma once
class Context;
class State {
public:
	Context& context;
	State(Context& context):context(context) {}
	virtual ~State() {}
	virtual void process_imput() = 0;
	virtual void update() = 0;
	virtual void draw() = 0;
	virtual void enter() {}
	virtual void exit() {}

};
