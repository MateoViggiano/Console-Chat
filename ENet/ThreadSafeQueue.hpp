#pragma once
#include <mutex>
#include <condition_variable>
#include <viggiano>

namespace mpv {
	template<typename T,typename Alloc=allocator<T>>
	class tsQueue {
		mpv::Queue<T, Alloc> queue;
		std::mutex mtx;
		std::condition_variable cv;
		bool closed=false;
		using Queue = mpv::Queue<T, Alloc>;
	public:
		using value_type = typename Queue::value_type;
		using pointer = typename Queue::pointer;
		using const_pointer = typename Queue::const_pointer;
		using reference = typename Queue::reference;
		using const_reference = typename Queue::const_reference;
		tsQueue(const tsQueue&) = delete;
		tsQueue& operator=(const tsQueue&) = delete;
		tsQueue() = default;
		template<typename... Args>
		void emplace(Args&&... args) {
			{
				std::lock_guard<std::mutex> lock(mtx);
				queue.emplace(static_cast<Args&&>(args)...);
			}
			cv.notify_one();
		}
		void push(const_reference val) {
			emplace(val);
		}
		void push(value_type&& val) {
			emplace(static_cast<value_type&&>(val));
		}
		bool wait_pop(mpv::Optional<value_type>& out) {
			std::unique_lock<std::mutex> lock(mtx);
			cv.wait(lock, [this]() {return !queue.empty() || closed;});
			if (queue.empty()) {
				out.reset();
				return false;
			}
			out = static_cast<value_type&&>(queue.front());
			queue.del();
			return true;
		}
		bool try_pop(mpv::Optional<value_type>& out) {
			std::lock_guard<std::mutex> lock(mtx);
			if (queue.empty()) {
				out.reset();
				return false;
			}
			out = static_cast<value_type&&>(queue.front());
			queue.del();
			return true;
		}
		void close() {
			{
				std::lock_guard<std::mutex> lock(mtx);
				closed = true;
			}
			cv.notify_all();
		}
	};
}