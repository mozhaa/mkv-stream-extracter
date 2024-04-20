#include <boost/regex.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <ncurses.h>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace std::chrono;

// Source: https://gist.github.com/meritozh/f0351894a2a4aa92871746bf45879157
std::string exec(const char* cmd) {
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe)
		throw runtime_error("popen() failed!");
	char buffer[128];
	std::string result = "";
	while (!feof(pipe.get())) {
		if (fgets(buffer, 128, pipe.get()) != NULL)
			result += buffer;
	}
	return result;
}

struct container_element {
	string stream;
	string lang;
	string type;
	string ext;
	string name;
	string fname;

	bool selected;
};

static struct container_list {
	vector<container_element> options = {};
	int rel_start = 0;
	int rel_cursor = 0;
} menu;

static string fname;

static int extract_info(const string& s) {
	const char* rstr =
		R"(^\s*Stream\s*#([0-9:]+)(\(.*?\))?:\s*(\w+):\s*(\S*).*?(?:(?:\n(?! {0,2}\S)[^\n]*)*title\s*:\s*(.*?))?(?:(?:\n(?! {0,2}\S)[^\n]*)*filename\s*:\s*(.*?))?$)";
	const boost::regex r(rstr);

	steady_clock::time_point begin = steady_clock::now();

	const vector<boost::smatch> matches = {
		boost::sregex_iterator{cbegin(s), cend(s), r},
		boost::sregex_iterator{}};


	menu.options.reserve(matches.size());
	for (auto& m : matches) {
		menu.options.push_back(
			container_element{m[1], m[2], m[3], m[4], m[5], m[6],
							  m[3] != "Video" && m[3] != "Audio"});
	}

	return duration_cast<microseconds>(steady_clock::now() - begin).count();
}

static void run_interactive() {}

static string replace_ext(string f, string ext, string add = "") {
	const boost::regex r(R"(^(.*)\.(.*)$)");
	boost::smatch m;
	if (boost::regex_match(f, m, r)) {
		return m[1] + add + "." + ext;
	} else {
		return f + add + "." + ext;
	}
}

static void extract_data() {
	string outdir = fname + " extracted";
	string cmd;
	cmd = "mkdir \"" + outdir + "\"";
	cout << cmd << endl;
	// system(cmd.c_str());

	set<string> unique_ext = {};
	for (const auto& elem : menu.options) {
		if (!elem.selected)
			continue;

		string ext = elem.ext;

		if (elem.type == "Video") {
			ext = "mp4";
		} else if (elem.type == "Audio") {
			ext = "mp3";
		}

		string elem_fname;
		if (elem.fname.size() != 0) {
			elem_fname = elem.fname;
		} else if (elem.name.size() != 0) {
			elem_fname = replace_ext(fname, ext, " " + elem.name);
		} else if (elem.lang.size() != 0) {
			elem_fname = replace_ext(fname, ext, " " + elem.lang);
		} else if (!unique_ext.count(ext)) {
			elem_fname = replace_ext(fname, ext);
			unique_ext.insert(ext);
		} else {
			elem_fname = replace_ext(fname, ext, " " + elem.stream);
		}

		cmd = "ffmpeg -i \"" + fname + "\" -map " + elem.stream +
			  " -c copy \"" + outdir + "/" + elem_fname + "\"";
		cout << cmd << endl;
		// system(cmd.c_str());
	}
}

int main(int argc, char** argv) {
	bool op_i;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (argv[i] == "-i" || argv[i] == "--interactive") {
				op_i = true;
				continue;
			}
			throw std::invalid_argument("unknown option: " + string(argv[i]));
		} else {
			fname = argv[i];
		}
	}
	if (fname.size() == 0) {
		throw std::invalid_argument("specify container file name!");
	}

	if (access(fname.c_str(), F_OK)) {
		throw std::runtime_error("file does not exist!");
	}

	string cmd = "ffprobe " + string(fname) + " 2>&1";
	string cmd_out = exec(cmd.c_str());
	int regex_time = extract_info(cmd_out);

	sort(menu.options.begin(), menu.options.end(),
		 [](const container_element& x, const container_element& y) {
			 return x.type < y.type;
		 });

	if (op_i) {
		run_interactive();
	}

	extract_data();

    cout << "[TIME]: ffprobe regex time = " << regex_time << " μs" << endl;

	return 0;
}
