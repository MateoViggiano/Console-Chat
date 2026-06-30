#pragma once
#include <viggiano>
namespace mpv {
	template<typename K1,typename K2>
	class BiMap {
		mpv::Map<K1, K2> lmap;
		mpv::Map<K2, K1> rmap;
	public:
		bool insert(const K1& k1, const K2& k2) {
			if (lmap.contains(k1) || rmap.contains(k2)) return false;
			lmap.insert({ k1, k2 });
			rmap.insert({ k2, k1 });
			return true;
		}
		const Map<K1, K2>& left()const noexcept{
			return lmap;
		}
		const Map<K2, K1>& right()const noexcept {
			return rmap;
		}
		bool erase_by_left(const K1& k1) {
			if (lmap.contains(k1)) {
				rmap.pop_at(*lmap.pop_at(k1));
				return true;
			}
			else return false;
		}
		bool erase_by_right(const K2& k2) {
			if (rmap.contains(k2)) {
				lmap.pop_at(*rmap.pop_at(k2));
				return true;
			}
			else return false;
		}
	};
}