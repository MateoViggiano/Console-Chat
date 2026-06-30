#pragma once
#include <viggiano>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <io.h>
#include <assert.h>
class deserializer_buffer_overflow :public std::exception {
	const char* what() const noexcept override {
		return "Deserializer buffer overflow";
	}
};
namespace mpv {
	class Database {
		Map<String, String> dict;
		std::filesystem::path path;
		std::fstream file;
		using WrappedStr = serialization::Wrapper<String, unsigned char>;

		serialization::Bytes serialize_dict() const{
			serialization::Bytes ret(serialization::to_bytes(dict.size()));
			for (const MapPair<String, String>& mp : dict) {
				ret += serialization::to_bytes(*reinterpret_cast<const MapPair<WrappedStr, WrappedStr>*>(&mp));
			}
			return ret;
		}

	public:
		Database(const std::filesystem::path& path):path(path),file(path,std::ios::in | std::ios::out | std::ios::app | std::ios::binary) {
			String str;
			file.clear();
			file.seekg(0);
			str.read(file);
			if (str.size() == 0) {
				puts("Creating new database file");
				file.close();
				file.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
				if (!file.is_open()) throw std::runtime_error("file couldn't be opened");
				file.write((char*)serialization::Serialized<size_t>(0).bytearray,sizeof(size_t));
			}
			else {
				serialization::Deserializer<deserializer_buffer_overflow> deserializer(str.c_str(), str.size());
				dict=deserializer.read<Map<WrappedStr, WrappedStr>>();
				std::cout << "read bytes:" << str.size() << std::endl;
				std::cout << "stored db:" << dict << std::endl;
				assert(deserializer.get_remaining_bytes() == 0);
			}
			file.flush();
			file.close();
			file.open(path, std::ios::out | std::ios::in | std::ios::ate | std::ios::binary);
		}
		const Map<String, String>& get_dict()const {
			return dict;
		}
		void insert(const String& key,const String& val) {
			dict[key] = val;
			serialization::Bytes vec = serialization::to_bytes(*reinterpret_cast<const WrappedStr*>(&key)) + serialization::to_bytes(*reinterpret_cast<const WrappedStr*>(&val));
			
			{
				serialization::Deserializer d(vec.get_array(), vec.size());
				std::cout << "inserted: " << d.read<MapPair<WrappedStr, WrappedStr>>() << std::endl;
			}

			file.write((const char*)vec.get_array(), vec.size());
			file.seekp(0);
			file.write(reinterpret_cast<const char*>(serialization::Serialized<size_t>(dict.size()).bytearray), sizeof(size_t));
			file.seekp(0,std::ios::end);
		}
		MapPair<String, String> pop(const String& key) {
			MapPair<String, String> popped = *dict.pop_elem(key);
			serialization::Bytes vec = serialize_dict();
			file.close();
			file.open(path, std::ios::out | std::ios::trunc | std::ios::binary);
			file.write((const char*)vec.get_array(), vec.size());
			file.close();
			file.open(path, std::ios::out | std::ios::in | std::ios::ate | std::ios::binary);
			return popped;
		}
	};
}
