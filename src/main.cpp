#include <iostream>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <map>
#include <unordered_set>
#include <optional>
#include <array>
#include <cstdlib>
#include <format>
#include <charconv>
#include <vector>

namespace fs = std::filesystem;

fs::path GetDataRoot();

struct AppConfig {
	fs::path baseDir;
	fs::path rawDir;
	fs::path blacklistDir;
	fs::path outDir;
	AppConfig() {
		baseDir = GetDataRoot();
		rawDir = baseDir / L"data" / L"raw";
		blacklistDir = baseDir / L"data" / L"blacklist";
		outDir = baseDir;
	}
};
struct WordLists {
	std::unordered_set<std::string> bad;
	std::unordered_set<std::string> malformed;
	std::unordered_set<std::string> english;
};

template <typename T>
void GetNumberFromStringView(T& num, std::string_view view) {
	std::from_chars(view.data(), view.data() + view.size(), num);
}
fs::path GetDataRoot() {
	if (const char* envPath = std::getenv("WORDLIST_DATA_ROOT"))
		return fs::path(envPath);

	return fs::current_path();
}
static std::optional<std::ifstream> OpenFileFromPath(const fs::path& filePath) {
	const std::string fileName = filePath.filename().string();

	if (!fs::exists(filePath)) {
		std::cout << std::format("[ERROR] File \"{}\" does not exists.\n", fileName);
		return std::nullopt;
	}

	std::ifstream ifstream(filePath);
	if (!ifstream.is_open()) {
		std::cout << std::format("[ERROR] Failed to open file \"{}\".\n", fileName);
		return std::nullopt;
	}

	return ifstream;
}
static std::optional<std::unordered_set<std::string>> GetWordListFromTxt(const fs::path& wordListTxt) {
	std::unordered_set<std::string> wordList;
	std::optional<std::ifstream> ifstream = OpenFileFromPath(wordListTxt);
	if (!ifstream)
		return std::nullopt;

	std::string word;
	while (std::getline(*ifstream, word))
		wordList.insert(word);
	return wordList;
}
std::vector<std::string_view> SeparateByDelimeter(std::string_view inputStr, std::string_view delimeter) {
	std::vector<std::string_view> outputStrs;

	size_t currentOffset = 0;
	while (currentOffset <= inputStr.length()) {
		size_t foundOffset = inputStr.find(delimeter, currentOffset);

		if (foundOffset == std::string_view::npos) {
			outputStrs.push_back(inputStr.substr(currentOffset));
			break;
		}
		outputStrs.push_back(inputStr.substr(currentOffset, foundOffset - currentOffset));

		currentOffset = foundOffset + delimeter.length();
	}

	return outputStrs;
}
std::optional<WordLists> LoadWordLists(const AppConfig& config) {
	WordLists outWordLists;

	const std::vector<std::pair<fs::path, std::reference_wrapper<std::unordered_set<std::string>>>> wordListTextFiles = {
		{config.blacklistDir / "bad_words.txt", outWordLists.bad},
		{config.blacklistDir / "malformed_words.txt", outWordLists.malformed},
		{config.rawDir / "csw24.txt", outWordLists.english}
	};
	for (const auto& [filePath, wordListSetRef] : wordListTextFiles) {
		std::optional<std::unordered_set<std::string>> loadedWords = GetWordListFromTxt(filePath);

		if (!loadedWords)
			return std::nullopt;

		auto& wordListSet = wordListSetRef.get();
		wordListSet.insert(
			std::make_move_iterator(loadedWords->begin()),
			std::make_move_iterator(loadedWords->end())
		);
	}
	return outWordLists;
}
std::optional<std::ifstream> OpenNgramStream(const AppConfig& config) {
	fs::path ngramCsvPath = config.rawDir / "ngram_freq_dict.csv";

	std::optional<std::ifstream> ngramCsvStream = OpenFileFromPath(ngramCsvPath);
	if (!ngramCsvStream)
		return std::nullopt;

	return ngramCsvStream;
}
bool create() {
	const AppConfig config;
	std::optional<WordLists> wordLists = LoadWordLists(config);
	if (!wordLists)
		return false;
	std::optional<std::ifstream> ngramStream = OpenNgramStream(config);
	if (!ngramStream)
		return false;
	std::map<std::string, double> wordsZipf;

	const std::string_view delimeter = ",";
	std::string record;
	std::getline(*ngramStream, record); // Consume csv header
	while (std::getline(*ngramStream, record)) {
		std::string_view view(record);
		const auto data = SeparateByDelimeter(view, delimeter);
		const std::string word = std::string(data[0]);
		long long int frequency = 0;
		GetNumberFromStringView(frequency, data[1]);

		if (word.length() < 3 || word.length() > 8)
			continue;
		if (word.length() == 3 && frequency < 1000000)
			continue;
		if (word.length() > 3 && frequency < 1000)
			continue;

		if (wordLists->bad.find(word) != wordLists->bad.end())
			continue;
		if (wordLists->english.find(word) == wordLists->english.end())
			continue;
		if (wordLists->malformed.find(word) != wordLists->malformed.end())
			continue;
		

		wordsZipf[word] = frequency;
	}
	fs::create_directories(config.outDir);
	fs::path outputJsonPath = config.outDir / "word_list.json";
	std::ofstream ofstream(outputJsonPath);
	if (!ofstream.is_open()) {
		std::cout << "[ERROR] Failed to open writing file." << std::endl;
		return 1;
	}
	ofstream << "{\n";
	long long int wordIndex = 0;
	for (const auto& [word, zipfValue] : wordsZipf) {
		if (wordIndex == wordsZipf.size() - 1)
			ofstream << std::format("\t\"{}\": {}\n", word, zipfValue);
		else
			ofstream << std::format("\t\"{}\": {},\n", word, zipfValue);
		wordIndex++;
	}
	ofstream << "}";
	return 0;
}


int main() {
	create();
}