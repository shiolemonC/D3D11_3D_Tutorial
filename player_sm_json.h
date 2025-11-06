#pragma once
#include <string>
#include <vector>
#include <unordered_map>


namespace smjson {


	struct Value {
		enum class Type { Null, Bool, Number, String, Array, Object };
		Type type = Type::Null;
		bool b = false;
		double num = 0.0;
		std::string str;
		std::vector<Value> arr;
		std::unordered_map<std::string, Value> obj;


		bool isNull() const { return type == Type::Null; }
		bool isBool() const { return type == Type::Bool; }
		bool isNumber() const { return type == Type::Number; }
		bool isString() const { return type == Type::String; }
		bool isArray() const { return type == Type::Array; }
		bool isObject() const { return type == Type::Object; }


		// helpers
		bool getBool(bool def = false) const { return isBool() ? b : def; }
		double getNumber(double def = 0.0) const { return isNumber() ? num : def; }
		std::string getString(const std::string& def = "") const { return isString() ? str : def; }


		const Value* find(const char* key) const {
			if (!isObject()) return nullptr;
			auto it = obj.find(std::string(key));
			return (it == obj.end()) ? nullptr : &it->second;
		}
	};


	// Parse a UTF‑8 JSON text into Value. Return true on success; on failure set err.
	bool ParseText(const std::string& utf8, Value& out, std::string* err);


	// Load file (UTF‑8, optional BOM) and parse.
	bool ParseFileUTF8(const wchar_t* path, Value& out, std::string* err);


} // namespace smjson